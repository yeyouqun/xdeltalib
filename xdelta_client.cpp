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
#include "stream.h"
#include "xdelta_server.h"
#include "xdelta_client.h"
#include "inplace.h"

namespace xdelta {

struct client_struct
{
	CPassiveSocket *	client_;
	xdelta_observer *	observer_;
	client_slot_t *		task_slot_;
};

struct task_struct
{
	file_reader *	reader_;
	deletor *		deletor_;
};

struct client_slot_t
{
	client_slot_t () : waiting_ (false) {}
	mutex						lock_;
	condition_variable			event_;
	std::list<task_struct*>		tasks_;
	std::vector<thread *>		threads_;
	bool						waiting_;// task creator is waiting to finish
	deletor *					deletor_;
	
	void add_task (task_struct * pto)
	{
		lock_guard<mutex> lg (lock_);
		tasks_.push_back (pto);
	}
	task_struct * pop_task ()
	{
		lock_guard<mutex> lg (lock_);
		if (tasks_.empty ())
			return (task_struct*)0;
		
		task_struct * ptask = *tasks_.begin ();
		tasks_.pop_front ();
		return ptask;
	}
	bool is_waiting () {
		lock_guard<mutex> lg (lock_);
		return waiting_;
	}

	uint32_t task_count () const
	{
		lock_guard<mutex> lg (const_cast<mutex&> (lock_));
		return (uint32_t)tasks_.size ();
	}

	void wait () {
		lock_.lock ();
		event_.wait (lock_);
		lock_.unlock ();
	}

	void notify_all () {
		event_.notify_all ();
	}

