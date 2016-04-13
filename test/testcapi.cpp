#include <time.h>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <set>
#include <list>

#ifdef _WIN32
	#include <windows.h>
	#include <functional>
	#include <hash_map>
	#include <errno.h>
	#include <io.h>
	#include <direct.h>
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
#include "tinythread.h"
#include "buffer.h"
#include "platform.h"
#include "md4.h"
#include "platform.h"
#include "tinythread.h"
#include "rw.h"
#include "rollsum.h"
#include "xdeltalib.h"

#include "capi.h"

/**
 *			srchole = (0, source filesize);
 *			tgthole = (0, target filesize);
 *			blklen = origin_len;
 *
 *			tmpfile = create_a_temp_file ();
 *			for (blklen >= minimal len) { // 当执行单轮计算时，将 minimal len 设定为 blklen。
 *				void *inner_data = start_hash ();
 *				for (each hole in tgthole) {
 *					pipehandle = calc_hash (hole, blklen, inner_data);
 *					write_file_data_in_this_hole_to_pipe (pipehandle);
 *				}
 *				hash_result = get_hash_result_and_free_inner_data (inner_data);
 *
 *				inner_data = start_xdelta (hash_result);
 *				free_hash_result (hash_result);
 *	 
 *				for (each hole in srchole) {
 *					pipehandle = run_xdelta (blklen, hole, inner_data);
 *					write_file_data_in_this_hole_to_pipe (pipehandle);
 *				}
 *				xdelta_result = get_xdelta_result_and_free_inner_data (inner_data);
 *
 *				for (item in xdelta_result) {
 *					if (item.type == DT_IDENT) {
 *						data = copy_from (tgtfile);
 *						seek (tmpfile, block.s_offset);
 *						write (tmpfile, data, block.len);
 *						divide_hole (tgthole, item.t_offset, item.blklen);
 *						divide_hole (srchole, item.s_offset, item.blklen);
 *					}
 *				}
 *				free_xdelta_result (xdelta_result);
 *
 *				blklen = next_block_len; // next_block_len < blklen
 *			}
 * 
 * 			for (item in xdelta_result) {
 *				if (item.type == DT_DIFF) {
 *					data = copy_from (srcfile);
 *					seek (tmpfile, block.s_offset);
 *					write (tmpfile, data, block.len);
 *				}
 *			}
 *			close (tmpfile);
 */

#ifdef _WIN32
	#define SEP '\\'
#else
	#define SEP '/'
#endif // _WIN32

using namespace xdelta;

unsigned write_pipe_handle (PIPE_HANDLE handle, uchar_t * buff, unsigned len)
{
#ifdef _WIN32
	xdelta::uint32_t dwWritten;
	while (true) {
		int result = ::WriteFile (handle, (char*)buff, len, (LPDWORD)(&dwWritten), 0);
		//TODO: check the dwWritten
		if (!result) {
			THROW_XDELTA_EXCEPTION ("Can't not write file");
		}
		
		if (dwWritten == len) {
			break;
		}
		else {
			len -= dwWritten;
			buff += dwWritten;
		}
	}

	return len;
#else
	if (handle == -1) {
		std::string errmsg ("File not open.");
		THROW_XDELTA_EXCEPTION_NO_ERRNO (errmsg);
	}
	return write (handle, buff, len);
#endif
}
#define BUFSIZE (1024*1024)

int handle_this_node (const fh_t * head, file_reader * preader, PIPE_HANDLE wr)
{
	char_buffer<uchar_t> databuf (BUFSIZE);
	
	unsigned b2r = (unsigned)head->len;
	while (b2r > 0) {
		int size = 0;
		unsigned readlen = b2r > BUFSIZE ? BUFSIZE : b2r;
		size = preader->read_file (databuf.begin (), readlen);
		if (size > 0)
			write_pipe_handle (wr, databuf.begin (), size);
		else if (size < 0)
			return -1;
		b2r -= size;
	}
	return 0;
}

int read_and_write (file_reader * preader, file_writer * pwriter, unsigned blklen)
{
	char_buffer<uchar_t> databuf (BUFSIZE);
	unsigned b2r = blklen;	
	while (b2r > 0) {
		unsigned readlen = b2r > BUFSIZE ? BUFSIZE : b2r;
		int size = preader->read_file (databuf.begin (), readlen);
		if (size < 0)
			return -1;
			
		pwriter->write_file (databuf.begin (), size);
		b2r -= size;
	}
	return 0;
}

