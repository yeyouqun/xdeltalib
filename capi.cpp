/*
* Copyright (C) 2016- yeyouqun@163.com
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, visit the http://fsf.org website.
*/

#ifdef _WIN32
	#include <windows.h>
	#include <functional>
	#define _SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS
	#include <hash_map>
		
	#include <errno.h>
	#include <io.h>
	#include <direct.h>
#else
	#include <unistd.h>
	#include <memory>
	#include <ext/functional>
    #include <memory.h>
    #include <stdio.h>
	#if !defined (__CXX_11__)
		#include <ext/hash_map>
	#else
		#include <ext/unordered_map>
		#define hash_map			std::unordered_map
		#define hash_set			std::unordered_set
	#endif
#endif

#include <algorithm>
#include <set>
#include <string>
#include <list>
#include <iterator>
#include <assert.h>

#include "mytypes.h"
#include "tinythread.h"
#include "buffer.h"
#include "platform.h"
#include "md4.h"
#include "rw.h"
#include "rollsum.h"
#include "xdeltalib.h"
#include "capi.h"

namespace xdelta {
struct equal_node
{
	uint64_t	s_offset;	///< 源文件中的偏移
	target_pos	tpos;		///< 目标文件中的位置信息
	void *	 	data;
	uint32_t	blength:29; ///< 块长度，绝不会超过 MAX_XDELTA_BLOCK_BYTES 或者 MULTIROUND_MAX_BLOCK_SIZE
	uint32_t	visited:1;  ///< 本对象所表示的块是否已经处理过。
	uint32_t	stacked:1;	///< 本对象所表示的块是否已经在处理栈中。
	uint32_t	deleted:1;	///< 本对象所表示的块是否已经删除（有循环依赖）。
};

/// \struct
/// 描述一个不同的数据对类型
struct diff_node
{
	uint64_t s_offset;	///< 源文件中的偏移，将存储在目标文件中相同的位置。
	uint32_t blength;	///< 块长度。
};

void resolve_inplace_identical_block (std::set<equal_node *> & enode_set
									, equal_node * node
									, std::list<equal_node*> & ident_blocks
									, std::list<diff_node> * diff_blocks = 0)
{
	if (node->stacked == TRUE) { // cyclic condition, convert it to adding bytes to target.
		if (diff_blocks) {
			diff_node dn;
			dn.blength = node->blength;
			dn.s_offset = node->s_offset;
			diff_blocks->push_back (dn);
		}
		node->deleted = TRUE;
		return;
	}

	if (node->visited == TRUE)
		return;

	node->stacked = TRUE;
	//
	// 如果两个块索引是相同的，就说明这个块没有经过移动。
	// 这里的查找逻辑是这样的：
	// enode_set 已经按照其所在的目标文件的块索引经过排序了（set 的特性）：
	// 现在某个目标块在可以移动前，需要以 s_offset 为目标位置查找，是否有某个块在这个块影响之下，
	// 因此要将这个块先处理。如果覆盖的块，有一边是自己，则不需要处理这一边。
	//
	uint64_t left_index = node->s_offset / node->blength, 
			right_index = (node->s_offset - 1 + node->blength) / node->blength;

	equal_node enode;
	enode = *node;
	enode.tpos.index = (uint32_t)left_index;

	typedef std::set<equal_node*>::iterator it_t;
	// to forge a node, only t_index member will be used.
	it_t pos = enode_set.find (&enode);
	//
	// to check if this equal node is overlap with one and/or its 
	// directly following block on target.先处理左边
	//
	if (pos != enode_set.end () && *pos != node)
		xdelta::resolve_inplace_identical_block (enode_set, *pos, ident_blocks, diff_blocks);

	//
	// 再处理右边。
	//
	enode.tpos.index = (uint32_t)right_index;
	pos = enode_set.find (&enode);
	if (pos != enode_set.end () && *pos != node)
		resolve_inplace_identical_block (enode_set, *pos, ident_blocks, diff_blocks);

	// this node's all dependencies have been resolved.
	// so push the node to the back, and when return from this call,
	// blocks depend on this node will be pushed to the back just behind
	// its dependent block.
	if (node->deleted == FALSE)
		ident_blocks.push_back (node);
	
	node->stacked = FALSE;
	node->visited = TRUE;
	return;
}

static void create_pipe (PIPE_HANDLE * rd, PIPE_HANDLE * wr)
{
#ifdef _WIN32
	BOOL success = CreatePipe (rd, wr, NULL, 0);
#else
	int pipefds[2];
	int failed = pipe (pipefds);
	int success = !failed;
	
	*rd = pipefds[0];
	*wr = pipefds[1];
#endif
	if (!success) {
		std::string msg = fmt_string ("Create pipe error:%s", error_msg ().c_str ());
		THROW_XDELTA_EXCEPTION (msg);
	}
}

typedef struct inner_hash_xdelta_result_type
{
	thread * pthread;
	union {
		struct {
			hit_t *	 hhead, *htail;
		}hash;
		struct {
			xit_t *	 xhead, *xtail;
		}xdelta;
	}head;
	
	#define xhead head.xdelta.xhead
	#define hhead head.hash.hhead

	// 这个两成员是在多轮中用来保存上轮中最后一个元素的。
	#define htail head.hash.htail
	#define xtail head.xdelta.xtail
	
	PIPE_HANDLE rd;
	PIPE_HANDLE wr;
	hole_t	 	hole;
	uint32_t	blklen;
	hash_table	table;			// 在计算差异时使用的哈希表。
	
	diff_func_t diffcb;	// 在计算差异数据时的回调函数。
	void * cbpriv;		// 回调函数的数据。

	inner_hash_xdelta_result_type () :
		pthread (0),
		rd (INVALID_HANDLE_VALUE),
		wr (INVALID_HANDLE_VALUE),
		blklen(-1),
		diffcb(0),
		cbpriv (0)
		{
			xhead = 0;
			xtail = 0;
		}
}ihx_t;

class pipe_reader : public file_reader {
	virtual int read_file (uchar_t * data, const uint32_t len)
	{
		return local_read (f_handle_, data, len);
	}
	virtual uint64_t seek_file (const uint64_t offset, const int whence)
	{
		return offset;
	}
private:
	PIPE_HANDLE f_handle_;
public:
	pipe_reader (PIPE_HANDLE f_handle) : f_handle_ (f_handle) {}
	~pipe_reader() {}
};

class pipe_hasher_stream : public hasher_stream
{
	ihx_t * pihx_;
	virtual void add_block (const uint32_t fhash, const slow_hash & shash)
	{
		if (pihx_->hhead == 0) {
			pihx_->hhead = new hit_t;
			pihx_->htail = pihx_->hhead;
		}
		else {
			pihx_->htail->next = new hit_t;
			pihx_->htail = pihx_->htail->next;
		}
		
		pihx_->htail->fast_hash = fhash;
		memcpy (pihx_->htail->slow_hash, shash.hash, DIGEST_BYTES); // 16 Bytes
		pihx_->htail->t_offset = shash.tpos.t_offset;
		pihx_->htail->t_index =  shash.tpos.index;
		pihx_->htail->next = 0;
	}
	
public:
	pipe_hasher_stream (ihx_t * pihx) : pihx_ (pihx) {}
	~pipe_hasher_stream () {}
};

					
static void inner_calc_hash (void *data)
{
	ihx_t * pihx = (ihx_t *)data;
	pipe_hasher_stream pipehasher (pihx);
	pipe_reader pipereader (pihx->rd);
	
	read_and_hash (pipereader, pipehasher, pihx->hole.length, pihx->blklen, pihx->hole.offset, 0);
}

static void clear_hash_xdelta_result (ihx_t * pihx)
{
	if (pihx == 0)
		return;

	if (pihx->pthread != 0) {
		pihx->pthread->join ();
		delete pihx->pthread;
		pihx->pthread = 0;
	}
	
	if (pihx->rd != INVALID_HANDLE_VALUE) {
		CloseHandle (pihx->rd);
		pihx->rd = INVALID_HANDLE_VALUE;
	}
	
	if (pihx->wr != INVALID_HANDLE_VALUE) {
		CloseHandle (pihx->wr);
		pihx->wr = INVALID_HANDLE_VALUE;
	}
}

class pipe_xdelta_stream : public xdelta_stream
{
	ihx_t * pihx_;
	void add_block (uint16_t type, uint64_t t_pos, uint64_t s_pos, uint32_t blklen, uint32_t t_index)
	{
		if (pihx_->xhead == 0) {
			pihx_->xhead = new xit_t;
			pihx_->xtail = pihx_->xhead;
		}
		else {
			pihx_->xtail->next = new xit_t;
			pihx_->xtail = pihx_->xtail->next;
		}
		
		pihx_->xtail->next = 0;

		pihx_->xtail->type = type;
		pihx_->xtail->s_offset = s_pos;
		pihx_->xtail->t_offset = t_pos;
		pihx_->xtail->index = t_index;
		pihx_->xtail->blklen = blklen;
	}
	
