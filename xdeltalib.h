/*
 * Copyright (C) 2013- yeyouqun@163.com
 */ 
#ifndef __XDELTA_LIB_H__
#define __XDELTA_LIB_H__

/// @file
/// @mainpage xdeltalib 库使用指南
///
/// @section intro_sec 介绍
/// xdeltalib 库是用 C++ 实现的差异数据提取库，其核心就是 Rsync 的算法。
/// Xdeltalib 库的特点有如下几个：
/// \li 完全用 C++ 写成，可以集中到 C++ 项目中，并充分利用 C++ 的优势。
/// \li 支持多平台，在 Windows 与 Linux 中经过严格测试，也可以整合到 Unix 平台中。
/// \li 代码经过特别优化，差异算法及数据结构经过精心设计，增加了执行性能。
/// \li 支持 in-place 同步算法，可以应用到各种平台中，包括移动平台、服务器环境以及 PC 环境。
/// \li 支持可配置的 multi-round （多轮）同步算法，提高同步效率，同时提高了集成平台的可配置性。
/// \li 集成网络数据传输功能，减少了用户整合的工作量，加快整合进度。
/// \li 支持可配置的或者默认的线程数，充分利用多核优势，提高了执行性能。
/// \li 采用消费者与生产者模型提交与处理任务，充分利用并发优势。
/// \li 一库多用途，即可用于传统的文件数据同步，也可用于其他差异算法可应用的场景。
/// \li 良好的平台适应性。通过特别的设计，提供在各种存储平台的应用，如单设备环境，云存储环境，以及分存式存储环境。
/// \li 完备的文档、支持与快速响应。
///
/// @section port_sec 可移植
/// xdeltalib 库具有良好的可移植性，可能使用于 Windows 平台，Linux 平台，在这两个平台上进行了
/// 全面的测试，只要做较少的工作，即可移植到 Unix 平台。
///
/// @section class_sec 核心类说明
/// xdeltalib.h 文件提供了 xdeltalib 库的核心组件的基础声明，包括
/// \li hasher_stream       用于流化文件 Hash 值的基类。
/// \li xdelta_stream       用于流化根据 Hash 表生成的差异数据的基类，用户可以实现自己的基类，来
///                         来实现使用差异数据的功能。
/// \li hash_table          用于记录 Hash 表，并且利于调整查询跟插入。
/// \li multiround_hash_table 用于记录 Hash 表，并且利于调整查询跟插入，但只用于多轮 Hash 中。
/// \li xdelta_hash_table   用于实现差异数据的计算，并通过 xdelta_stream 流化输出。
/// \li rolling_hasher      实现 Rolling Hash 的类。
///
/// @section misc_sec 其他说明
/// \li 传统的快速文件数据同步，包括本地或者远程。
/// \li 基于源端重复数据删除。
/// \li 各种云存储或者网盘产品，由于数据的同步与去重。
/// \li 以差异数据的方式保存文件多版本。
/// \li等等。


/// \namespace xdelta
/// \brief 最外层名字空间 xdelta
/// xdeltalib 所有的实现均在此名字空间中。
namespace xdelta {

/// \struct
/// 用来描述目标文件的位置信息
struct target_pos
{
	uint64_t	t_offset;		///< Hash 值在目标文件的文件，index 基于这个值来表示。
								///< 本参数只在多轮 Hash 时才会用到，其他情况下为 0。
	uint64_t	index;          ///< Hash 数据块的块索引值。
};

/// \struct
/// 用来描述慢 Hash 值。
struct slow_hash {
    uchar_t		hash[DIGEST_BYTES]; ///< 数据块的 MD4 Hash 值。
	target_pos	tpos;				///< 数据块在目标文件中的位置信息。
};

/// \struct
/// 在多轮 Hash 过程中用来描述文件洞，即 Hash 所处理的文件区域
struct hole_t
{
	uint64_t offset;		///< 文件的偏移。
	uint64_t length;		///< 文件洞的长度。
};

} // namespace xdelta

