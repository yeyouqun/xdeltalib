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
#ifndef __XDELTA_SERVER_H__
#define __XDELTA_SERVER_H__
/// @file
/// 声明网络服务端处理基类。
namespace xdelta {
/// \def XDELTA_DEFAULT_PORT
/// 默认服务器同步端口，用户可以指定其他端口。
#define XDELTA_DEFAULT_PORT 1366
/// \def XDELTA_ADDR
/// 本地服务地址
#define XDELTA_ADDR "127.0.0.1"
/// \def NO_MULTI_ROUND_FILSIZE
/// 默认多轮 Hash 的文件尺寸大小，一般不会有文件超过这个大小，因此指定这个参数时，所有文件都只执行单轮 Hash。
#define NO_MULTI_ROUND_FILSIZE ((uint64_t)(-1))

/// \class
/// \brief
/// 本类用于观察同步执行流程，使用者需要自己派生相应的观察者类，以实现自己的统计或者其他相应的操作。
class xdelta_observer
{
public:
	xdelta_observer () {}
	virtual ~xdelta_observer () {}
	/// \brief
	/// 指示开始处理文件的 Hash 流
	/// \param[in] fname	文件名，带相对路径
	/// \param[in] blk_len	处理文件的块长度
	/// \return 没有返回
	virtual void start_hash_stream (const std::string & fname, const int32_t blk_len) = 0;
	/// \brief
	/// 指示结束一个文件的 Hash 流的处理。
	/// \param[in] filsize		源文件的大小。
	/// \return 没有返回
	virtual void end_hash_stream (const uint64_t filesize) = 0;
	/// \brief
	/// 指示结束多轮 Hash 中第一轮，其相应结果相当于单轮 Hash 中的 end_hash_stream。
	/// \param[in] file_hash		整个文件的 MD4 Hash 值。
	/// \return 如果源文件中判断需要继续下一轮，则返回真，否则返回假。
	virtual void end_first_round () = 0;
	/// \brief
	/// 指示下一轮 Hash 流开始。
	/// \param[in] blk_len		下一轮 Hash 的块长度。
	/// \return 没有返回
	virtual void next_round (const int32_t blk_len) = 0;
	/// \brief
	/// 指示结束一轮 Hash，只在多轮 Hash 中调用
	/// \return 没有返回
	virtual void end_one_round () = 0;
	/// \brief
	/// 指示处理过程中的错误。
	/// \param[in] errmsg		错误信息。
	/// \param[in] errorno		错误码。
	/// \return 没有返回
	virtual void on_error (const std::string & errmsg, const int errorno) = 0;
	/// \brief
	/// 指示发送的字节数，可以用来进行数据统计。
	/// \param[in] bytes		字节数。
	/// \return 没有返回
	virtual void bytes_send (const uint32_t bytes) = 0;
	/// \brief
	/// 指示接收的字节数，可以用来进行数据统计。
	/// \param[in] bytes		字节数。
	/// \return 没有返回
	virtual void bytes_recv (const uint32_t bytes) = 0;
	/// \brief
	/// 指示输出了一个相同的块
	/// \param[in] blk_len	块长度。
	/// \param[in] s_offset	相同块在源文件中的位置偏移。
	/// \return 没有返回
	virtual void on_equal_block(const uint32_t blk_len
								, const uint64_t s_offset) = 0;
	/// \brief
	/// 指示输出了一个差异块
	/// \param[in] blk_len	数据块长度。
	/// \return 没有返回
	virtual void on_diff_block (const uint32_t blk_len) = 0;
};


/// \class
/// \brief
/// 本类用于作为服务端接收同步数据，当执行 run 后，一个任务结果后才会返回。
class DLL_EXPORT xdelta_server
{
	CPassiveSocket server_;					///<	服务器 Socket 对象。
	uint64_t		auto_multiround_filsize_;///<	多轮 Hash 的边界值。
	bool			inplace_;				///<	采用就地构造文件的方式同步。多轮 Hash 与就地构造不能同时出现。
	bool			compress_;				///< 对数据传输是否需要压缩。
	void _start_task (file_operator & foperator
					, xdelta_observer & observer
					, uint16_t port);
public:
	//
	// @auto_multiround_filsize if file's size excess this size
	// will used multiround xdelta.
	//
	xdelta_server (bool compress) 
		: server_ (compress)
		, auto_multiround_filsize_ (NO_MULTI_ROUND_FILSIZE)
		, inplace_ (false)
		, compress_ (compress)
	{
		// less than this size will cause no difference with single round.
		if (MULTIROUND_MAX_BLOCK_SIZE > auto_multiround_filsize_)
			auto_multiround_filsize_ = NO_MULTI_ROUND_FILSIZE;
	}
	/// \brief
	/// 指示同步任务的文件构造通过就地的方式进行。
	/// \return 没有返回
	void set_inplace ();
	/// \brief
	/// 设置需要进行多轮 Hash 同步的文件大小边界。如果没有设置，则默认不进行多轮 Hash。
	/// \param[in]	multi_round_size 执行多轮 Hash 同步的边界大小。
	/// \return 没有返回
	void set_multiround_size (uint64_t multi_round_size);
	~xdelta_server () {}

	/// \brief
	/// 开始执行同步，同步完成后，本接口才会返回。
	/// \param[in]	foperator 文件操作器。
	/// \param[in]	observer 观察者对象。
	/// \param[in]	port	服务器端口。
	/// \return 没有返回
	void run (file_operator & foperator
				, xdelta_observer & observer
				, uint16_t port = XDELTA_DEFAULT_PORT);
};

inline void wait_to_exit (std::vector<thread*> & thrds)
{
	std::for_each (thrds.begin (),thrds.end (), std::mem_fun (&thread::join));
	std::for_each (thrds.begin (), thrds.end (), delete_obj<thread>);
	thrds.clear ();
}

void init_passive_socket (CPassiveSocket & passive, uint16_t port);
void init_active_socket (CActiveSocket & active, const uchar_t * addr, uint16_t port);

inline void read_block (char_buffer<uchar_t> & buff
						, CSimpleSocket & client
						, int32_t len
						, xdelta_observer & observer)
{
	if (len == 0)
		return;

	int32_t nbytes = client.Receive (buff, len);
	if (nbytes <= 0) {
		std::string errmsg = fmt_string ("Socket is broken or closed!");	
		THROW_XDELTA_EXCEPTION (errmsg);
	}
	observer.bytes_recv (len);
}

inline block_header read_block_header (CSimpleSocket & client, xdelta_observer & observer)
{
	DEFINE_STACK_BUFFER (buff);
	read_block (buff, client, BLOCK_HEAD_LEN, observer);
	block_header header;
	buff >> header;
	return header;
}

inline void send_block (CSimpleSocket & socket, char_buffer<uchar_t> & data, xdelta_observer & observer)
{
	int32_t tbytes = data.data_bytes ();
	if (tbytes > 0) {
		if (!socket.Send (data.rd_ptr (), tbytes)) {
			std::string errmsg = fmt_string ("Send data to client failed.");
			THROW_XDELTA_EXCEPTION (errmsg);
		}
		observer.bytes_send (tbytes);
	}
	data.reset ();
}

void buffer_or_send (CSimpleSocket & socket
							, char_buffer<uchar_t> & stream_buff
							, char_buffer<uchar_t> & buff
							, xdelta_observer & observer
							, bool now = false);
} //namespace xdelta

#endif //__XDELTA_SERVER_H__

