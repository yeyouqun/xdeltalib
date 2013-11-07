// authour:yeyouqun@163.com
#ifdef _WIN32
	#include <windows.h>
	#include <functional>
	#include <hash_map>
	#include <errno.h>
#else
	#include <unistd.h>
	#include <memory>
	#include <ext/functional>
    #if !defined (__CXX_11__)
    	#include <ext/hash_map>
    #else
    	#include <unordered_map>
    #endif
    #include <memory.h>
    #include <stdio.h>
#endif

#include <algorithm>
#include <set>
#include <string>
#include <list>
#include <iterator>
#include <assert.h>

#include "mytypes.h"
#include "digest.h"
#include "rw.h"
#include "rollsum.h"
#include "buffer.h"
#include "xdeltalib.h"
#include "buffer.h"
#include "reconstruct.h"
#include "inplace.h"

namespace xdelta {

void in_place_reconstructor::start_hash_stream (const std::string & fname
								, const int32_t blk_len)
{
	reader_ = foperator_.create_reader (fname);
	if (reader_->exist_file ()) {
		reader_->open_file ();
	}
	else {
		foperator_.release (reader_);
		reader_ = 0;
	}

	writer_ = foperator_.create_writer (fname);
	if (writer_ == 0) {
		std::string errmsg = fmt_string ("Can't create file %s.", fname.c_str ());
		THROW_XDELTA_EXCEPTION_NO_ERRNO (errmsg);
	}

	writer_->open_file ();
	fname_ = fname;
	ftmpname_.clear ();
}

void in_place_reconstructor::add_block (const target_pos & tpos
							, const uint32_t blk_len
							, const uint64_t s_offset)
{
	assert (tpos.t_offset == 0);
	if (tpos.index * blk_len == s_offset)
		return;

	reconstructor::add_block (tpos, blk_len, s_offset);
}

void in_place_reconstructor::add_block (const uchar_t * data
								, const uint32_t blk_len, const uint64_t s_offset)
{
	uint64_t pos = writer_->seek_file (s_offset, FILE_BEGIN);
	if (s_offset != pos) {
		std::string errmsg = fmt_string ("Can't seek file %s(%s)."
										, fname_.c_str ()
										, error_msg ().c_str ());
		THROW_XDELTA_EXCEPTION (errmsg);
	}
	reconstructor::add_block (data, blk_len, s_offset);
}

void in_place_reconstructor::end_hash_stream (const uint64_t filsize)
{
	writer_->set_file_size (filsize);
	reconstructor::end_hash_stream (filsize);
}

void in_place_reconstructor::on_error (const std::string & errmsg, const int errorno)
{
	reconstructor::on_error (errmsg, errorno);
}


void in_place_stream::start_hash_stream (const std::string & fname, const int32_t blk_len)
{
	_clear ();
	output_.start_hash_stream (fname, blk_len);
}

void in_place_stream::end_hash_stream (const uint64_t filsize)
{
	_calc_send ();
	output_.end_hash_stream (filsize);
	_clear ();
}

void in_place_stream::on_error (const std::string & errmsg, const int errorno)
{
	output_.on_error (errmsg, errorno);
}

void in_place_stream::add_block (const target_pos & tpos
							, const uint32_t blk_len
							, const uint64_t s_offset)
{
	assert (tpos.t_offset == 0);
	equal_node * p = new equal_node ();
	p->blength = blk_len;
	p->s_offset = s_offset;
	p->visited = FALSE;
	p->stacked = FALSE;
	p->tpos = tpos;
	equal_nodes_.push_back (p);
}

void in_place_stream::add_block (const uchar_t * data, const uint32_t blk_len, const uint64_t s_offset)
{
	diff_node dn;
	dn.blength = blk_len;
	dn.s_offset = s_offset;
	diff_nodes_.push_back (dn);
}

void in_place_stream::_clear ()
{
	std::for_each (equal_nodes_.begin (), equal_nodes_.end (), delete_obj<equal_node>);
	equal_nodes_.clear ();
	diff_nodes_.clear ();
}

uint32_t seek_and_read (file_reader & reader, uint64_t offset, uint32_t length, uchar_t * data)
{
	uint64_t s_offset = reader.seek_file (offset, FILE_BEGIN);
	if (offset != s_offset) {
		std::string errmsg = fmt_string ("Can't seek file %s(%s)."
			, reader.get_fname ().c_str (), error_msg ().c_str ());
		THROW_XDELTA_EXCEPTION (errmsg);
	}

	uint32_t ret = reader.read_file (data, length);
	if (ret < length) {
		std::string errmsg = fmt_string ("Bytes read from %s(%s) is less than demand."
			, reader.get_fname ().c_str (), error_msg ().c_str ());
		THROW_XDELTA_EXCEPTION (errmsg);
	}

	return ret;
}

void in_place_stream::_calc_send ()
{
	//
	// refer to <<In-Place Rsync: File Synchronization for Mobile and Wireless Devices>>
	// to get the algorithm details.
	//
	typedef std::list<equal_node*>::iterator it_t;
	std::list<equal_node*> result;

	std::set<equal_node *>	enode_set;
	std::copy (equal_nodes_.begin (), equal_nodes_.end ()
				, std::inserter (enode_set, enode_set.end ()));

	for (it_t pos = equal_nodes_.begin (); pos != equal_nodes_.end (); ++pos)
		_handle_node (enode_set, *pos, result);

	for (it_t pos = result.begin (); pos != result.end (); ++pos) {
		equal_node * node = *pos;
		output_.add_block (node->tpos, node->blength, node->s_offset);
	}

	reader_.open_file ();
	for (std::list<diff_node>::iterator pos = diff_nodes_.begin ()
								; pos != diff_nodes_.end (); ++pos) {
		const diff_node & node = *pos;
		seek_and_read (reader_, node.s_offset, node.blength, buff_.begin ());
		output_.add_block (buff_.begin (), node.blength, node.s_offset);
	}
	reader_.close_file ();
	return;
}

void in_place_stream::_handle_node (std::set<equal_node *> & enode_set
									, equal_node * node
									, std::list<equal_node*> & result)
{
	if (node->stacked == TRUE) { // cyclic condition, convert it to adding bytes to target.
		diff_node dn;
		dn.blength = node->blength;
		dn.s_offset = node->s_offset;
		diff_nodes_.push_back (dn);
		enode_set.erase (node);
		node->deleted = TRUE;
		return;
	}

	if (node->visited == TRUE || node->deleted == TRUE)
		return;

	uint64_t left_index = node->s_offset / node->blength, 
			right_index = (node->s_offset - 1 + node->blength) / node->blength;

	equal_node enode;
	enode = *node;
	enode.tpos.index = left_index;

	typedef std::set<equal_node*>::iterator it_t;
	// to forge a node, only t_index member will be used.
	it_t pos = enode_set.find (&enode);
	// to check if this equal node is overlap with one and/or its 
	// directly following block on target.
	if (pos != enode_set.end () && *pos != node) {
		node->stacked = TRUE;
		_handle_node (enode_set, *pos, result);
		node->stacked = FALSE;
	}

	if (node->deleted == FALSE) {
		enode.tpos.index = right_index;
		pos = enode_set.find (&enode);
		if (pos != enode_set.end () && *pos != node) {
			node->stacked = TRUE;
			_handle_node (enode_set, *pos, result);
			node->stacked = FALSE;
		}
	}
	// this node's all dependencies have been resolved.
	// so push the node to the back, and when return from this call,
	// blocks depend on this node will be pushed to the back just behind
	// its dependent block.
	if (node->deleted == FALSE) {
		result.push_back (node);
		node->visited = TRUE;
	}
	return;
}

} //namespace xdelta