	virtual void add_block (const target_pos & tpos
							, const uint32_t blk_len
							, const uint64_t s_offset)
	{
		if (blk_len != pihx_->blklen) {
			BUG ("Block length not match!");
		}
		
		add_block (DT_IDENT, tpos.t_offset, s_offset, blk_len, tpos.index);
	}
	virtual void add_block (const uchar_t * data
							, const uint32_t blk_len
							, const uint64_t s_offset)
	{
		add_block (DT_DIFF, 0, s_offset, blk_len, -1);
		if (pihx_->diffcb != 0)
			pihx_->diffcb((char *)data, blk_len, s_offset, pihx_->cbpriv);
		return;
	}
public:	
	pipe_xdelta_stream (ihx_t * pihx) : pihx_ (pihx) {}
	~pipe_xdelta_stream () {}
};

static void inner_xdelta (void *data)
{
	ihx_t * pihx = (ihx_t *)data;
	pipe_xdelta_stream pipexdelta (pihx);
	pipe_reader pipereader (pihx->rd);
	
	std::set<hole_t> hs;
	hs.insert (pihx->hole);
	
	read_and_delta (pipereader, pipexdelta, pihx->table, hs, pihx->blklen, false);
}

} // xdelta

using namespace xdelta;

void * xdelta_start_hash (unsigned blklen)
{
	if (blklen > MAX_XDELTA_BLOCK_BYTES || XDELTA_BLOCK_SIZE > blklen) {
		errno = 22;
		return 0;
	}
	
	ihx_t * pihx = new ihx_t;
	pihx->blklen = blklen;
	return (void*)pihx;
}

