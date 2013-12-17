/*
* Copyright (C) 2013- yeyouqun@163.com
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
#include <string>
#include <set>
#include <algorithm>
#include <assert.h>
#include <list>

#ifdef _WIN32
	#include <functional>
	#include <hash_map>
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
#if defined (_UNIX)
    #include <sys/types.h>
    #include <netinet/in.h>
    #include <inttypes.h>
#endif
#endif

#include "mytypes.h"
#include "buffer.h"
#include "simple_socket.h"
#include "passive_socket.h"
#include "platform.h"
#include "rw.h"
#include "rollsum.h"
#include "digest.h"
#include "xdeltalib.h"
#include "tinythread.h"
#include "reconstruct.h"
#include "multiround.h"
#include "xdelta_server.h"
#include "stream.h"
#include "inplace.h"

namespace xdelta {

#define SAFE_MARGIN 256
tcp_hasher_stream::tcp_hasher_stream (CActiveSocket & client
				, file_operator & fop
				, xdelta_observer & observer
				, bool inplace) 
				: client_ (client)
				, header_buff_ (STACK_BUFF_LEN)
				, stream_buff_ (XDELTA_BUFFER_LEN + SAFE_MARGIN) // used to buffer the block data and receive.
				, observer_ (observer) 
				, fop_ (fop)
				, blk_len_ (0)
				, error_no_ (0)
				, multiround_ (false)
				, inplace_ (inplace)
{
}

void tcp_hasher_stream::on_error (const std::string & errmsg, const int errorno)
{
	error_no_ = errorno;
	observer_.on_error (errmsg, errorno);
}

const uint16_t MULTI_ROUND_FLAG = 0xffff;
const uint16_t SINGLE_ROUND_FLAG = 0x5a5a;
const uint16_t INPLACE_FLAG = 0xa5a5;

#define BEGINE_HEADER(buff)			\
	buff.reset();					\
	buff.wr_ptr (BLOCK_HEAD_LEN);	\
	uchar_t * ptr = buff.wr_ptr()

#define END_HEADER(buff,type)	do {				\
		block_header header;							\
		header.blk_type = type;							\
		header.blk_len = (uint32_t)((buff).wr_ptr () - ptr);	\
		char_buffer<uchar_t> tmp (buff.begin (), STACK_BUFF_LEN); \
		tmp << header; \
	}while (0)

void tcp_hasher_stream::start_hash_stream (const std::string & fname, const int32_t blk_len)
{
	BEGINE_HEADER (header_buff_);

	header_buff_ << fname << blk_len;
	if (inplace_)
		header_buff_ << INPLACE_FLAG;
	else if (multiround_)
		header_buff_ << MULTI_ROUND_FLAG;
	else
		header_buff_ << SINGLE_ROUND_FLAG;

	END_HEADER (header_buff_, BT_HASH_BEGIN_BLOCK);
	_streamize (header_buff_);
	observer_.start_hash_stream (fname, blk_len);
	filename_ = fname;
	return;
}

void tcp_hasher_stream::add_block (const uint32_t fhash, const slow_hash & shash)
{
	BEGINE_HEADER (header_buff_);
	header_buff_ << fhash << shash;
	END_HEADER (header_buff_, BT_HASH_BLOCK);
	_streamize (header_buff_);
}

bool tcp_hasher_stream::end_first_round (const uchar_t file_hash[DIGEST_BYTES])
{
	return false; 
}

void tcp_hasher_stream::next_round (const int32_t blk_len)
{ 
	assert (false);
}

void tcp_hasher_stream::end_one_round ()
{ 
	assert (false);
}

void tcp_hasher_stream::_end_one_stage (uint16_t endtype, const uchar_t file_hash[DIGEST_BYTES])
{
	BEGINE_HEADER (header_buff_);

	header_buff_.copy (file_hash, DIGEST_BYTES);
	header_buff_ << error_no_;
	
	END_HEADER (header_buff_, endtype);
	_streamize (header_buff_, true);
}

void tcp_hasher_stream::end_hash_stream (const uchar_t file_hash[DIGEST_BYTES], const uint64_t filsize)
{
	_end_one_stage (BT_HASH_END_BLOCK, file_hash);
	if (!is_no_file_error (error_no_))
		return;

	_reconstruct_it ();
	observer_.end_hash_stream (filsize);
}

void tcp_hasher_stream::_streamize (char_buffer<uchar_t> & buff, bool now)
{
	buffer_or_send (client_, stream_buff_, buff, observer_, now);
}

void tcp_hasher_stream::_receive_construct_data (reconstructor & reconst)
{
	reconst.start_hash_stream (filename_, blk_len_);
	try {
		while (true) {
			stream_buff_.reset ();
			block_header header = read_block_header (client_, observer_);
			if (header.blk_type == BT_EQUAL_BLOCK) {
				assert (header.blk_len != 0);
				read_block (stream_buff_, client_, header.blk_len, observer_);
				target_pos tpos;
				int32_t blk_len;
				uint64_t s_offset;
				stream_buff_ >> tpos.index >> tpos.t_offset >> blk_len >> s_offset;
				reconst.add_block (tpos, blk_len, s_offset);
				observer_.on_equal_block (blk_len, s_offset);
			}
			else if (header.blk_type == BT_DIFF_BLOCK) {
				assert (header.blk_len != 0);
				read_block (stream_buff_, client_, header.blk_len, observer_);
				uint64_t s_offset;
				stream_buff_ >> s_offset;
				uint32_t blk_len = (uint32_t)(header.blk_len - sizeof (s_offset));
				reconst.add_block(stream_buff_.rd_ptr(), blk_len, s_offset);
				observer_.on_diff_block(blk_len);
			}
			else if (header.blk_type == BT_XDELTA_END_BLOCK) {
				assert (header.blk_len != 0);
				read_block (stream_buff_, client_, header.blk_len, observer_);
				uint64_t filsize;
				stream_buff_ >> filsize;
				reconst.end_hash_stream (filsize);
				break;
			}
			else {
				std::string errmsg = fmt_string ("Error data format.");
				THROW_XDELTA_EXCEPTION_NO_ERRNO (errmsg);
			}
		}
	}
	catch (...) {
		reconst.end_hash_stream (0);
		throw;
	}
}

void tcp_hasher_stream::_reconstruct_it ()
{
	block_header header = read_block_header (client_, observer_);
	if (header.blk_type != BT_XDELTA_BEGIN_BLOCK) {
		std::string errmsg = fmt_string ("Error data format(not BT_XDELTA_BEGIN_BLOCK).");
		THROW_XDELTA_EXCEPTION_NO_ERRNO (errmsg);
	}
    
    std::auto_ptr<reconstructor> reconst;
	if (inplace_)
	    reconst.reset (new in_place_reconstructor (fop_));
	else
	    reconst.reset (new reconstructor (fop_));

	_receive_construct_data (*reconst);
}

////////////////////////////////////////////////////////////////////////////
// multiround

multiround_tcp_stream::multiround_tcp_stream (CActiveSocket & client
					, file_operator & fop
					, xdelta_observer & observer)
		: tcp_hasher_stream (client, fop, observer)
		, _end_first_round (false)
		, reconst_ (fop)
{
	multiround_ = true;
}

void multiround_tcp_stream::set_holes (std::set<hole_t> * holeset)
{
	reconst_.set_holes (holeset);
}

void multiround_tcp_stream::start_hash_stream (const std::string & fname, const int32_t blk_len)
{
	reconst_.start_hash_stream (fname, blk_len);
	tcp_hasher_stream::start_hash_stream (fname, blk_len);
}

void multiround_tcp_stream::end_hash_stream (const uchar_t file_hash[DIGEST_BYTES], const uint64_t filsize)
{
	_end_one_stage (BT_HASH_END_BLOCK, file_hash);
	if (!is_no_file_error (error_no_))
		return;

	while (true) {
		stream_buff_.reset ();
		block_header header = read_block_header (client_, observer_);
		if (header.blk_type == BT_DIFF_BLOCK) {
			assert (header.blk_len != 0);
			read_block (stream_buff_, client_, header.blk_len, observer_);
			uint64_t s_offset;
			stream_buff_ >> s_offset;
			uint32_t blk_len = (uint32_t)(header.blk_len - sizeof (uint64_t));
			reconst_.add_block(stream_buff_.rd_ptr(), blk_len, s_offset);
			observer_.on_diff_block(blk_len);
		}
		else if (header.blk_type == BT_XDELTA_END_BLOCK) {
			assert (header.blk_len != 0);
			read_block (stream_buff_, client_, header.blk_len, observer_);
			uint64_t filsize;
			stream_buff_ >> filsize;
			reconst_.end_hash_stream (filsize);
			break;
		}
		else {
			std::string errmsg = fmt_string ("Error data format.");
			reconst_.end_hash_stream (0);
			THROW_XDELTA_EXCEPTION_NO_ERRNO (errmsg);
		}
	}

	observer_.end_hash_stream (filsize);
}

bool multiround_tcp_stream::_receive_equal_node ()
{
	while (true) {
		header_buff_.reset ();
		block_header header = read_block_header (client_, observer_);

		if (!_end_first_round && header.blk_type == BT_END_FIRST_ROUND) {
			_end_first_round = true;
			if (header.blk_len != 0) {
				std::string errmsg = fmt_string ("Error data format(not BT_XDELTA_BEGIN_BLOCK).");
				THROW_XDELTA_EXCEPTION_NO_ERRNO (errmsg);
			}
			else
				return false;
		}

		if (header.blk_type == BT_EQUAL_BLOCK) {
			assert (header.blk_len != 0);
			read_block (header_buff_, client_, header.blk_len, observer_);
			target_pos tpos;
			int32_t blk_len;
			uint64_t s_offset;
			header_buff_ >> tpos.index >> tpos.t_offset >> blk_len >> s_offset;
			reconst_.add_block (tpos, blk_len, s_offset);
			observer_.on_equal_block(blk_len, s_offset);
		}
		else if (header.blk_type == BT_END_ONE_ROUND) {
			if (header.blk_len != 0) {
				std::string errmsg = fmt_string ("Error data format(not BT_END_ONE_ROUND).");
				THROW_XDELTA_EXCEPTION_NO_ERRNO (errmsg);
			}
			break;
		}
		else {
			std::string errmsg = fmt_string ("Error data format.");
			reconst_.end_hash_stream (0);
			THROW_XDELTA_EXCEPTION_NO_ERRNO (errmsg);
		}
	}

	_end_first_round = true;
	return true;
}

bool multiround_tcp_stream::end_first_round (const uchar_t file_hash[DIGEST_BYTES])
{
	_end_one_stage (BT_END_FIRST_ROUND, file_hash);
	block_header header = read_block_header (client_, observer_);
	if (header.blk_type != BT_XDELTA_BEGIN_BLOCK) {
		std::string errmsg = fmt_string ("Error data format(not BT_XDELTA_BEGIN_BLOCK).");
		THROW_XDELTA_EXCEPTION_NO_ERRNO (errmsg);
	}

	bool neednround = _receive_equal_node ();
	observer_.end_first_round ();
	if (!neednround)
		reconst_.stop_reconstruct ();

	return neednround;
}

void multiround_tcp_stream::next_round (const int32_t blk_len)
{
	BEGINE_HEADER (header_buff_);
	header_buff_ << blk_len;
	END_HEADER (header_buff_, BT_BEGIN_ONE_ROUND);
	_streamize (header_buff_);
	observer_.next_round (blk_len);
}

void multiround_tcp_stream::end_one_round ()
{
	BEGINE_HEADER (header_buff_);
	END_HEADER (header_buff_, BT_END_ONE_ROUND);
	_streamize (header_buff_, true);
	_receive_equal_node ();
	observer_.end_one_round ();
}

} // namespace xdelta

