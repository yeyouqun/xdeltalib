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

#ifndef __RECONSTRUCT_H__
#define __RECONSTRUCT_H__
/// @file
/// 声明重构文件的基本类型及定义。

namespace xdelta {

/// \class reconstructor
/// 文件重构基类。
class DLL_EXPORT reconstructor
{
protected:
	file_operator &		foperator_;			///< 文件操作器，用于返回文件对像等
	file_reader *		reader_;			///< 文件读对象。
	file_writer *		writer_;			///< 文件写对象。
	std::string			fname_;				///< 处理的文件名。
	std::string			ftmpname_;			///< 临时文件名，在重要时生成。
	char_buffer<uchar_t>	buff_;			///< 缓冲区。
public:
	reconstructor (file_operator & foperator) : foperator_ (foperator)
		, reader_ (0), writer_ (0)
		, buff_ (XDELTA_BUFFER_LEN)
	{}
	virtual ~reconstructor () {}
	/// \brief
	/// 指示开始处理文件的 Hash 流或者数据流。
	/// \param fname[in] 文件名，带相对路径
	/// \param blk_len[in]	处理文件的块长度
	/// \return 没有返回
	virtual void start_hash_stream (const std::string & fname, const int32_t blk_len);
	/// \brief
	/// 输出一个相同块的块信息记录，向文件构造一个相同的数据块。
	/// \param[in] tpos		块在目标文件中的位置信息。
	/// \param[in] blk_len	块长度。
	/// \param[in] s_offset	相同块在源文件中的位置偏移。
	/// \return 没有返回
	virtual void add_block (const target_pos & tpos
							, const uint32_t blk_len
							, const uint64_t s_offset);
	/// \brief
	/// 输出一个差异的块数据到文件中。
	/// \param[in] data		差异数据块指针。
	/// \param[in] blk_len	数据块长度。
	/// \param[in] s_offset	数据块在源文件中的位置偏移。
	/// \return 没有返回
	virtual void add_block(const uchar_t * data
							, const uint32_t blk_len
							, const uint64_t s_offset);
	/// \brief
	/// 指示结束一个文件的数据流的处理。
	/// \param[in] filsize		源文件的大小。
	/// \return 没有返回
	virtual void end_hash_stream (const uint64_t filsize);
	/// \brief
	/// 指示处理过程中的错误。
	/// \param[in] errmsg		错误信息。
	/// \param[in] errorno		错误码。
	/// \return 没有返回
	virtual void on_error (const std::string & errmsg, const int errorno);
};

uint32_t seek_and_read (file_reader & reader
					, uint64_t offset
					, uint32_t length
					, uchar_t * data);
} // namespace xdelta
#endif //__RECONSTRUCT_H__

