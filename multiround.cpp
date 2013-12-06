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
#include "tinythread.h"
#include "rw.h"
#include "rollsum.h"
#include "buffer.h"
#include "xdeltalib.h"
#include "platform.h"
#include "reconstruct.h"
#include "multiround.h"

namespace xdelta {

void split_hole (std::set<hole_t> & holeset, const hole_t & hole)
{
	typedef std::set<hole_t>::iterator it_t;
	hole_t findhole;
	findhole.offset = hole.offset;
	findhole.length = 0;

	it_t pos = holeset.find (findhole);
	if (pos != holeset.end ()) {
		const hole_t bighole = *pos;
		if (bighole.offset <= hole.offset && 
			(bighole.offset + bighole.length) >= (hole.offset + hole.length)) {
				// split to one or more hole, like this
				// |--------------------------------------|
				// |---------| added block |--------------|
				holeset.erase (pos);
				if (bighole.offset < hole.offset) {
					hole_t newhole;
					newhole.offset = bighole.offset;
					newhole.length = hole.offset - bighole.offset;
					holeset.insert (newhole);
				}

				uint64_t bigend = bighole.offset + bighole.length,
					     holeend = hole.offset + hole.length;
				if (bigend > holeend) {
					hole_t newhole;
					newhole.offset = hole.offset + hole.length;
					newhole.length = bigend - holeend;
					holeset.insert (newhole);
				}

				return;
		}
		++pos;
	}

	BUG("hole must be exists");
}

void multiround_reconstructor::add_block (const target_pos & tpos
								, const uint32_t blk_len
								, const uint64_t s_offset)
{
	hole_t hole;
	hole.length = blk_len;
	hole.offset = tpos.t_offset + tpos.index * blk_len;
	reconstructor::add_block (tpos, blk_len, s_offset);
	if (target_hole_)
		split_hole (*target_hole_, hole);
}

void multiround_reconstructor::add_block (const uchar_t * data
										, const uint32_t blk_len
										, const uint64_t filpos)
{
	//
	// this interface only called on the last round.
	//
	uint64_t pos = writer_->seek_file (filpos, FILE_BEGIN);
	if (filpos != pos) {
		std::string errmsg = fmt_string ("Can't seek file %s(%s)."
										, fname_.c_str ()
										, error_msg ().c_str ());
		THROW_XDELTA_EXCEPTION (errmsg);
	}
	reconstructor::add_block (data, blk_len, filpos);
}

/////////////////////////////////////////////////////////////////////
void multiround_hasher_stream::start_hash_stream (const std::string & fname
										, const int32_t blk_len)
{
	blk_len_ = blk_len;
	hashes_.clear ();

	if (reader_.exist_file ()) {
		hole_t hole;
		reader_.open_file ();
		hole.length = reader_.get_file_size ();
		hole.offset = 0;
		source_holes_.insert (hole);
	}

	output_->start_hash_stream (reader_.get_fname (), blk_len_);
}

void multiround_hasher_stream::add_block (const uint32_t fhash, const slow_hash & shash)
{
	hashes_.add_block (fhash, shash);
}

void multiround_hasher_stream::end_hash_stream (const uchar_t file_hash[DIGEST_BYTES], const uint64_t filsize)
{
	char_buffer<uchar_t> buff (XDELTA_BUFFER_LEN);
	typedef std::set<hole_t>::iterator it_t;

	reader_.open_file ();
	for (it_t begin = source_holes_.begin (); begin != source_holes_.end (); ++begin) {
		hole_t hole = *begin;
		while (hole.length != 0) {
			uint32_t bytes_to_read = (uint32_t)hole.length;
			bytes_to_read = bytes_to_read > XDELTA_BUFFER_LEN ? XDELTA_BUFFER_LEN : bytes_to_read;
			uint32_t bytes_read = seek_and_read (reader_
												, hole.offset
												, bytes_to_read
												, buff.begin ());
			output_->add_block (buff.begin (), bytes_read, hole.offset);
			hole.offset += bytes_read;
			hole.length -= bytes_read;
		}
	}
	output_->end_hash_stream (filsize);
	reader_.close_file ();
}

void read_and_delta (file_reader & fobj
					, xdelta_stream & h_stream
					, const hash_table & hashes
					, std::set<hole_t> & hole_set
					, const int blk_len
					, bool need_split_hole);

bool multiround_hasher_stream::end_first_round (const uchar_t file_hash[DIGEST_BYTES])
{
	uchar_t hash[DIGEST_BYTES];
	get_file_digest (reader_, hash);
	if (memcmp (hash, file_hash, DIGEST_BYTES) == 0)
		return false;

	read_and_delta (reader_, *output_, hashes_, source_holes_, blk_len_, true);
	output_->end_one_round ();
	hashes_.clear ();
	return true;
}

void multiround_hasher_stream::next_round (const int32_t blk_len)
{
	output_->next_round (blk_len);
	hashes_.clear ();
	blk_len_ = blk_len;
}

void multiround_hasher_stream::end_one_round ()
{
	read_and_delta (reader_, *output_, hashes_, source_holes_, blk_len_, true);
	output_->end_one_round ();
	hashes_.clear ();
}

void multiround_hasher_stream::on_error (const std::string & errmsg, const int errorno)
{
	output_->on_error (errmsg, errorno);
}

} //namespace xdelta