NAMESPACE_STD_BEGIN

template <> struct less<xdelta::hole_t> {
	bool operator () (const xdelta::hole_t & left, const xdelta::hole_t & right) const
	{
		return right.offset + right.length < left.offset;
	}
};

template <> struct less<xdelta::slow_hash> {
	bool operator () (const xdelta::slow_hash & left, const xdelta::slow_hash & right) const
	{
		return memcmp (left.hash, right.hash, DIGEST_BYTES) < 0 ? true : false;
	}
};

NAMESPACE_STD_END

namespace xdelta {

class DLL_EXPORT xdelta_stream 
{
public:
	virtual ~xdelta_stream () {} 
	/// \brief
	/// 指示开始处理文件的 Hash 流
	/// \param[in] fname	文件名，带相对路径
	/// \param[in] blk_len	处理文件的块长度
	/// \return 没有返回
	virtual void start_hash_stream (const std::string & fname, const int32_t blk_len) = 0;
	/// \brief
	/// 输出一个相同块的块信息记录
	/// \param[in] tpos		块在目标文件中的位置信息。
	/// \param[in] blk_len	块长度。
	/// \param[in] s_offset	相同块在源文件中的位置偏移。
	/// \return 没有返回
	virtual void add_block (const target_pos & tpos
							, const uint32_t blk_len
							, const uint64_t s_offset) = 0;
	/// \brief
	/// 输出一个差异的块数据到流中。
	/// \param[in] data		差异数据块指针。
	/// \param[in] blk_len	数据块长度。
	/// \param[in] s_offset	数据块在源文件中的位置偏移。
	/// \return 没有返回
	virtual void add_block (const uchar_t * data
							, const uint32_t blk_len
							, const uint64_t s_offset) = 0;
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
	/// 指示结束一个文件的 Hash 流的处理。
	/// \param[in] filsize		源文件的大小。
	/// \return 没有返回
	virtual void end_hash_stream (const uint64_t filsize) = 0;
	/// \brief
	/// 指示处理过程中的错误。
	/// \param[in] errmsg		错误信息。
	/// \param[in] errorno		错误码。
	/// \return 没有返回
	virtual void on_error (const std::string & errmsg, const int errorno) = 0;
	/// \brief
	/// 在多轮 Hash 中设置源文件洞对象。
	/// \param[in] holeset		文件洞集合对像。
	/// \return 没有返回
	virtual void set_holes (std::set<hole_t> * holeset) = 0;
};

class DLL_EXPORT hasher_stream 
{
public:
	virtual ~hasher_stream () {}
	/// \brief
	/// 指示开始处理文件的 Hash 流
	/// \param[in] fname	文件名，带相对路径
	/// \param[in] blk_len	处理文件的块长度
	/// \return 没有返回
	virtual void start_hash_stream (const std::string & fname, const int32_t blk_len) = 0;
	/// \brief
	/// 输出一个块数据的快、慢 Hash 值。
	/// \param[in] fhash		快 Hash 值。
	/// \param[in] shash		慢 Hash 值。
	virtual void add_block (const uint32_t fhash, const slow_hash & shash) = 0;
	/// \brief
	/// 指示结束一个文件的 Hash 流的处理。
	/// \param[in] filsize		源文件的大小。
	/// \return 没有返回
	virtual void end_hash_stream (const uchar_t file_hash[DIGEST_BYTES], const uint64_t filsize) = 0;
	/// \brief
	/// 指示结束多轮 Hash 中第一轮，其相应结果相当于单轮 Hash 中的 end_hash_stream。
	/// \param[in] file_hash		整个文件的 MD4 Hash 值。
	/// \return 如果源文件中判断需要继续下一轮，则返回真，否则返回假。
	virtual bool end_first_round (const uchar_t file_hash[DIGEST_BYTES]) = 0;
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
	/// 大多轮 Hash 中设置目标文件洞对象。
	/// \param[in] holeset		文件洞集合对像。
	/// \return 没有返回
	virtual void set_holes (std::set<hole_t> * holeset) = 0;
};

/// 在单轮 Hash 中的最小块长度
#define XDELTA_BLOCK_SIZE 400

/// 在单轮 Hash 中的最大块长度
#define MAX_XDELTA_BLOCK_BYTES ((int32_t)1 << 17) //128KB

/// 在一些内存受限系统中，如果下面的参数定义得很多，有可能导致内存耗用过多，使系统
/// 运行受到影响，甚至是宕机，所以，当你需要你的目标系统的内存特性后，请你自己定义相应的
/// 的大小。不过建议 MULTIROUND_MAX_BLOCK_SIZE 不要小于 512 KB，XDELTA_BUFFER_LEN 必须大于
/// MULTIROUND_MAX_BLOCK_SIZE，最好为 2 的 N 次方倍，如 8 倍 等。
/// 当你的系统中存在大量内存时，较大的内存可以优化实现。使用库同步数据时，软件系统最多占用内存
/// 的大概为：
///		在数据源端（客户端）：
///			线程数 * XDELTA_BUFFER_LEN * 3，如果系统内存受限，你可以采用少线程的方式进行处理方式，
///			但是你无法使用多线程的优势。
///		在数据目标端（服务端）：
///			线程数 * XDELTA_BUFFER_LEN * 2，但是线程数量受到并发的客户端的数目以及每客户端在同步数据时
///			采用的线程数。
/// 由于在同步时，如果文件大小或者块大小，没有达到 XDELTA_BUFFER_LEN 长度，则未被使用的地址系统不会分配
/// 物理内存，因此有时只会占用进程的地址空间，但却不会占用系统的物理内存。

#ifndef MEMORY_LIMIT
	/// 在多轮 Hash 中的最大块长度
	#define MULTIROUND_MAX_BLOCK_SIZE (1 << 22)

