// authour:yeyouqun@163.com

#ifndef __BUFFER_H__
#define __BUFFER_H__
/// \file
/// \brief
/// 本文件中的类提供 Buffer 类的声明。其作用如下：
/// \li 提供缓冲区空间。
/// \li 提供缓冲区空间管理。
/// \li 提供缓冲区空间自主分配，以及由用户分配管理（主要是在栈空间上分配少量缓存的情）。
/// 以及提供将变动流化或者反流化到缓冲区的接口。

namespace xdelta {

/// \class
/// \brief A very simple buffer class that allocates a buffer of 
/// a given type and size in the constructor and 
/// deallocates the buffer in the destructor. 
/// 
/// This class is useful everywhere where a temporary buffer 
/// is needed. 
/// T 的类型限于 char 以及 unsigned char 两种类型。
template <class T> 
class char_buffer 
{ 
public: 
	/// \brief
	/// 生成一个大小为 size 的缓冲区对象
	/// \param size[in]	缓冲区大小。
	char_buffer(const std::size_t size): 
		_size(size), 
		_occupied (0),
		_bytes_read (0),
		_ptr((T*)malloc (size * sizeof (T))),
		auto_release_ (true) { ; } 

	/// \brief
	/// 生成一个大小为 size 的缓冲区对象，但缓冲区由用户自行管理
	/// \param size[in]	缓冲区大小。
	char_buffer(T * buff, const std::size_t size): 
		_size(size), 
		_occupied (0),
		_bytes_read (0),
		_ptr(buff),
		auto_release_ (false) { ; } 
	 
	~char_buffer()
	{
		if (auto_release_)
			free (_ptr);
	}
	/// \brief
	/// 返回缓冲区的大小
	/// \return 返回缓冲区的大小
	std::size_t size() const { return _size; } 
	/// \brief
	/// 返回缓冲区的首部
	/// \return 返回缓冲区的首部
	T* begin() { return _ptr; }
	const T* begin() const { return _ptr; }
	/// \brief
	/// 返回缓冲区的读指针，即去除了先期已经处理过的字节。
	/// \return 返回缓冲区的读指针
	const T* rd_ptr () const {	return _ptr + _bytes_read; }
	T * rd_ptr () {	return _ptr + _bytes_read; }
	/// \brief
	/// 设置缓冲区的读指针
	/// \param[in] bytes 读指针的移动字节数。
	/// \return 无返回
	void rd_ptr (const std::size_t bytes) {	_bytes_read += bytes; }

	/// \brief
	/// 返回缓冲区的写指针，即去除了先期已经存储过的字节。
	/// \return 返回缓冲区的读指针
	const T* wr_ptr () const {	return _ptr + _occupied; }
	T * wr_ptr () {	return _ptr + _occupied; }
	/// \brief
	/// 设置缓冲区的写指针
	/// \param[in] bytes 写指针的移动字节数。
	/// \return 无返回
	void wr_ptr (const std::size_t bytes) {	_occupied += bytes; }

	/// \brief
	/// 重置所有指定参数。
	/// \return 无返回
	void reset () { _occupied = 0; _bytes_read = 0; _occupied = 0; }
	/// \brief
	/// 返回缓冲区的尾部
	/// \return 返回缓冲区的尾部指针
 	T* end() { return _ptr + _size; } 
	const T* end() const  { return _ptr + _size; }
	/// \brief
	/// 向缓冲区中复制数据，并设置对应的写指针。
	/// \param[in] data 写指针的移动字节数。
	/// \param[in] len 写指针的移动字节数。
	/// \return 无返回
	/// \throw  xdelta_exeption 如果空间不足，将丢出 xdelta_exeption 异常。
	void copy (const T * data, const uint32_t len)
	{
		uint32_t remain = (uint32_t)(_size - _occupied);
		if (remain < len)
			THROW_XDELTA_EXCEPTION_NO_ERRNO ("No sufficient space left on buffer.");

		memcpy (_ptr + _occupied, data, len);
		_occupied += len;
	}
	/// \brief
	/// 返回缓冲区被占用的字节数
	/// \return 返回缓冲区被占用的字节数
	uint32_t occupied () const { return (uint32_t)_occupied; }
	/// \brief
	/// 返回缓冲区可用数据字节数
	/// \return 返回缓冲区可用数据字节数
	uint32_t data_bytes () const { return (uint32_t)(wr_ptr () - rd_ptr ()); } 
	/// \brief
	/// 返回缓冲区被可以空间字节数
	/// \return 返回缓冲区被可以空间字节数
	uint32_t available () const { return (uint32_t)(_size - _occupied); }
private: 
	char_buffer(); 
	char_buffer(const char_buffer&); 
	char_buffer& operator = (const char_buffer&); 
 