PIPE_HANDLE xdelta_run_hash (fh_t * phole, void * inner_data)
{
	ihx_t * pihx = (ihx_t *)(inner_data);
	clear_hash_xdelta_result (pihx);

	PIPE_HANDLE wr = INVALID_HANDLE_VALUE;

	try {
		create_pipe (&pihx->rd, &pihx->wr);
		pihx->hole.offset = phole->pos;
		pihx->hole.length = phole->len;
		wr = pihx->wr;

		pihx->pthread = new thread (inner_calc_hash, (void*)pihx);
	}
	catch (xdelta_exception &e) {
		clear_hash_xdelta_result (pihx);
		errno = e.get_errno ();
		return INVALID_HANDLE_VALUE;
	}
	return wr;
}

hit_t * xdelta_get_hashes_free_inner (void * inner_data)
{
	ihx_t * pihx = (ihx_t *)inner_data;
	if (pihx == 0)
		return 0;
		
	clear_hash_xdelta_result (pihx);
		
	hit_t * head = pihx->hhead;
	delete pihx;
	
	return head;
}

/****************************************** Xdelta *********************************/

void * xdelta_start_xdelta(hit_t * head, unsigned blklen
						, diff_func_t diffcb
						, void * cbpriv)
{
	//Todo: ..
	if (blklen > MAX_XDELTA_BLOCK_BYTES || XDELTA_BLOCK_SIZE > blklen) {
		errno = 22;
		return 0;
	}
	
	ihx_t * pihx = new ihx_t;
	pihx->blklen = blklen;
	
	for (;head != 0;head = head->next) {
		slow_hash sh;
		memcpy (sh.hash, head->slow_hash, DIGEST_BYTES);
		sh.tpos.t_offset = head->t_offset;
		sh.tpos.index = head->t_index;
		pihx->table.add_block (head->fast_hash, sh);
	}

	pihx->diffcb = diffcb;
	pihx->cbpriv = cbpriv;
	return (void*)pihx;
}
	
PIPE_HANDLE xdelta_run_xdelta (fh_t * srchole, void * inner_data)
{
	ihx_t * pihx = (ihx_t *)(inner_data);
	clear_hash_xdelta_result (pihx);

	PIPE_HANDLE wr = INVALID_HANDLE_VALUE;

	try {
		create_pipe (&pihx->rd, &pihx->wr);
		wr = pihx->wr;
		pihx->hole.offset = srchole->pos;
		pihx->hole.length = srchole->len;

		pihx->pthread = new thread (inner_xdelta, (void*)pihx);
	}
	catch (xdelta_exception &e) {
		clear_hash_xdelta_result (pihx);
		errno = e.get_errno ();
		return INVALID_HANDLE_VALUE;
	}
	return wr;
}

xit_t * xdelta_get_xdeltas_free_inner (void * inner_data)
{
	// Todo:
	ihx_t * pihx = (ihx_t *)inner_data;
	if (pihx == 0)
		return 0;
		
	clear_hash_xdelta_result (pihx);
	
	pihx->table.clear ();
		
	xit_t * head = pihx->xhead;
	delete pihx;
	
	return head;
}

/****************************************** multiround *********************************/