struct sync_stat
{
	time_t start;
	time_t end;
	unsigned long long identical_nr;
	unsigned long long identical_bytes;
	unsigned long long different_nr;
	unsigned long long different_bytes;
};
#define SYNC_START() sync_stat s; s.start = time (0); s.end = 0; \
					s.identical_nr = s.identical_bytes = 0; \
					s.different_nr = 0; s.different_bytes = 0;

#define SYNC_IDENT(h) s.identical_nr++;s.identical_bytes += (h)->blklen
#define SYNC_DIFF(h) s.different_nr++;s.different_bytes += (h)->blklen
#define SYNC_END() s.end = time (0); \
					printf ("Time:%20llu\n", s.end - s.start); \
					printf ("Identical bytes:%20lld\tidentical nr:%20lld\n", s.identical_bytes, s.identical_nr); \
					printf ("Different:%20lld\tdifferent nr:%20lld\n", s.different_bytes, s.different_nr); \
					printf ("Radio(ident/(ident+diff)):%.6f\n", ((float)(s.identical_bytes))/(s.identical_bytes + s.different_bytes)); \


///////////////////////////////////////////////////////////////
void test_single_round (const std::string & srcfile, const std::string & tgtfile)
{
	if (!xdelta::exist_file (srcfile))
		return;

	if (!xdelta::exist_file (tgtfile)) {
		f_local_fwriter t(tgtfile);
		((file_writer *)&t)->open_file ();
	}

	f_local_freader tgt_reader (tgtfile);
	file_reader *ptgtreader = &tgt_reader;
	ptgtreader->open_file ();

	f_local_freader src_reader (srcfile);
	file_reader *psrcreader = &src_reader;
	psrcreader->open_file ();

	std::string tmptgt = get_tmp_fname (tgtfile);
	f_local_fwriter tmptgt_writer (tmptgt);
	file_writer * p_cstor_writer = &tmptgt_writer;
	p_cstor_writer->open_file ();


	// 如果目标文件大小为0时，用源文件的大小计算出来的块大小来分析文件，因为这样
	// 可以尽量减少计算的量。在计算 Xdelta 时，0 的块大小，可能导致最小的块计算大小，如400B，
	// 如果此时，源文件很大，如几百M，则可能导致内部会有很多循环，并且每个循环只输出 400B，
	// 如果此时输出更大的块，则计算量会小很多，如最大的块为 1M，则 1024*1024/400，差距达
	// 2000 多倍。
	unsigned blklen = 0;
	if (ptgtreader->get_file_size () == 0) // 对于这种情况，应该是直接复制文件即可，这里这么写只是为了
											// 实现代码上的一致性。
		blklen = xdelta_calc_block_len (psrcreader->get_file_size ());
	else
		blklen = xdelta_calc_block_len (ptgtreader->get_file_size ());
	
	fh_t head;
	head.pos = 0;
	head.len = ptgtreader->get_file_size ();
	xit_t * xdelta_result = 0;
	hit_t * hash_result = 0;
	
	SYNC_START();
	void *inner_data = xdelta_start_hash (blklen);
	if (inner_data == 0)
		return;

	if (head.len > 0) {
		PIPE_HANDLE wh = xdelta_run_hash (&head, inner_data);
		ptgtreader->seek_file (head.pos, FILE_BEGIN);
		if (handle_this_node (&head, ptgtreader, wh) != 0) {
			hit_t * hash_result = xdelta_get_hashes_free_inner (inner_data);
			xdelta_free_hashes (hash_result);
			goto over;
		}
	}
		
	hash_result = xdelta_get_hashes_free_inner (inner_data);
	inner_data = xdelta_start_xdelta (hash_result, blklen, 0, 0);
	xdelta_free_hashes (hash_result);

	head.pos = 0;
	head.len = psrcreader->get_file_size ();
		
	if (head.len > 0) {
		PIPE_HANDLE wh = xdelta_run_xdelta (&head, inner_data);
		psrcreader->seek_file (head.pos, FILE_BEGIN);

		if (handle_this_node (&head, psrcreader, wh) != 0) {
			xit_t * result = xdelta_get_xdeltas_free_inner (inner_data);
			xdelta_free_xdeltas (result);
			goto over;
		}
	}
		
	xdelta_result = xdelta_get_xdeltas_free_inner (inner_data);
	if (xdelta_result == 0)
		return;
		
	for (xit_t * p = xdelta_result; p != 0; p = p->next) {
		if (p->type == DT_IDENT) {
			SYNC_IDENT(p);
			ptgtreader->seek_file (get_target_offset(p), FILE_BEGIN);
			p_cstor_writer->seek_file (p->s_offset, FILE_BEGIN);
				
			if (read_and_write (ptgtreader, p_cstor_writer, p->blklen) != 0) {
				xdelta_free_xdeltas (xdelta_result);
				goto over;
			}
		}
		else { // (head->type == DT_DIFF)
			SYNC_DIFF(p);
			psrcreader->seek_file (p->s_offset, FILE_BEGIN);
			p_cstor_writer->seek_file (p->s_offset, FILE_BEGIN);
					
			if (read_and_write (psrcreader, p_cstor_writer, p->blklen) != 0) {
				xdelta_free_xdeltas (xdelta_result);
				goto over;
			}
		}
	}
	
	xdelta_free_xdeltas (xdelta_result);
	SYNC_END()
over:	
	ptgtreader->close_file ();
	psrcreader->close_file ();
	p_cstor_writer->close_file ();
	
	size_t pos = tmptgt.rfind (SEP);
	size_t pos2 = tgtfile.rfind (SEP);
	f_local_creator c(tmptgt.substr (0, pos));

	c.rename (tmptgt.substr (pos + 1), tgtfile.substr (pos2 + 1));
}
////////////////////////////////////////////////////////////////////
void test_multiple_round (const std::string & srcfile, const std::string & tgtfile)
{
	if (!xdelta::exist_file (srcfile))
		return;

	if (!xdelta::exist_file (tgtfile)) {
		f_local_fwriter t(tgtfile);
		((file_writer *)&t)->open_file ();
	}

	f_local_freader tgt_reader (tgtfile);
	file_reader *ptgtreader = &tgt_reader;
	ptgtreader->open_file ();

	f_local_freader src_reader (srcfile);
	file_reader *psrcreader = &src_reader;
	psrcreader->open_file ();

	std::string tmptgt = get_tmp_fname (tgtfile);
	f_local_fwriter tmptgt_writer (tmptgt);
	file_writer * p_cstor_writer = &tmptgt_writer;
	p_cstor_writer->open_file ();

	// 如果目标文件大小为0时，用源文件的大小计算出来的块大小来分析文件，因为这样
	// 可以尽量减少计算的量。在计算 Xdelta 时，0 的块大小，可能导致最小的块计算大小，如400B，
	// 如果此时，源文件很大，如几百M，则可能导致内部会有很多循环，并且每个循环只输出 400B，
	// 如果此时输出更大的块，则计算量会小很多，如最大的块为 1M，则 1024*1024/400，差距达
	// 2000 多倍。
	unsigned blklen = 0;
	if (ptgtreader->get_file_size () == 0) // 对于这种情况，应该是直接复制文件即可，这里这么写只是为了
											// 实现代码上的一致性。
		blklen = xdelta_calc_block_len (psrcreader->get_file_size ());
	else
		blklen = xdelta_calc_block_len (ptgtreader->get_file_size ());
		
	fh_t * srchole = (fh_t*)malloc (sizeof (fh_t));
	fh_t * tgthole = (fh_t*)malloc (sizeof (fh_t));
	
	srchole->pos = 0;
	srchole->len = psrcreader->get_file_size ();
	srchole->next = 0;
	
	tgthole->pos = 0;
	tgthole->len = ptgtreader->get_file_size ();
	tgthole->next = 0;
	xit_t * xdelta_result = 0;
	hit_t * hash_result = 0;

	unsigned minimal_blklen = XDELTA_BLOCK_SIZE;
	SYNC_START();
	for (;;) {
		void *inner_data = xdelta_start_hash (blklen);
		if (inner_data == 0)
			return;

		for (fh_t * head = tgthole; head != 0; head = head->next) {
			if (head->len > 0) {
				PIPE_HANDLE wh = xdelta_run_hash (head, inner_data);
				ptgtreader->seek_file (head->pos, FILE_BEGIN);
				if (handle_this_node (head, ptgtreader, wh) != 0) {
					hit_t * hash_result = xdelta_get_hashes_free_inner (inner_data);
					xdelta_free_hashes (hash_result);
					goto over;
				}
			}
		}
		
		hash_result = xdelta_get_hashes_free_inner (inner_data);
		inner_data = xdelta_start_xdelta (hash_result, blklen, 0, 0);
		xdelta_free_hashes (hash_result);
		
		for (fh_t * head = srchole; head != 0; head = head->next) {
			if (head->len > 0) {
				PIPE_HANDLE wh = xdelta_run_xdelta (head, inner_data);
				psrcreader->seek_file (head->pos, FILE_BEGIN);

				if (handle_this_node (head, psrcreader, wh) != 0) {
					xit_t * result = xdelta_get_xdeltas_free_inner (inner_data);
					xdelta_free_xdeltas (result);
					goto over;
				}
			}
		}
		
		xdelta_result = xdelta_get_xdeltas_free_inner (inner_data);
		if (xdelta_result == 0)
			return;

		for (xit_t * head = xdelta_result; head != 0; head = head->next) {
			if (head->type == DT_IDENT) {
				SYNC_IDENT(head);
				ptgtreader->seek_file (get_target_offset(head), FILE_BEGIN);
				p_cstor_writer->seek_file (head->s_offset, FILE_BEGIN);
				
				if (read_and_write (ptgtreader, p_cstor_writer, head->blklen) != 0) {
					xdelta_free_xdeltas (xdelta_result);
					goto over;
				}
			}
		}
		
		blklen /= 2; // 减少一半再执行一轮，直到最小块大小。
		if (blklen >= minimal_blklen) {
			for (xit_t * head = xdelta_result; head != 0; head = head->next) {
				if (head->type == DT_IDENT) {
					xdelta_divide_hole (&tgthole, get_target_offset(head), head->blklen);
					xdelta_divide_hole (&srchole, head->s_offset, head->blklen);
				}
			}
			
			xdelta_free_xdeltas (xdelta_result);
			continue;
		}

		// 结束轮。
		for (xit_t * head = xdelta_result; head != 0; head = head->next) {
			if (head->type == DT_DIFF) {
				SYNC_DIFF(head);
				psrcreader->seek_file (head->s_offset, FILE_BEGIN);
				p_cstor_writer->seek_file (head->s_offset, FILE_BEGIN);
					
				if (read_and_write (psrcreader, p_cstor_writer, head->blklen) != 0) {
					xdelta_free_xdeltas (xdelta_result);
					goto over;
				}
			}
		}
			
		xdelta_free_xdeltas (xdelta_result);
		break; // actually goto over;
	}
	SYNC_END();
over:	
	xdelta_free_hole (srchole);
	xdelta_free_hole (tgthole);
	
	ptgtreader->close_file ();
	psrcreader->close_file ();
	p_cstor_writer->close_file ();
	
	size_t pos = tmptgt.rfind (SEP);
	size_t pos2 = tgtfile.rfind (SEP);
	f_local_creator c(tmptgt.substr (0, pos));

	c.rename (tmptgt.substr (pos + 1), tgtfile.substr (pos2 + 1));
}