	/// 库中使用缓存长度
	#define XDELTA_BUFFER_LEN ((int32_t)1 << 25) // 32MB
#else
	/// 在多轮 Hash 中的最大块长度
	#define MULTIROUND_MAX_BLOCK_SIZE (1 << 20)

	/// 库中使用缓存长度
	#define XDELTA_BUFFER_LEN ((int32_t)1 << 23) // 8MB
#endif

#define MAX(a,b) ((a)>(b)?(a):(b))
    
/// \fn int32_t minimal_multiround_block
/// \brief 取得在多轮 Hash 中最小的块长度, refer to:<<Multiround Rsync.pdf>> to get
/// more details for multiround rsync.
inline int32_t minimal_multiround_block ()
{
	return XDELTA_BLOCK_SIZE;
}

/// \fn int32_t multiround_base 
/// \brief
/// 返回多轮 Hash 中的块基数，即块大小每次除以这个基数，refer to:<<Multiround Rsync.pdf>> to get
/// more details for multiround rsync.
inline int32_t multiround_base ()
{
#define MULTIROUND_BASE_VALUE 3
	return MULTIROUND_BASE_VALUE;
}

class DLL_EXPORT hash_table  {
	hash_map<uint32_t, std::set<slow_hash> * > hash_table_;
	typedef hash_map<uint32_t, std::set<slow_hash> *>::const_iterator chit_t;
	typedef hash_map<uint32_t, std::set<slow_hash> *>::iterator hash_iter;
public:
	hash_table () {}
	virtual ~hash_table ();
	/// \brief
	/// 清除所有的 Hash 值。
	/// \return		no return.
	///
	void clear ();
	/// \brief
	/// 检查 Hash 表是否为空。
	/// \return If hash table is empty,return true, otherwise return false
	///
	bool empty () const { return hash_table_.empty (); }
	/// \brief
	/// 在 Hash 表中插入一个快慢 Hash 对。
	/// \param[in]	fhash 慢速 Hash 值。
	/// \param[in]	shash 慢速 Hash 值。
	/// \return		no return.
	///
	void add_block (const uint32_t fhash, const slow_hash & shash);

