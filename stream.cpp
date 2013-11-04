// author:yeyouqun@163.com
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
#endif

#include "mytypes.h"
#include "Host.h"
#include "buffer.h"
#include "SimpleSocket.h"
#include "PassiveSocket.h"
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
				, data_buff_ (STACK_BUFF_LEN)
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

void tcp_hasher_stream::start_hash_stream (const std::string & fname, const int32_t blk_len)
{
	data_buff_.reset ();
	data_buff_ << fname << blk_len;
	if (inplace_)
		data_buff_ << INPLACE_FLAG;
	else if (multiround_)
		data_buff_ << MULTI_ROUND_FLAG;
	else
		data_buff_ << SINGLE_ROUND_FLAG;
	
	block_header header;
	header.blk_type = BT_HASH_BEGIN_BLOCK;
	header.blk_len = (uint32_t)(data_buff_.wr_ptr () - data_buff_.rd_ptr ());

	header_buff_.reset ();
	header_buff_ << header;

	_streamize ();
	observer_.start_hash_stream (fname, blk_len);
	filename_ = fname;
	return;
}

void tcp_hasher_stream::add_block (const uint32_t fhash, const slow_hash & shash)
{
	data_buff_.reset ();
	data_buff_ << fhash << shash;
	
	block_header header;
	header.blk_type = BT_HASH_BLOCK;
	header.blk_len = (uint32_t)(data_buff_.wr_ptr () - data_buff_.rd_ptr ());

	header_buff_.reset ();
	header_buff_ << header;
	_streamize ();
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
	data_buff_.reset ();
	data_buff_.copy (file_hash, DIGEST_BYTES);
	data_buff_ << error_no_;
	
	block_header header;
	header.blk_type = endtype;
	header.blk_len = data_buff_.data_bytes ();

	header_buff_.reset ();
	header_buff_ << header;
	_streamize ();
	send_block (client_, stream_buff_, observer_);
}

void tcp_hasher_stream::end_hash_stream (const uchar_t file_hash[DIGEST_BYTES], const uint64_t filsize)
{
	_end_one_stage (BT_HASH_END_BLOCK, file_hash);
	if (!is_no_file_error (error_no_))
		return;

	_reconstruct_it ();
	observer_.end_hash_stream (filsize);
}

void tcp_hasher_stream::_streamize ()
{
	buffer_or_send (client_, stream_buff_, header_buff_, data_buff_, observer_);
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
			}
			else if (header.blk_type == BT_DIFF_BLOCK) {
				assert (header.blk_len != 0);
				read_block (stream_buff_, client_, header.blk_len, observer_);
				uint64_t s_offset;
				stream_buff_ >> s_offset;
				reconst.add_block (stream_buff_.rd_ptr (), header.blk_len - sizeof (s_offset), s_offset);
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
	char_buffer<uchar_t> buff (XDELTA_BUFFER_LEN); // largest block won't eccess XDELTA_BUFFER_LEN;

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
			reconst_.add_block (stream_buff_.rd_ptr (), header.blk_len - sizeof (uint64_t), s_offset);
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
		data_buff_.reset ();
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
			read_block (data_buff_, client_, header.blk_len, observer_);
			target_pos tpos;
			int32_t blk_len;
			uint64_t s_offset;
			data_buff_ >> tpos.index >> tpos.t_offset >> blk_len >> s_offset;
			reconst_.add_block (tpos, blk_len, s_offset);
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
	return neednround;
}

void multiround_tcp_stream::next_round (const int32_t blk_len)
{
	data_buff_.reset ();
	data_buff_ << blk_len;
	
	block_header header;
	header.blk_type = BT_BEGIN_ONE_ROUND;
	header.blk_len = (uint32_t)(data_buff_.wr_ptr () - data_buff_.rd_ptr ());

	header_buff_.reset ();
	header_buff_ << header;
	_streamize ();
	observer_.next_round (blk_len);
}

void multiround_tcp_stream::end_one_round ()
{
	data_buff_.reset ();

	block_header header;
	header.blk_type = BT_END_ONE_ROUND;
	header.blk_len = (uint32_t)(data_buff_.wr_ptr () - data_buff_.rd_ptr ());

	header_buff_.reset ();
	header_buff_ << header;
	_streamize ();

	if (stream_buff_.data_bytes () > 0)
		send_block (client_, stream_buff_, observer_);

	_receive_equal_node ();
	observer_.end_one_round ();
}

} // namespace xdelt