void test_single_round_inplace (const std::string & srcfile, const std::string & tgtfile)
{
	if (!xdelta::exist_file (srcfile))
		return;

	if (!xdelta::exist_file (tgtfile))
		return;

	f_local_freader tgt_reader (tgtfile);
	file_reader *ptgtreader = &tgt_reader;
	ptgtreader->open_file ();

	f_local_freader src_reader (srcfile);
	file_reader *psrcreader = &src_reader;
	psrcreader->open_file ();

	f_local_fwriter tmptgt_writer (tgtfile);
	file_writer * p_cstor_writer = &tmptgt_writer;
	p_cstor_writer->open_file ();


	// 如果目标文件大小为0时，用源文件的大小计算出来的块大小来分析文件，因为这样
	// 可以尽量减少计算的量。在计算 Xdelta 时，0 的块大小，可能导致最小的块计算大小，如400B，
	// 如果此时，源文件很大，如几百M，则可能导致内部会有很多循环，并且每个循环只输出 400B，
	// 如果此时输出更大的块，则计算量会小很多，如最大的块为 1M，则 1024*1024/400，差距达
	// 2000 多倍。
	unsigned blklen = 0;
	if (ptgtreader->get_file_size () == 0) // 对于这种情况，应该是直接复制文件即可，这里这么写只是为了
											// 实现代码上的一致性。
		blklen = xdelta_calc_block_len (psrcreader->get_file_size ());
	else
		blklen = xdelta_calc_block_len (ptgtreader->get_file_size ());
	
	fh_t head;
	head.pos = 0;
	head.len = ptgtreader->get_file_size ();
	xit_t * xdelta_result = 0;
	hit_t * hash_result = 0;
	
	SYNC_START();
	void *inner_data = xdelta_start_hash (blklen);
	if (inner_data == 0)
		return;

	if (head.len > 0) {
		PIPE_HANDLE wh = xdelta_run_hash (&head, inner_data);
		ptgtreader->seek_file (head.pos, FILE_BEGIN);
		if (handle_this_node (&head, ptgtreader, wh) != 0) {
			hit_t * hash_result = xdelta_get_hashes_free_inner (inner_data);
			xdelta_free_hashes (hash_result);
			goto over;
		}
	}
		
	hash_result = xdelta_get_hashes_free_inner (inner_data);
	inner_data = xdelta_start_xdelta (hash_result, blklen, 0, 0);
	xdelta_free_hashes (hash_result);

	head.pos = 0;
	head.len = psrcreader->get_file_size ();
		
	if (head.len > 0) {
		PIPE_HANDLE wh = xdelta_run_xdelta (&head, inner_data);
		psrcreader->seek_file (head.pos, FILE_BEGIN);

		if (handle_this_node (&head, psrcreader, wh) != 0) {
			xit_t * result = xdelta_get_xdeltas_free_inner (inner_data);
			xdelta_free_xdeltas (result);
			goto over;
		}
	}
		
	xdelta_result = xdelta_get_xdeltas_free_inner (inner_data);
	if (xdelta_result == 0)
		return;

	xdelta_resolve_inplace (&xdelta_result);
		
	for (xit_t * p = xdelta_result; p != 0; p = p->next) {
		if (p->type == DT_IDENT) {
			SYNC_IDENT(p);
			ptgtreader->seek_file (get_target_offset(p), FILE_BEGIN);
			p_cstor_writer->seek_file (p->s_offset, FILE_BEGIN);
				
			if (read_and_write (ptgtreader, p_cstor_writer, p->blklen) != 0) {
				xdelta_free_xdeltas (xdelta_result);
				goto over;
			}
		}
		else { // (head->type == DT_DIFF)
			SYNC_DIFF(p);
			psrcreader->seek_file (p->s_offset, FILE_BEGIN);
			p_cstor_writer->seek_file (p->s_offset, FILE_BEGIN);
					
			if (read_and_write (psrcreader, p_cstor_writer, p->blklen) != 0) {
				xdelta_free_xdeltas (xdelta_result);
				goto over;
			}
		}
	}
	
	xdelta_free_xdeltas (xdelta_result);
	SYNC_END();
over:	
	p_cstor_writer->set_file_size (psrcreader->get_file_size ());
	ptgtreader->close_file ();
	psrcreader->close_file ();
	p_cstor_writer->close_file ();
}

