// definition for class and method for in-place synchronizing file
// author:yeyouqun@eisoo.com

#ifndef __INPLACE_H__
#define __INPLACE_H__

/// @file
/// 声明就地重构文件的类型及相关的 stream 类型。
namespace xdelta {
/// \class
/// 就地文件重构器
class DLL_EXPORT in_place_reconstructor : public reconstructor
{
	/// \brief
	/// 指示开始处理文件的 Hash 流或者数据流。
	/// \param fname[in] 文件名，带相对路径
	/// \param blk_len[in]	处理文件的块长度
	/// \return 没有返回
	virtual void start_hash_stream (const std::string & fname, const int32_t blk_len);
	/// \brief
	/// 输出一个差异的块数据到文件中。
	/// \param[in] data		差异数据块指针。
	/// \param[in] blk_len	数据块长度。
	/// \param[in] s_offset	数据块在源文件中的位置偏移。
	/// \return 没有返回
	virtual void add_block (const uchar_t * data
							, const uint32_t blk_len
							, const uint64_t s_offset);
	/// \brief
	/// 指示结束一个文件的数据流的处理。
	/// \param[in] filsize		源文件的大小。
	/// \return 没有返回
	virtual void end_hash_stream(const uint64_t filsize);
	/// \brief
	/// 指示处理过程中的错误。
	/// \param[in] errmsg		错误信息。
	/// \param[in] errorno		错误码。
	/// \return 没有返回
	virtual void on_error(const std::string & errmsg, const int errorno);
public:
	in_place_reconstructor (file_operator & foperator) : reconstructor (foperator)
	{}
	~in_place_reconstructor () {}
};

/// \struct
/// 描述一个相同的数据对类型。
struct equal_node
{
	uint64_t	s_offset;	///< 源文件中的偏移
	target_pos	tpos;		///< 目标文件中的位置信息
	uint32_t	blength:29; ///< 块长度，绝不会超过 MAX_XDELTA_BLOCK_BYTES 或者 MULTIROUND_MAX_BLOCK_SIZE
	uint32_t	visited:1;  ///< 本对象所表示的块是否已经处理过。
	uint32_t	stacked:1;	///< 本对象所表示的块是否已经在处理栈中。
	uint32_t	deleted:1;	///< 本对象所表示的块是否已经删除（有循环依赖）。
};

/// \struct
/// 描述一个不同的数据对类型
struct diff_node
{
	uint64_t s_offset;	///< 源文件中的偏移，将存储在目标文件中相同的位置。
	uint32_t blength;	///< 块长度。
};

} // namespace xdelta

NAMESPACE_STD_BEGIN

template <> struct less<xdelta::equal_node *> {
	bool operator () (const xdelta::equal_node * left, const xdelta::equal_node * right) const
	{
		return left->tpos.index < right->tpos.index;
	}
};

NAMESPACE_STD_END

namespace xdelta {

/// \class
/// 就地 xdelta 流化类。
class DLL_EXPORT in_place_stream : public xdelta_stream
{
	std::list<diff_node>	diff_nodes_;
	std::list<equal_node *>	equal_nodes_;
	xdelta_stream &			output_;
	file_reader &			reader_;
	char_buffer<uchar_t>	buff_;
private:
	/// \brief
	/// 指示开始处理文件的 Hash 流
	/// \param[in] fname	文件名，带相对路径
	/// \param[in] blk_len	处理文件的块长度
	/// \return 没有返回
	virtual void start_hash_stream (const std::string & fname, const int32_t blk_len);
	/// \brief
	/// 输出一个相同块的块信息记录
	/// \param[in] tpos		块在目标文件中的位置信息。
	/// \param[in] blk_len	块长度。
	/// \param[in] s_offset	相同块在源文件中的位置偏移。
	/// \return 没有返回
	virtual void add_block (const target_pos & tpos
							, const uint32_t blk_len
							, const uint64_t s_offset);
	/// \brief
	/// 输出一个差异的块数据到流中。
	/// \param[in] data		差异数据块指针。
	/// \param[in] blk_len	数据块长度。
	/// \param[in] s_offset	数据块在源文件中的位置偏移。
	/// \return 没有返回
	virtual void add_block (const uchar_t * data
							, const uint32_t blk_len
							, const uint64_t s_offset);
	/// \brief
	/// 指示下一轮 Hash 流开始。
	/// \param[in] blk_len		下一轮 Hash 的块长度。
	/// \return 没有返回
	virtual void next_round (const int32_t blk_len) { output_.next_round (blk_len); }
	/// \brief
	/// 指示结束一轮 Hash，只在多轮 Hash 中调用
	/// \return 没有返回
	virtual void end_one_round () { output_.end_one_round (); }
	/// \brief
	/// 指示结束一个文件的 Hash 流的处理。
	/// \param[in] filsize		源文件的大小。
	/// \return 没有返回
	virtual void end_hash_stream (const uint64_t filsize);
	/// \brief
	/// 在多轮 Hash 中设置源文件洞对象。
	/// \param[in] holeset		文件洞集合对像。
	/// \return 没有返回
	virtual void set_holes (std::set<hole_t> * holeset) {}
	/// \brief
	/// 指示处理过程中的错误。
	/// \param[in] errmsg		错误信息。
	/// \param[in] errorno		错误码。
	/// \return 没有返回
	virtual void on_error (const std::string & errmsg, const int errorno);
	void _clear ();
	/// \brief
	/// 计算就地构造文件中，相同结点的依赖关系，并对先后顺序进行排序。
	void _calc_send ();
	void _handle_node (std::set<equal_node *> & enode_set
						, equal_node * node
						, std::list<equal_node*> & result);
public:
	in_place_stream (xdelta_stream & output, file_reader & reader)
		: output_ (output)
		, reader_ (reader) 
		, buff_ (XDELTA_BUFFER_LEN){}
	~in_place_stream () { _clear (); }
};


} // namespace xdelta

#endif //__INPLACE_H__

