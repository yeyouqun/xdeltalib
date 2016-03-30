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

#define SYNC_END() do { s.end = time (0); \
					s.identical_nr /= 2; \
					s.identical_bytes /= 2;\
					s.different_nr /= 2;\
					s.different_bytes /= 2;\
					printf ("Time:%20llu\n", s.end - s.start); \
					printf ("Identical bytes:%20lld\tidentical nr:%20lld\n", s.identical_bytes, s.identical_nr); \
					printf ("Different:%20lld\tdifferent nr:%20lld\n", s.different_bytes, s.different_nr); \
					printf ("Radio(ident/(ident+diff)):%.6f\n", ((float)(s.identical_bytes))/(s.identical_bytes + s.different_bytes)); \
				}while(0)


///////////////////////////////////////////////////////////////
void test_single_round (const std::string & srcfile, const std::string & tgtfile, unsigned blklen, sync_stat & s)
{
	if (!xdelta::exist_file (srcfile))
		return;

	if (!xdelta::exist_file (tgtfile)) {
		return;
	}

	f_local_freader tgt_reader (tgtfile);
	file_reader *ptgtreader = &tgt_reader;
	ptgtreader->open_file ();

	f_local_freader src_reader (srcfile);
	file_reader *psrcreader = &src_reader;
	psrcreader->open_file ();

	// 如果目标文件大小为0时，用源文件的大小计算出来的块大小来分析文件，因为这样
	// 可以尽量减少计算的量。在计算 Xdelta 时，0 的块大小，可能导致最小的块计算大小，如400B，
	// 如果此时，源文件很大，如几百M，则可能导致内部会有很多循环，并且每个循环只输出 400B，
	// 如果此时输出更大的块，则计算量会小很多，如最大的块为 1M，则 1024*1024/400，差距达
	// 2000 多倍。
	if (blklen == 0) {
		if (ptgtreader->get_file_size () == 0) // 对于这种情况，应该是直接复制文件即可，这里这么写只是为了
												// 实现代码上的一致性。
			blklen = xdelta_calc_block_len (psrcreader->get_file_size ());
		else
			blklen = xdelta_calc_block_len (ptgtreader->get_file_size ());
	}
	
	fh_t head;
	head.pos = 0;
	head.len = ptgtreader->get_file_size ();
	xit_t * xdelta_result = 0;
	hit_t * hash_result = 0;
	
	void *inner_data = xdelta_start_hash (blklen);
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
	inner_data = xdelta_start_xdelta (hash_result, blklen);
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
		
	for (xit_t * p = xdelta_result; p != 0; p = p->next) {
		if (p->type == DT_IDENT) {
			SYNC_IDENT(p);
		}
		else { // (head->type == DT_DIFF)
			SYNC_DIFF(p);
		}
	}
	
	xdelta_free_xdeltas (xdelta_result);
over:	
	ptgtreader->close_file ();
	psrcreader->close_file ();
	return;
}


int main (int argn, char ** argc)
{
	if (argn != 4 && argn != 3) {
		return -1;
	}

	std::string srcfile (argc[1]); // 目标文件。
	std::string tgtfile (argc[2]);
	unsigned int blklen = 0;
	if (argn == 4)
		blklen = atoi (argc[3]);
		
	SYNC_START();
	test_single_round (srcfile, tgtfile, blklen, s);
	test_single_round (tgtfile, srcfile, blklen, s);
	SYNC_END();
    return 0;
}
