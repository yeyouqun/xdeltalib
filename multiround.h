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

#ifndef __MULTIROUND_H__
#define __MULTIROUND_H__
/// @file
/// 声明多轮 Hash 中的类型。

namespace xdelta {
class multiround_hasher_stream;

/// \class
/// 多轮 Hash 中的文件重构器。
class DLL_EXPORT multiround_reconstructor : public reconstructor
{
	std::set<hole_t> *	target_hole_;	///< 目标文件同，在重要中会不断进行计算更新。
public:
	multiround_reconstructor (file_operator & foperator) 
								: reconstructor (foperator)
	{}
	void set_holes (std::set<hole_t> * holeset)
	{
		target_hole_ = holeset;
	}
	virtual ~multiround_reconstructor () {}
	/// \brief
	/// 输出一个相同块的块信息记录，向文件构造一个相同的数据块。
	/// \param[in] tpos		块在目标文件中的位置信息。
	/// \param[in] blk_len	块长度。
	/// \param[in] s_offset	相同块在源文件中的位置偏移。
	/// \return 没有返回
	virtual void add_block(const target_pos & tpos
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
};

/// \class
/// 用在源中的 Hasher Stream。
class DLL_EXPORT multiround_hasher_stream : public hasher_stream
{
	/// \brief
	/// 指示开始处理文件的 Hash 流
	/// \param[in] fname	文件名，带相对路径
	/// \param[in] blk_len	处理文件的块长度
	/// \return 没有返回
	virtual void start_hash_stream (const std::string & fname, const int32_t blk_len);
	/// \brief
	/// 输出一个块数据的快、慢 Hash 值。
	/// \param[in] fhash		快 Hash 值。
	/// \param[in] shash		慢 Hash 值。
	virtual void add_block (const uint32_t fhash, const slow_hash & shash);
	/// \brief
	/// 指示结束一个文件的 Hash 流的处理。
	/// \param[in] filsize		源文件的大小。
	/// \return 没有返回
	virtual void end_hash_stream (const uchar_t file_hash[DIGEST_BYTES], const uint64_t filsize);
	/// \brief
	/// 指示结束多轮 Hash 中第一轮，其相应结果相当于单轮 Hash 中的 end_hash_stream。
	/// \param[in] file_hash		整个文件的 MD4 Hash 值。
	/// \return 如果源文件中判断需要继续下一轮，则返回真，否则返回假。
	virtual bool end_first_round (const uchar_t file_hash[DIGEST_BYTES]);
	/// \brief
	/// 指示下一轮 Hash 流开始。
	/// \param[in] blk_len		下一轮 Hash 的块长度。
	/// \return 没有返回
	virtual void next_round (const int32_t blk_len);
	/// \brief
	/// 指示结束一轮 Hash，只在多轮 Hash 中调用
	/// \return 没有返回
	virtual void end_one_round ();
	/// \brief
	/// 指示处理过程中的错误。
	/// \param[in] errmsg		错误信息。
	/// \param[in] errorno		错误码。
	/// \return 没有返回
	virtual void on_error (const std::string & errmsg, const int errorno);
	/// \brief
	/// 在多轮 Hash 中设置目标文件洞对象。
	/// \param[in] holeset		文件洞集合对像。
	/// \return 没有返回
	virtual void set_holes (std::set<hole_t> * holeset)
	{
		output_->set_holes (holeset);
	}
private:
	xdelta_stream *		output_;		///< 输出流对像
	file_reader &		reader_;		///< 源文件读对象
	deletor *			stream_deletor_;		///< 对象删除器
	std::set<hole_t>	source_holes_;		///< 源文件洞。
	hash_table			hashes_;		///< 当前接收到的 Hash 表对象。
	int32_t				blk_len_;		///< 当前处理的块长度。
public:
	~multiround_hasher_stream () 
	{
		if (stream_deletor_) stream_deletor_->release (output_);
	}
	multiround_hasher_stream (file_reader & reader
							, xdelta_stream * output
							, deletor * stream_deletor)
		: output_ (output)
		, reader_ (reader)
		, stream_deletor_ (stream_deletor)
		, blk_len_ (0)
	{
		;
	}
};

} // namespace xdelta

#endif //__MULTIROUND_H__