void xdelta_divide_hole (fh_t ** head, unsigned long long pos, unsigned len)
{
	fh_t * prev = 0;
	fh_t * tmphead = *head;

	for (; tmphead != 0;) {
		if (tmphead->pos <= pos && (tmphead->pos + tmphead->len) >= (pos + len)) {
			// split to one or more hole, like this
			// |--------------------------------------|
			// |---------| added block |--------------|
			fh_t * newhole = 0;
			newhole = (fh_t *)malloc (sizeof (fh_t));
			newhole->pos = pos + len;
			newhole->len = (unsigned)(tmphead->pos + tmphead->len - pos - len);
			
			newhole->next = tmphead->next; // 先将两个串起来。
			tmphead->next = newhole;
			tmphead->len = (unsigned)(pos - tmphead->pos);

			if (tmphead->len == 0) { // 如果为0，说明边界重合。
				if (prev == 0) {
					*head = tmphead->next;  // 换新头。tmphead 就是 head;
				}
				else {
					prev->next = tmphead->next;
				}
				free (tmphead);
			}
			
			if (newhole->len == 0) {
				tmphead->next = newhole->next;
				free (newhole);
			}
			
			break;
		}
		prev = tmphead;
		tmphead = tmphead->next;
	}
	
	return;
}

void xdelta_free_hole (fh_t * head)
{
	if (head == 0)
		return;
		
	for (fh_t * node = head; node != 0; ) {
		fh_t * t = node->next;
		free (node);
		node = t;
	}
	return;
}

void xdelta_resolve_inplace (xit_t ** head)
{
	if (*head == 0)
		return;
		
	std::set<equal_node *> enode_set;
	std::list<equal_node*> ident_blocks, result_ident_blocks;
	typedef std::list<equal_node*>::iterator it_t;

	xit_t * diffhead = 0, * diffprev = 0; // 差异数据链表
	for (xit_t * node = *head; node != 0; node = node->next) {
		if (node->type == DT_IDENT) {
			equal_node * p = new equal_node ();
			p->blength = node->blklen;
			p->s_offset = node->s_offset;
			p->visited = FALSE;
			p->stacked = FALSE;
			p->deleted = FALSE;
			p->tpos.t_offset = node->t_offset;
			p->tpos.index = node->index;
			p->data = (void*)node;
			ident_blocks.push_back (p);
		}
		else {
			if (diffhead == 0)
				diffhead = node;

			if (diffprev)
				diffprev->next = node;
			diffprev = node;
		}
	}

	if (diffprev)
		diffprev->next = 0;

	std::copy (ident_blocks.begin (), ident_blocks.end ()
				, std::inserter (enode_set, enode_set.end ()));
					
	for (it_t pos = ident_blocks.begin (); pos != ident_blocks.end (); ++pos)
		xdelta::resolve_inplace_identical_block (enode_set, *pos, result_ident_blocks);
		
	for (it_t pos = ident_blocks.begin (); pos != ident_blocks.end (); ++pos) {
		 // 已经存在 diff_blocks 中，即将这些处理过块放在链表头。
		 // 这些块没有顺序。
		if ((*pos)->deleted == TRUE) {
			xit_t * p = (xit_t *)((*pos)->data);
			p->type = DT_DIFF;
			p->next = diffhead;
			diffhead = p;
		}
	}

	//
	// 这里是已经经过排的可以在目标文件中直接通过移动块即可以实文件重构的。
	// 表头就是最先需要处理的块，依次排序下去。因此需要从后面处理起，处理到表头
	// 时，刚好是最先需要处理的相同的块。（这些块必须有严格的顺序）
	//
	typedef std::list<equal_node*>::reverse_iterator rit_t;
	for (rit_t pos = result_ident_blocks.rbegin (); pos != result_ident_blocks.rend (); ++pos) {
		xit_t * p = (xit_t *)((*pos)->data);
		p->next = diffhead;
		diffhead = p;
	}
	
	*head = diffhead;
	std::for_each (ident_blocks.begin (), ident_blocks.end (), delete_obj<equal_node>);
	return;
}

unsigned xdelta_calc_block_len (unsigned long long filesize)
{
	return get_xdelta_block_size (filesize);
}

/**
 * 释放哈希对象。
 * @head	哈希对象列表头。
 */
void xdelta_free_hashes (hit_t * head)
{
	if (head == 0)
		return;
		
	for (; head != 0;) {
		hit_t * tmp = head->next;
		delete head;
		head = tmp;
	}
}

void xdelta_free_xdeltas (xit_t * head)
{
	if (head == 0)
		return;
		
	for (; head != 0;) {
		xit_t * tmp = head->next;
		delete head;
		head = tmp;
	}
}