	std::size_t _size; 
	std::size_t _occupied;
	std::size_t _bytes_read;
	T* _ptr; 
	bool		auto_release_;
};

/// \fn char_buffer<char_type> & operator << (char_buffer<char_type> & buff, uint16_t var)
/// \brief 将对象变量流化到 buff 中。
/// \param[in] buff char_buff 对象。
/// \param[in] var  变量值。
/// \return buff char_buff 的引用。
template <typename char_type>
inline char_buffer<char_type> & operator << (char_buffer<char_type> & buff, uint16_t var)
{
	// we use little endian bytes order.
	char_type * arr = buff.wr_ptr ();
#if BYTE_ORDER == BIG_ENDIAN
	arr[0] = (char_type)(var);
	arr[1] = (char_type)(var >> 8);
#else
	memcpy (arr, &var, 2);
#endif
	buff.wr_ptr (2);
	return buff;
}

// variable to bytes.
template <typename char_type>
inline char_buffer<char_type> & operator << (char_buffer<char_type> & buff, int16_t var)
{
	return buff << (uint16_t)var;
}

/// \fn char_buffer<char_type> & operator << (char_buffer<char_type> & buff, uint32_t var)
/// \brief 将对象变量流化到 buff 中。
/// \param[in] buff char_buff 对象。
/// \param[in] var  变量值。
/// \return buff char_buff 的引用。
template <typename char_type>
inline char_buffer<char_type> & operator << (char_buffer<char_type> & buff, uint32_t var)
{
	// we use little endian bytes order.
	char_type * arr = buff.wr_ptr ();
#if BYTE_ORDER == BIG_ENDIAN
	arr[0] = (char_type)(var);
	arr[1] = (char_type)(var >> 8);
	arr[2] = (char_type)(var >> 16);
	arr[3] = (char_type)(var >> 24);
#else
	memcpy (arr, &var, 4);
#endif
	buff.wr_ptr (4);
	return buff;
}

/// \fn char_buffer<char_type> & operator << (char_buffer<char_type> & buff, uint64_t var)
/// \brief 将对象变量流化到 buff 中。
/// \param[in] buff char_buff 对象。
/// \param[in] var  变量值。
/// \return buff char_buff 的引用。
template <typename char_type>
inline char_buffer<char_type> & operator << (char_buffer<char_type> & buff, uint64_t var)
{
	// we use little endian bytes order.
	char_type * arr = buff.wr_ptr ();
#if BYTE_ORDER == BIG_ENDIAN
	arr[0] = (char_type)(var);
	arr[1] = (char_type)(var >> 8);
	arr[2] = (char_type)(var >> 16);
	arr[3] = (char_type)(var >> 24);
	arr[4] = (char_type)(var >> 32);
	arr[5] = (char_type)(var >> 40);
	arr[6] = (char_type)(var >> 48);
	arr[7] = (char_type)(var >> 56);
#else
	memcpy (arr, &var, 8);
#endif
	buff.wr_ptr (8);
	return buff;
}

/// \fn char_buffer<char_type> & operator << (char_buffer<char_type> & buff, const std::string & var)
/// \brief 将对象变量流化到 buff 中。
/// \param[in] buff char_buff 对象。
/// \param[in] var  变量值。
/// \return buff char_buff 的引用。
template <typename char_type>
inline char_buffer<char_type> & operator << (char_buffer<char_type> & buff, const std::string & var)
{
	// we use little endian bytes order.
	uint32_t len = (uint32_t)var.length ();
	buff << len;
	memcpy (buff.wr_ptr (), var.c_str (), len);
	buff.wr_ptr (len);
	return buff;
}

/// \fn char_buffer<char_type> & operator >> (char_buffer<char_type> & buff, std::string & var)
/// \brief 将对象变量反流化到 buff 中。
/// \param[in] buff char_buff 对象。
/// \param[in] var  变量值。
/// \return buff char_buff 的引用。
template <typename char_type>
inline char_buffer<char_type> & operator >> (char_buffer<char_type> & buff, std::string & var)
{
	// we use little endian bytes order.
	uint32_t len;
	buff >> len;
	var.clear ();
	var.assign ((char_t*)buff.rd_ptr (), len);
	buff.rd_ptr (len);
	return buff;
}

/// \fn char_buffer<char_type> & operator >> (char_buffer<char_type> & buff, uint16_t & var)
/// \brief 将对象变量反流化到 buff 中。
/// \param[in] buff char_buff 对象。
/// \param[in] var  变量值。
/// \return buff char_buff 的引用。
template <typename char_type>
inline char_buffer<char_type> & operator >> (char_buffer<char_type> & buff, uint16_t & var)
{
	// we use little endian bytes order.
	char_type * rdptr = buff.rd_ptr ();
#if BYTE_ORDER == BIG_ENDIAN
	var = ((uint16_t)rdptr [0]) | (((uint16_t)rdptr [1]) << 8);
#else
	memcpy (&var, rdptr, 2);
#endif
	buff.rd_ptr (2);
	return buff;
}

/// \fn char_buffer<char_type> & operator >> (char_buffer<char_type> & buff, uint32_t & var)
/// \brief 将对象变量反流化到 buff 中。
/// \param[in] buff char_buff 对象。
/// \param[in] var  变量值。
/// \return buff char_buff 的引用。
template <typename char_type>
inline char_buffer<char_type> & operator >> (char_buffer<char_type> & buff, uint32_t & var)
{
	// we use little endian bytes order.
	char_type * rdptr = buff.rd_ptr ();
#if BYTE_ORDER == BIG_ENDIAN
	var = ((uint32_t)rdptr [0]) | (((uint32_t)rdptr [1]) << 8)
			| (((uint32_t)rdptr [2]) << 16) | (((uint32_t)rdptr [3]) << 24);
#else
	memcpy (&var, rdptr, 4);
#endif
	buff.rd_ptr (4);
	return buff;
}

/// \fn char_buffer<char_type> & operator >> (char_buffer<char_type> & buff, uint64_t & var)
/// \brief 将对象变量反流化到 buff 中。
/// \param[in] buff char_buff 对象。
/// \param[in] var  变量值。
/// \return buff char_buff 的引用。
template <typename char_type>
inline char_buffer<char_type> & operator >> (char_buffer<char_type> & buff, uint64_t & var)
{
	// we use little endian bytes order.
	char_type * rdptr = buff.rd_ptr ();
#if BYTE_ORDER == BIG_ENDIAN
	var = ((uint64_t)rdptr [0]) | (((uint64_t)rdptr [1]) << 8)
			| (((uint64_t)rdptr [2]) << 16) | (((uint64_t)rdptr [3]) << 24)
			| (((uint64_t)rdptr [4]) << 32) | (((uint64_t)rdptr [5]) << 40)
			| (((uint64_t)rdptr [6]) << 48) | (((uint64_t)rdptr [7]) << 56);
#else
	memcpy (&var, rdptr, 8);
#endif
	buff.rd_ptr (8);
	return buff;
}

/// \enum block_type
/// \brief 定义在目标文件与源文件之间的块类型
/// xdeltalib 库使用的是 PUSH 模式来同步数据，即由源端发起同步请求。
enum block_type {
	BT_CLIENT_BLOCK,			///< 客户端发送的初始数据块，其中记录了客户等待连接的端口号，由服务服务端进行连接。
								///< 通过这些连接在两者之间传递数据及 Hash 结果。
	BT_SERVER_BLOCK,			///< 服务端针对 BT_CLIENT_BLOCK 返回信息块，包括块长度，文件名等。
	BT_CLIENT_FILE_BLOCK,		///< 客户端发送到服务端，发送文件信息。
	BT_XDELTA_BEGIN_BLOCK,		///< 客户端发送到服务端，开始一个文件的同步。
	BT_EQUAL_BLOCK,				///< 客户端发送到服务端，一个相同的块信息。
	BT_DIFF_BLOCK,				///< 客户端发送到服务端，一个差异块信息及数据。
	BT_END_FIRST_ROUND,			///< 客户端发送到服务端，如果多轮 Hash 中不会有多轮进行，则不会发送本块。
	BT_XDELTA_END_BLOCK,		///< 客户端发送到服务端，结束一个文件的同步。
	BT_HASH_BEGIN_BLOCK,		///< 服务端发往客户端，开始一个文件处理。
	BT_HASH_BLOCK,				///< 服务端发往客户端，一个 Hash 块，记录 Hash 值。
	BT_BEGIN_ONE_ROUND,			///< 服务端发往客户端，开始 新一轮。
	BT_END_ONE_ROUND,			///< 服务端发往客户端，结束一轮。
	BT_HASH_END_BLOCK,			///< 服务端发往客户端，开始一个文件处理。
};

/// \struct
/// \brief 数据块的块头结构
struct block_header
{
#define BLOCK_HEAD_LEN 6
	uint16_t	blk_type;		///< 块类型，如 block_type。
	uint32_t	blk_len;		///< 块长度。
};

/// \fn char_buffer<char_type> & operator << (char_buffer<char_type> & buff, const block_header & var)
/// \brief 将对象变量流化到 buff 中。
/// \param[in] buff char_buff 对象。
/// \param[in] var  变量值。
/// \return buff char_buff 的引用。
template <typename char_type>
inline char_buffer<char_type> & operator << (char_buffer<char_type> & buff, const block_header & var)
{
	return buff << var.blk_type << var.blk_len;
}

/// \fn char_buffer<char_type> & operator >> (char_buffer<char_type> & buff, block_header & var)
/// \brief 将对象变量反流化到 buff 中。
/// \param[in] buff char_buff 对象。
/// \param[in] var  变量值。
/// \return buff char_buff 的引用。
template <typename char_type>
inline char_buffer<char_type> & operator >> (char_buffer<char_type> & buff, block_header & var)
{
	return buff >> var.blk_type >> var.blk_len;
}

template <typename char_type>
inline char_buffer<char_type> & operator << (char_buffer<char_type> & buff, int32_t var)
{
	return buff << (uint32_t)var;
}

template <typename char_type>
inline char_buffer<char_type> & operator << (char_buffer<char_type> & buff, int64_t var)
{
	return buff << (uint64_t)var;
}

// from bytes to variable.
template <typename char_type>
inline char_buffer<char_type> &	operator >> (char_buffer<char_type> & buff, int16_t & var)
{
	uint16_t tmp;
	buff >> tmp;
	var = (int16_t)tmp;
	return buff;
}
template <typename char_type>
inline char_buffer<char_type> &	operator >> (char_buffer<char_type> & buff, int32_t & var)
{
	uint32_t tmp;
	buff >> tmp;
	var = (int32_t)tmp;
	return buff;
}
template <typename char_type>
inline char_buffer<char_type> &	operator >> (char_buffer<char_type> & buff, int64_t & var)
{
	uint64_t tmp;
	buff >> tmp;
	var = (int64_t)tmp;
	return buff;
}

/// \def DEFINE_STACK_BUFFER(name)
/// 定义一个在栈上的缓冲区，STACK_BUFF_LEN 不能太大。
#define DEFINE_STACK_BUFFER(name)								\
	uchar_t stack_space_##name[STACK_BUFF_LEN];					\
	char_buffer<uchar_t> name (stack_space_##name, STACK_BUFF_LEN)

} // namespace xdelta
#endif //__BUFFER_H__

