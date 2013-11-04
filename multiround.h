// authour:yeyouqun@163.com

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
	virtual void add_block (const target_pos & tpos
							, const uint32_t blk_len
							, const uint64_t s_offset);
	virtual void add_block (const uchar_t * data, const uint32_t blk_len, const uint64_t s_offset);
};

/// \class
/// 用在源中的 Hasher Stream。
class DLL_EXPORT multiround_hasher_stream : public hasher_stream
{
	virtual void start_hash_stream (const std::string & fname, const int32_t blk_len);
	virtual void add_block (const uint32_t fhash, const slow_hash & shash);
	virtual void end_hash_stream (const uchar_t file_hash[DIGEST_BYTES], const uint64_t filsize);
	virtual bool end_first_round (const uchar_t file_hash[DIGEST_BYTES]);
	virtual void next_round (const int32_t blk_len);
	virtual void end_one_round ();
	virtual void on_error (const std::string & errmsg, const int errorno);
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