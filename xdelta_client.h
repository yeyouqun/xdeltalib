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
#ifndef __XDELTA_CLIENT_H__
#define __XDELTA_CLIENT_H__
/// @file
/// 声明网络客户端处理基类。
namespace xdelta {
struct client_slot_t;

/// \class
/// \brief 客户端执行时的类。

class DLL_EXPORT xdelta_client
{
	CActiveSocket			client_;		///< 本端对象
	client_slot_t		*	task_slot_;		///< 任务槽
	uint32_t				thread_nr_;		///< 当前线程数。
	bool					compress_;		///< 对数据传输是否需要压缩。
public:
	xdelta_client (bool compress, uint32_t thread_nr = 0);
	~xdelta_client ();
	/// \brief
	/// 开户任务执行。
	/// 客户端执行顺序:run -> add_task -< 完成了么？>--Y--->  wait
	///                    ^               |
	///                    \----<-----N----/   
	/// \param[in] foperator	文件操作器。
	/// \param[in] observer		进度查看器。
	/// \param[in] paddr		服务器地址，如果是本地，则为 127.0.0.1，或者 0。
	/// \param[in] port			服务器执行端口。
	/// \return 无返回。
	void run (file_operator & foperator
				, xdelta_observer & observer
				, const uchar_t * paddr
				, uint16_t port = XDELTA_DEFAULT_PORT);
	/// \brief
	/// 添加一个文件同步任务。
	/// \param[in] reader	文件读取对象。
	/// \param[in] pdel		文件对象删除器。
	/// \return 无返回。
	void add_task (file_reader * reader, deletor * pdel);
	/// \brief
	/// 等待所有已经添加的任务执行完成后返回。
	/// \return 无返回。
	void wait ();
};

} //namespace xdelta

#endif //__XDELTA_CLIENT_H__