	~client_slot_t ()
	{
		std::for_each (threads_.begin (), threads_.end (), delete_obj<thread>);
		for (std::list<task_struct*>::iterator begin = tasks_.begin ();
			begin != tasks_.end (); ++begin) {
			deletor_->release ((*begin)->reader_);
			delete *begin;
		}
		tasks_.clear ();
	}
};


class tcp_xdelta_stream : public xdelta_stream 
{
	virtual void start_hash_stream (const std::string & fname, const int32_t blk_len);
	virtual void add_block (const target_pos & tpos
							, const uint32_t blk_len
							, const uint64_t s_offset);
	virtual void add_block (const uchar_t * data, const uint32_t blk_len, const uint64_t offset);
	virtual void next_round (const int32_t blk_len) {}
	virtual void end_one_round ();
	virtual void set_holes (std::set<hole_t> * holeset) {}
	virtual void end_hash_stream (const uint64_t filsize);
	virtual void on_error (const std::string & errmsg, const int errorno);
private:
	CSimpleSocket & client_;
	xdelta_observer & observer_;
	char_buffer<uchar_t> header_buff_;
	char_buffer<uchar_t> data_buff_;
	char_buffer<uchar_t> stream_buff_; // used to buffer header and data.
public:
	tcp_xdelta_stream (CSimpleSocket & client, xdelta_observer & observer)
		: client_ (client)
		, observer_ (observer)
		, header_buff_ (STACK_BUFF_LEN)
		, data_buff_ (STACK_BUFF_LEN)
		, stream_buff_ (2*1024*1024)
	{}
};

void tcp_xdelta_stream::start_hash_stream (const std::string & fname, const int32_t blk_len)
{
	block_header header;
	header.blk_type = BT_XDELTA_BEGIN_BLOCK;
	header.blk_len = 0;
	data_buff_.reset ();
	header_buff_ << header;
	buffer_or_send (client_, stream_buff_, header_buff_, data_buff_, observer_);
}

void tcp_xdelta_stream::add_block (const target_pos & tpos
						, const uint32_t blk_len
						, const uint64_t s_offset)
{
	block_header header;
	header.blk_type = BT_EQUAL_BLOCK;
	header.blk_len = 0;
	header_buff_.reset ();

	data_buff_.reset ();
	data_buff_ << tpos.index << tpos.t_offset << blk_len << s_offset;
	header.blk_len = data_buff_.data_bytes ();
	header_buff_ << header;

	buffer_or_send (client_, stream_buff_, header_buff_, data_buff_, observer_);
}

void tcp_xdelta_stream::add_block (const uchar_t * data, const uint32_t blk_len, const uint64_t offset)
{
	block_header header;
	header.blk_type = BT_DIFF_BLOCK;
	header.blk_len = 0;
	header_buff_.reset ();

	char_buffer<uchar_t> buff (const_cast<uchar_t*> (data), blk_len);
	buff.wr_ptr (blk_len);

	header.blk_len = blk_len + sizeof (offset);

	header_buff_ << header;
	header_buff_ << offset;

	buffer_or_send (client_, stream_buff_, header_buff_, buff, observer_);
}

void tcp_xdelta_stream::end_one_round ()
{
	header_buff_.reset ();
	data_buff_.reset ();

	block_header header;
	header.blk_type = BT_END_ONE_ROUND;
	header.blk_len = 0;
	header_buff_ << header;
	buffer_or_send (client_, stream_buff_, header_buff_, data_buff_, observer_);
	send_block (client_, stream_buff_, observer_);
	observer_.end_one_round ();
}

void tcp_xdelta_stream::end_hash_stream (const uint64_t filsize)
{
	header_buff_.reset ();
	data_buff_.reset ();

	block_header header;
	header.blk_type = BT_XDELTA_END_BLOCK;
	header.blk_len = 0;

	data_buff_ << filsize;
	header.blk_len = data_buff_.data_bytes ();
	header_buff_ << header;

	buffer_or_send (client_, stream_buff_, header_buff_, data_buff_, observer_);
	send_block (client_, stream_buff_, observer_);

	stream_buff_.reset ();
	observer_.end_hash_stream (filsize);
}

void tcp_xdelta_stream::on_error (const std::string & errmsg, const int errorno)
{
	observer_.on_error (errmsg, errorno);
}

static int receive_hash_table (CSimpleSocket & client, hasher_stream & stream, xdelta_observer & observer)
{
	int error_no = 0;
	DEFINE_STACK_BUFFER (data_buff);
	while (true) {
		data_buff.reset ();
		block_header header = read_block_header (client, observer);
		if (header.blk_type == BT_HASH_BLOCK) {
			assert (header.blk_len != 0);
			read_block (data_buff, client, header.blk_len, observer);
			uint32_t fhash;
			slow_hash shash;
			data_buff >> fhash >> shash;
			stream.add_block (fhash, shash);
		}
		else if (header.blk_type == BT_END_FIRST_ROUND) {
			assert (header.blk_len != 0);
			read_block (data_buff, client, header.blk_len, observer);
			uchar_t file_hash[DIGEST_BYTES];
			memcpy (file_hash, data_buff.rd_ptr (), DIGEST_BYTES);
			data_buff.rd_ptr (DIGEST_BYTES);

			bool cont_multiround = stream.end_first_round (file_hash);
			if (!cont_multiround) {
				char_buffer<uchar_t> data_buff (100);
				char_buffer<uchar_t> header_buff (100);
				header.blk_type = BT_END_FIRST_ROUND;
				header.blk_len = 0;
				header_buff << header;
				streamize_data (client, header_buff, data_buff, observer);
			}
		}
		else if (header.blk_type == BT_BEGIN_ONE_ROUND) {
			assert (header.blk_len != 0);
			read_block (data_buff, client, header.blk_len, observer);
			int32_t blk_len;
			data_buff >> blk_len;
			stream.next_round (blk_len);
		}
		else if (header.blk_type == BT_END_ONE_ROUND) {
			stream.end_one_round ();
		}
		else if (header.blk_type == BT_HASH_END_BLOCK) {
			read_block (data_buff, client, header.blk_len, observer);
			uchar_t file_hash[DIGEST_BYTES];
			memcpy (file_hash, data_buff.rd_ptr (), DIGEST_BYTES);
			data_buff.rd_ptr (DIGEST_BYTES);
			data_buff >> error_no;
			break;
		}
		else {
			std::string errmsg = fmt_string ("Error data format.");
			THROW_XDELTA_EXCEPTION_NO_ERRNO (errmsg);
		}
	}

	return error_no;
}

static void xdelta_multi_round (CSimpleSocket & client
								, xdelta_observer & observer
								, file_reader & reader
								, int32_t blk_len)
{
	tcp_xdelta_stream xstream (client, observer);
	multiround_hasher_stream hstream (reader, &xstream, 0);
	hasher_stream & hasher = hstream;

	hasher.start_hash_stream (reader.get_fname (), blk_len);
	try {
		receive_hash_table (client, hasher, observer);
	}
	catch (...) {
		uchar_t filhash[DIGEST_BYTES];
		memset (filhash, 0, DIGEST_BYTES);
		hasher.end_hash_stream (filhash, 0);
		throw;
	}
	uchar_t filhash[DIGEST_BYTES];
	memset (filhash, 0, DIGEST_BYTES);
	hasher.end_hash_stream (filhash, reader.get_file_size ());
}

static int receive_hash_table (CSimpleSocket & client, hash_table & table, xdelta_observer & observer)
{
	int error_no = 0;
	DEFINE_STACK_BUFFER (data_buff);
	table.clear ();

	while (true) {
		data_buff.reset ();
		block_header header = read_block_header (client, observer);
		if (header.blk_type == BT_HASH_BLOCK) {
			assert (header.blk_len != 0);
			read_block (data_buff, client, header.blk_len, observer);
			uint32_t fhash;
			slow_hash shash;
			data_buff >> fhash >> shash;
			table.add_block (fhash, shash);
		}
		else if (header.blk_type == BT_HASH_END_BLOCK) {
			assert (header.blk_len != 0);
			read_block (data_buff, client, header.blk_len, observer);
			uchar_t file_hash[DIGEST_BYTES];
			memcpy (file_hash, data_buff.rd_ptr (), DIGEST_BYTES);
			data_buff.rd_ptr (DIGEST_BYTES);
			data_buff >> error_no;
			break;
		}
		else {
			std::string errmsg = fmt_string ("Error data format.");
			THROW_XDELTA_EXCEPTION_NO_ERRNO (errmsg);
		}
	}

	return error_no;
}

static void xdelta_single_round (CSimpleSocket & client
								, xdelta_observer & observer
								, file_reader & reader
								, int32_t blk_len)
{
	hash_table table;
	int error_no = receive_hash_table (client, table, observer);
	if (!is_no_file_error (error_no))
		return;

	xdelta_hash_table htable (table, reader, blk_len);
	tcp_xdelta_stream stream (client, observer);
	htable.xdelta_it (stream);
	return;
}

static void xdelta_inplace (CSimpleSocket & client
								, xdelta_observer & observer
								, file_reader & reader
								, int32_t blk_len)
{
	hash_table table;
	int error_no = receive_hash_table (client, table, observer);
	if (!is_no_file_error (error_no))
		return;

	xdelta_hash_table htable (table, reader, blk_len);
	tcp_xdelta_stream xstream (client, observer);
	in_place_stream inpstream (xstream, reader);
	htable.xdelta_it (inpstream);
	return;
}

static void xdelta_client_task (void * data)
{
	std::auto_ptr<client_struct> autopcs ((client_struct*)data);
	client_struct * pcs = autopcs.get ();

	std::auto_ptr<CPassiveSocket> autosocket (pcs->client_);
	CPassiveSocket & client = *autosocket.get ();

	client_slot_t * slot = pcs->task_slot_;
	std::auto_ptr<CActiveSocket> server;

	while (true) {
		server.reset (client.Accept (5));
		if (server.get () == 0 &&
			client.GetSocketError () == CSimpleSocket::SocketTimedout &&
				(!slot->is_waiting () || slot->task_count () != 0))
			continue; // continue to wait.
		break;
	}

	if (server.get ()) {
		CActiveSocket & peer = *server.get ();
		DEFINE_STACK_BUFFER (header_buff);
		DEFINE_STACK_BUFFER (data_buff);
		while (true) {
			std::auto_ptr<task_struct> pto (slot->pop_task ());
			if (pto.get () == 0) {
				if (!slot->is_waiting ()) {
					slot->wait ();
					continue;
				}

				break;
			}

			file_reader & reader = *pto->reader_;
			try {
				header_buff.reset ();
				data_buff.reset ();

				data_buff << reader.get_fname ();

				block_header header;
				header.blk_type = BT_CLIENT_FILE_BLOCK;
				header.blk_len = data_buff.data_bytes ();
				header_buff << header;
				streamize_data (peer, header_buff, data_buff, *pcs->observer_);

				header = read_block_header (peer, *pcs->observer_);
				if (header.blk_type != BT_HASH_BEGIN_BLOCK) {
					std::string errmsg = fmt_string ("Error data format(not BT_HASH_BEGIN_BLOCK).");
					THROW_XDELTA_EXCEPTION_NO_ERRNO (errmsg);
				}

				assert (header.blk_len != 0);
				data_buff.reset ();
				read_block (data_buff, peer, header.blk_len, *pcs->observer_);
				std::string filename;
				int32_t blk_len;
				uint16_t xdelta_type;
				data_buff >> filename >> blk_len >> xdelta_type;

				if (xdelta_type == INPLACE_FLAG)
					xdelta_inplace (peer, *pcs->observer_, reader, blk_len);
				else if (xdelta_type == MULTI_ROUND_FLAG)
					xdelta_multi_round (peer, *pcs->observer_, reader, blk_len);
				else if (xdelta_type == SINGLE_ROUND_FLAG)
					xdelta_single_round (peer, *pcs->observer_, reader, blk_len);
				else {
					std::string errmsg = fmt_string ("Wrong data format %d of BT_HASH_BEGIN_BLOCK."
													, (int32_t)xdelta_type);
					THROW_XDELTA_EXCEPTION_NO_ERRNO (errmsg);
				}
			}
			catch (xdelta_exception &e) {
				std::string errmsg = fmt_string ("xdelta_exception when handle '%s', the error is '%s'.\n"
											, reader.get_fname ().c_str (), e.what ());
				pcs->observer_->on_error (errmsg, e.get_errno ());
			}
			catch (...) {
				std::string errmsg = fmt_string ("Unknown exception when handle '%s'.\n"
											, reader.get_fname ().c_str ());
				pcs->observer_->on_error (errmsg, 0);
			}

			if (pto->deletor_)
				pto->deletor_->release (pto->reader_);
		}
		peer.Close ();
	}

	client.Close ();
}

xdelta_client::xdelta_client (bool compress, uint32_t thread_nr) : client_ (compress)
												, thread_nr_ (thread_nr)
												, compress_ (compress)
{
	task_slot_ = new client_slot_t;
}

xdelta_client::~xdelta_client ()
{
	delete task_slot_;
}

void xdelta_client::run (file_operator & foperator
					, xdelta_observer & observer
					, const uchar_t * paddr
					, uint16_t port)
{
	init_active_socket (client_, paddr, port);

	DEFINE_STACK_BUFFER (header_buff);
	DEFINE_STACK_BUFFER (data_buff);

	if (thread_nr_ == 0)
		thread_nr_ = thread::hardware_concurrency () * 2;

	block_header header;
	header.blk_type = BT_CLIENT_BLOCK;

	uint32_t thread_nr = thread_nr_;
	while (thread_nr > 0) {
		std::auto_ptr<CPassiveSocket> client (new CPassiveSocket (compress_));
		init_passive_socket (*client.get (), 0);
		uint16_t data_port = client->GetServerPort ();
		data_buff << data_port;

		client_struct * pcs = new client_struct;
		pcs->client_ = client.release ();
		pcs->observer_ = &observer;
		pcs->task_slot_ = task_slot_;

		lock_guard<mutex> lg (task_slot_->lock_);
		thread * thrd = new thread (xdelta_client_task, (void*)pcs);
		//
		// we can't call like this:
		// task_slot_->threads_.push_back (new thread (xdelta_client_task, (void*)pcs))
		//
		task_slot_->threads_.push_back (thrd);
		--thread_nr;
	}

	header.blk_len = data_buff.data_bytes ();
	header_buff << header;
	streamize_data (client_, header_buff, data_buff, observer);
}

void xdelta_client::add_task (file_reader * reader, deletor * pdel)
{
	task_struct * task = new task_struct;
	task->reader_ = reader;
	task->deletor_ = pdel;
	task_slot_->add_task (task);
	task_slot_->notify_all ();
};

void xdelta_client::wait ()
{
	task_slot_->lock_.lock ();
	task_slot_->waiting_ = true;
	task_slot_->lock_.unlock ();
	task_slot_->event_.notify_all ();

	wait_to_exit (task_slot_->threads_);
}

} //namespace xdelta

