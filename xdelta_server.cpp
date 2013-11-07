// author:yeyouqun@163.com
#include <string>
#include <set>
#include <algorithm>
#include <assert.h>

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

namespace xdelta {

template <typename fobj_type>
struct file_releaser
{
	file_operator * fop_;
	fobj_type * fobj_;
	file_releaser (file_operator * fop, fobj_type * fobj) : fop_ (fop), fobj_ (fobj) {}
	~file_releaser () { fop_->release (fobj_); }
};


struct hasher_strcut
{
	std::string			addr_;
	uint16_t			port_;
	file_operator *		fop_;
	xdelta_observer *	observer_;
	uint64_t			auto_multiround_filsize_;
	bool				inplace_;
};

static void xdelta_server_thread (void * data)
{
	//
	// receive hasher request and send data back.
	//
	CActiveSocket client;
	hasher_strcut * phs = (hasher_strcut*)data;
	try {
		init_active_socket (client, (uchar_t*)phs->addr_.c_str (), phs->port_);
		DEFINE_STACK_BUFFER (buff);

		while (true) {
			block_header header = read_block_header (client, *phs->observer_);
			if (header.blk_type != BT_CLIENT_FILE_BLOCK) {
				std::string errmsg = fmt_string ("Error data format(not BT_CLIENT_FILE_BLOCK).");
				THROW_XDELTA_EXCEPTION_NO_ERRNO (errmsg);
			}

			buff.reset ();
			assert (header.blk_len != 0);
			read_block (buff, client, header.blk_len, *phs->observer_);
			std::string filename;
			buff >> filename;

			//how to determine multround stream?
			file_releaser<file_reader> reader (phs->fop_, phs->fop_->create_reader (filename));

			if (phs->inplace_) {
				tcp_hasher_stream hasher (client, *phs->fop_, *phs->observer_, phs->inplace_);
				hash_table ().hash_it (*reader.fobj_, hasher);
			}
			else {
				uint64_t filsize = 0;
				if (reader.fobj_->exist_file ()) {
					reader.fobj_->open_file ();
					filsize = reader.fobj_->get_file_size ();
				}

				if (phs->auto_multiround_filsize_ == NO_MULTI_ROUND_FILSIZE ||
					filsize <= phs->auto_multiround_filsize_) {
					tcp_hasher_stream hasher (client, *phs->fop_, *phs->observer_);
					hash_table ().hash_it (*reader.fobj_, hasher);
				}
				else {
				    multiround_hash_table mht;
					hash_table & table = mht;
					multiround_tcp_stream hasher (client, *phs->fop_, *phs->observer_);
					table.hash_it (*reader.fobj_, hasher);
				}
			}
		}
	}
	catch (xdelta_exception &e) {
		phs->observer_->on_error (e.what (), e.get_errno ());
	}
	catch (...) {
		phs->observer_->on_error ("Unknow exception.", 0);
	}

	client.Close ();
}

// client ---> server (one link, used to transmit files to be hash and other parameters)
// client <---> server (one to multiple, used to transmit hash value and xdelta value)
//

void xdelta_server::_start_task (file_operator & foperator
								, xdelta_observer & observer
								, uint16_t port)
{
	init_passive_socket (server_, port);

	std::auto_ptr<CActiveSocket> client_auto;
	CActiveSocket * client = 0;
	DEFINE_STACK_BUFFER (buff);
	if ((client = server_.Accept()) != NULL) {
		client_auto.reset (client);
		std::vector<thread*> hasher_threads;
		block_header header = read_block_header (*client, observer);
		if (header.blk_type != BT_CLIENT_BLOCK) {
			std::string errmsg = fmt_string ("Error data format(not BT_CLIENT_BLOCK).");
			THROW_XDELTA_EXCEPTION_NO_ERRNO (errmsg);
		}
		
		int nthread = header.blk_len / sizeof (uint16_t);
		// get client ports that receive hasher data.
		for (int i = 0; i < nthread; ++i) {
			buff.reset ();
			read_block (buff, (*client), sizeof (uint16_t), observer);
			// create hasher and sender.
			std::auto_ptr<hasher_strcut> data (new hasher_strcut);
			data->addr_ = (char_t*)client->GetClientAddr ();
			if (data->addr_.empty ())
				continue;
			buff >> data->port_;
			data->fop_ = &foperator;
			data->observer_ = &observer;
			data->auto_multiround_filsize_ = auto_multiround_filsize_;
			data->inplace_ = inplace_;
			thread * thrd = new thread (xdelta_server_thread, (void *)data.release ());
			hasher_threads.push_back (thrd);
		}

		wait_to_exit (hasher_threads);
	}
	server_.Close ();
}

void xdelta_server::run (file_operator & foperator
					, xdelta_observer & observer
					, uint16_t port)
{
	try {
		_start_task (foperator, observer, port);
	}
	catch (...) {
		server_.Close ();
		throw;
	}
}

void xdelta_server::set_inplace ()
{
	if (auto_multiround_filsize_ != NO_MULTI_ROUND_FILSIZE) {
		std::string errmsg = fmt_string ("Multi-round already set.");
		THROW_XDELTA_EXCEPTION_NO_ERRNO (errmsg);
	}

	inplace_ = true;
}

void xdelta_server::set_multiround_size (uint64_t multi_round_size)
{
	if (inplace_) {
		std::string errmsg = fmt_string ("In-place already set.");
		THROW_XDELTA_EXCEPTION_NO_ERRNO (errmsg);
	}

	// less than this size will cause no difference with single round.
	if (MULTIROUND_MAX_BLOCK_SIZE > auto_multiround_filsize_)
		auto_multiround_filsize_ = NO_MULTI_ROUND_FILSIZE;
	else
		auto_multiround_filsize_ = multi_round_size;
}

void init_passive_socket (CPassiveSocket & server, uint16_t port)
{
	if (!server.Initialize()) {
		std::string errmsg = fmt_string ("Initializing server socket failed.");
		THROW_XDELTA_EXCEPTION (errmsg);
	}

	if (!server.Listen((const uchar_t *)XDELTA_ADDR, port)) {
		std::string errmsg = fmt_string ("Listen on port %d failed.", (uint32_t)port);
		THROW_XDELTA_EXCEPTION (errmsg);
	}
}

void init_active_socket (CActiveSocket & active, const uchar_t * addr, uint16_t port)
{
	if (!active.Initialize ()) {
		std::string errmsg = fmt_string ("Socket initialized failed on port %d."
										, (uint32_t)port);
		THROW_XDELTA_EXCEPTION (errmsg);
	}

	if (!active.Open (addr, port)) {
		std::string errmsg = fmt_string ("Socket is broken or closed on port %d."
										, (uint32_t)port);
		THROW_XDELTA_EXCEPTION (errmsg);
	}
}

void buffer_or_send (CSimpleSocket & socket
							, char_buffer<uchar_t> & stream_buff
							, char_buffer<uchar_t> & header_buff
							, char_buffer<uchar_t> & data_buff
							, xdelta_observer & observer)
{
	uint32_t hbytes = header_buff.data_bytes ();
	uint32_t dbytes = data_buff.data_bytes ();

	if ((hbytes + dbytes) > stream_buff.available ())
		send_block (socket, stream_buff, observer);

	// size of header_buff_ is far more less than stream_buff.
	stream_buff.copy (header_buff.rd_ptr (), hbytes);
	header_buff.reset ();

	if (data_buff.data_bytes () < stream_buff.available ()) {
		stream_buff.copy (data_buff.rd_ptr (), dbytes);
		data_buff.reset ();
	}
	else {
		send_block (socket, stream_buff, observer);
		send_block (socket, data_buff, observer);
	}
}

} //namespace xdelta