	/// \brief
	/// 在表中查找是否有指定块的 Hash 对。
	/// \param[in] fhash	慢速 Hash 值。
	/// \param[in] buf		数据块指针。
	/// \param[in] len		数据块长度
	/// \return	如果存在与指定块相同的 Hash 对，则返回这个 Hash 对的指针，否则返回 0.
	const slow_hash * find_block (const uint32_t fhash
									, const uchar_t * buf
									, const uint32_t len) const;
	/// \brief
	/// 生成文件的 Hash 对，并流化输出。
	/// \param[in] reader	文件对象，数据从这个文件对像中读取数据。
	/// \param[in] stream	流化对象。
	/// \return	No return
	virtual void hash_it (file_reader & reader, hasher_stream & stream) const;
};

class DLL_EXPORT multiround_hash_table : public hash_table
{
	/// \brief 开始第一轮 Hash
	/// \param[in] reader	文件对象。
	/// \param[in] stream	Hash 流对像。
	/// \param[in] holes	目标文件的当前洞
	/// \return			返回当轮的块长度，如果返回 -1，表示不需要再执行下一轮。
	int32_t _haser_first_round (file_reader & reader
								, hasher_stream & stream
								, std::set<hole_t> & holes) const;
	/// \brief 开始下一轮 Hash
	/// \param[in] reader	文件对象。
	/// \param[in] stream	Hash 流对像。
	/// \param[in] holes	目标文件的当前洞。
	/// \param[in] blk_len	本轮 Hash 的块长度。
	/// \return			无返回
	void _next_round (file_reader & reader
							, hasher_stream & stream
							, std::set<hole_t> & holes
							, const int32_t blk_len) const;
	/// \brief
	/// 生成文件的 Hash 对，并流化输出。
	/// \param[in] reader	文件对象，数据从这个文件对像中读取数据。
	/// \param[in] stream	流化对象。
	/// \return	No return
	virtual void hash_it (file_reader & reader, hasher_stream & stream) const;
public:
	multiround_hash_table () {}
	~multiround_hash_table () {}
};

/// \class
/// 此类用于根据接收到的 Hash 表将输入的文件对象执行差异数据提出，并通过 xdelta_stream
/// 对象输出。
class DLL_EXPORT xdelta_hash_table {
protected:
	const hash_table &	hash_table_;
	file_reader &		reader_;
	const int			f_blk_len_;
public:
	/// \brief
	/// 生成 Hash 差异数据计算对象。
	/// \param[in] table	    目标比较文件的 Hash 表。
	/// \param[in] reader	    文件读取对像。
	/// \param[in] f_blk_len    块长度。
	xdelta_hash_table (const hash_table & table
						, file_reader & reader
						, const int f_blk_len);
	~xdelta_hash_table ();
	/// \brief
	/// 流化差异数据并输出。
	/// \param[in] stream	流化对象。
	/// \return	No return
	void xdelta_it (xdelta_stream & stream);
};

/// \fn uint32_t DLL_EXPORT get_xdelta_block_size (const uint64_t filesize)
/// \brief 根据文件大小计算相应的 Hash 块长度。
/// \param[in] filesize 文件的大小。
/// \return     对应的块长度。
uint32_t DLL_EXPORT get_xdelta_block_size (const uint64_t filesize);

/// \class
/// 用于计算快 Hash 值。
class DLL_EXPORT rolling_hasher
{
public:
	rolling_hasher () { RollsumInit ((&sum_)); }
	/// \brief
	/// 计算输出参数的快 hash 值。
	/// \param buf1[in] 数据块指针。
	/// \param len[in]  数据块的长度。
	/// \return     数据块的快 Hash 值。
    static uint32_t hash(const uchar_t *buf1, uint32_t len) {
		Rollsum sum;
		RollsumInit ((&sum))
    	RollsumUpdate (&sum, buf1, len);
		return RollsumDigest ((&sum));
    }
	
