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
	virtual void start_hash_stream (const std::string & fname, const int32_t blk_len);
	virtual void add_block (const uchar_t * data, const uint32_t blk_len, const uint64_t s_offset);
	virtual void end_hash_stream (const uint64_t filsize);
	virtual void on_error (const std::string & errmsg, const int errorno);
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
	virtual void start_hash_stream (const std::string & fname, const int32_t blk_len);
	virtual void add_block (const target_pos & tpos
							, const uint32_t blk_len
							, const uint64_t s_offset);
	virtual void add_block (const uchar_t * data
							, const uint32_t blk_len
							, const uint64_t s_offset);
	virtual void next_round (const int32_t blk_len) { output_.next_round (blk_len); }
	virtual void end_one_round () { output_.end_one_round (); }
	virtual void end_hash_stream (const uint64_t filsize);
	virtual void set_holes (std::set<hole_t> * holeset) {}
	virtual void on_error (const std::string & errmsg, const int errorno);
	void _clear ();
	//
	// calculate the dependency
	//
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