int check_file_sum (const std::string & srcfile, const std::string & tgtfile)
{
	if (!xdelta::exist_file (srcfile))
		return 1;

	if (!xdelta::exist_file (tgtfile))
		return 1;

	unsigned char sum_s[DIGEST_BYTES];
	f_local_freader s (srcfile);
	((file_reader &)s).open_file ();
	get_file_digest (s, sum_s);

	unsigned char sum_t[DIGEST_BYTES];
	f_local_freader t (tgtfile);
	((file_reader &)t).open_file ();
	get_file_digest (t, sum_t);

	if (memcmp (sum_s, sum_t, DIGEST_BYTES) == 0)
		return 0;
	else
		return 1;
}

int main (int argn, char ** argc)
{
	if (argn != 4) {
		return -1;
	}

	std::string srcfile (argc[1]); // 目标文件。
	std::string tgtfile (argc[2]);
	
	if (strcmp (argc[3], "m") == 0)  { // 多轮
		test_multiple_round (srcfile, tgtfile);
		if (check_file_sum (srcfile, tgtfile))
			printf ("file %s is different with %s.\n", srcfile.c_str (), tgtfile.c_str ());
		else
			printf ("file %s is same with %s.\n", srcfile.c_str (), tgtfile.c_str ());
	}
	else if (strcmp (argc[3], "s") == 0) { // 单轮
		test_single_round (srcfile, tgtfile);
		if (check_file_sum (srcfile, tgtfile))
			printf ("file %s is different with %s.\n", srcfile.c_str (), tgtfile.c_str ());
		else
			printf ("file %s is same with %s.\n", srcfile.c_str (), tgtfile.c_str ());
	}
	else if (strcmp (argc[3], "i") == 0) { // 就地生成，隐含单轮。
		test_single_round_inplace (srcfile, tgtfile);
		if (check_file_sum (srcfile, tgtfile))
			printf ("file %s is different with %s.\n", srcfile.c_str (), tgtfile.c_str ());
		else
			printf ("file %s is same with %s.\n", srcfile.c_str (), tgtfile.c_str ());
	}
	else
		return 0;

    return 0;
}