	/// \brief
	/// 用输入的数据块初始化对角内部状态，准备调用 Rolling Hash 接口 update。
	/// \param buf1[in] 数据块指针。
	/// \param len[in]  数据块的长度。
	/// \return     没有返回。
	void eat_hash (const uchar_t *buf1, uint32_t len)
	{
		RollsumInit ((&sum_));
#ifdef _WIN32
		std::for_each (buf1, buf1 + len
			, std::bind1st (std::mem_fun1 (&rolling_hasher::_eat), this));
#else
		std::for_each (buf1, buf1 + len
			, std::bind1st (__gnu_cxx::mem_fun1 (&rolling_hasher::_eat), this));
#endif
	}
	
	/// \brief
	/// 返回对像当前的快 Hash 值。
	/// \return     返回对像当前的快 Hash 值。
	uint32_t hash_value () const { return RollsumDigest ((&sum_)); }
    
	/// \brief
	/// 通过出与入字节，执行 Rolling Hash。
	/// \param[in] outchar 窗口中滑出的字节。
	/// \param[in] inchar  窗口中滑入的字节。
	/// \return     返回对像当前的快 Hash 值。
    uint32_t update(uchar_t outchar, uchar_t inchar) {
		RollsumRotate (&sum_, outchar, inchar);
		return RollsumDigest ((&sum_));
    } 
private:
	Rollsum sum_;
	void _eat (uchar_t inchar) { RollsumRollin ((&sum_), inchar); }
};

/// \fn void get_file_digest (file_reader & reader, uchar_t digest[DIGEST_BYTES])
/// \brief 取得文件的 MD4 Hash 值。
/// \param[in] reader	    文件读取对像。
/// \param[in] reader	    Hash 值输出地址。
/// \return     无返回
void DLL_EXPORT get_file_digest (file_reader & reader, uchar_t digest[DIGEST_BYTES]);

/// \struct
/// 用于释放用户传入库中的对象。
struct DLL_EXPORT deletor
{
	virtual void release (file_reader * p) = 0;
	virtual void release (hasher_stream * p) = 0;
	virtual void release (xdelta_stream * p) = 0;
	virtual void release (hash_table * p) = 0;
};

/// \fn char_buffer<char_type> & operator << (char_buffer<char_type> & buff, const slow_hash & var)
/// \brief 将 slow_hash 对象流化到 buff 中。
/// \param[in] buff char_buff 对象。
/// \param[in] var  Slow Hash 对象。
/// \return buff char_buff 的引用。
template <typename char_type>
inline char_buffer<char_type> & operator << (char_buffer<char_type> & buff, const slow_hash & var)
{
	buff << var.tpos.index << var.tpos.t_offset;
	buff.copy (var.hash, DIGEST_BYTES);
	return buff;
}

/// \fn char_buffer<char_type> & operator >> (char_buffer<char_type> & buff, slow_hash & var)
/// \brief 从 buff 中反流化一个 slow_hash 对象。
/// \param[in] buff char_buff 对象。
/// \param[out] var  Slow Hash 对象。
/// \return buff char_buff 的引用。
template <typename char_type>
inline char_buffer<char_type> &	operator >> (char_buffer<char_type> & buff, slow_hash & var)
{
	buff >> var.tpos.index >> var.tpos.t_offset;
	memcpy (var.hash, buff.rd_ptr (), DIGEST_BYTES);
	buff.rd_ptr (DIGEST_BYTES);
	return buff;
}

/// \fn bool is_no_file_error (int32_t error_no)
/// \brief 判断错误代码是否是文件不存在的错误，如目录不存在，文件不存在则返回真，否则返回假。
/// \param[in] error_no 错误代码。
/// \return 如目录不存在，文件不存在则返回真，否则返回假。
inline bool is_no_file_error (const int32_t error_no)
{
	if (error_no != 0 && error_no != ENOENT
#ifdef _WIN32
			&& error_no != ERROR_FILE_NOT_FOUND
			&& error_no != ERROR_PATH_NOT_FOUND
#endif
			) {
		return false;
	}
	return true;
}

} // namespace xdelta
#endif /*__XDELTA_LIB_H__*/

