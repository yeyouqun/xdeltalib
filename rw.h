// authour:yeyouqun@163.com

#ifndef __RW_H__
#define __RW_H__
/// @file
/// 平台相关的文件操作基类声明。

namespace xdelta {
/// \class
/// \brief 文件读取基类，你需要重写你自己平台的文件读取类。
class DLL_EXPORT file_reader {
public:
	virtual ~file_reader () {} 
	/// \brief
	/// 打开文件对象。
	/// \return 没有返回
	virtual void open_file () = 0;
	/// \brief
	/// 读取文件。
	/// \param[out] data	数据缓冲区。
	/// \param[in] len		读文件的长度。
	/// \return 返回读取的字节数。
	virtual int read_file (uchar_t * data, const uint32_t len) = 0;
	/// \brief
	/// 关闭文件。
	/// \return 没有返回
	virtual void close_file () = 0;
	/// \brief
	/// 取得文件名。
	/// \return 文件名。
	virtual std::string get_fname () const = 0;
	/// \brief
	/// 取文件大小。
	/// \return 文件大小
	virtual uint64_t get_file_size () const = 0;
	/// \brief
	/// 设置文件读指针。
	/// \return 返回指针位置。
	virtual uint64_t seek_file (const uint64_t offset, const int whence) = 0;
	/// \brief
	/// 判断文件是否存在。
	/// \return 如果存在，则返回 true，如果不存在，或者无法打开，则返回 false。
	virtual bool exist_file () const = 0;
};
/// \class
/// \brief 文件写基类，你需要重写你自己平台的文件写类。
class DLL_EXPORT file_writer {
public:
	virtual ~file_writer () {} 
	/// \brief
	/// 打开文件对象。
	/// \return 没有返回
	/// \throw 如果出错，会选出 xdelta_exception 异常。
	virtual void open_file () = 0;
	/// \brief
	/// 写文件。
	/// \param[out] data	数据缓冲区。
	/// \param[in] len		写文件的长度。
	/// \return 返回写入的字节数。
	virtual int write_file (const uchar_t * data, const uint32_t len) = 0;
	/// \brief
	/// 关闭文件。
	/// \return 没有返回
	virtual void close_file () = 0;
	/// \brief
	/// 取得文件名。
	/// \return 文件名。
	virtual std::string get_fname () const = 0;
	/// \brief
	/// 取文件大小。
	/// \return 文件大小
	virtual uint64_t get_file_size () const = 0;
	/// \brief
	/// 设置文件写指针。
	/// \return 返回指针位置。
	virtual uint64_t seek_file (uint64_t offset, int whence) = 0;
	/// \brief
	/// 判断文件是否存在。
	/// \return 如果存在，则返回 true，如果不存在，或者无法打开，则返回 false。
	virtual bool exist_file () const = 0;
	/// \brief
	/// 设置文件大小，如果小于文件原大小，则截断，否则，再写时，将从新写。可能导致文件洞（视平台）。
	/// \return 无返回。
	virtual void set_file_size (uint64_t filszie) = 0;
};

/// \class
/// 本地文件操作类型。
class DLL_EXPORT f_local_freader : public file_reader {
	virtual void open_file ();
	virtual int read_file (uchar_t * data, const uint32_t len);
	virtual void close_file ();
	virtual std::string get_fname () const { return f_filename_; }
	virtual uint64_t get_file_size () const;
	virtual uint64_t seek_file (const uint64_t offset, const int whence);
	virtual bool exist_file () const;
private:
#ifdef _WIN32
	HANDLE f_handle_;
#else
	int f_fd_;
#endif
	const std::string f_name_;
	const std::string f_path_;
	const std::string f_filename_;
public:
	f_local_freader (const std::string & path, const std::string & fname);
	~f_local_freader();
};

/// \class
/// 本地文件操作类型。
class DLL_EXPORT f_local_fwriter : public file_writer {
	virtual void open_file ();
	virtual int write_file (const uchar_t * data, const uint32_t len);
	virtual void close_file ();
	virtual std::string get_fname () const { return f_filename_; }
	virtual uint64_t get_file_size () const;
	virtual uint64_t seek_file (uint64_t offset, int whence);
	virtual bool exist_file () const;
	virtual void set_file_size (uint64_t filszie);
private:
#ifdef _WIN32
	HANDLE f_handle_;
#else
	int f_fd_;
#endif
	const std::string f_name_;
	const std::string f_path_;
	const std::string f_filename_;
public:
	f_local_fwriter (const std::string & path, const std::string & fname);
	~f_local_fwriter();
};

/// \class
/// 文件操作基类型。
class DLL_EXPORT file_operator
{
public:
	virtual ~file_operator () {}
	/// \brief
	/// 创建文件读对象。
	/// \param[in] filename 文件名。
	/// \return 返回对象指针。
	virtual file_reader * create_reader (const std::string & filename) = 0;
	/// \brief
	/// 创建文件写对象。
	/// \param[in] filename 文件名。
	/// \return 返回对象指针。
	virtual file_writer * create_writer (const std::string & filename) = 0;
	/// \brief
	/// 释放文件对象
	/// \param[in] reader 对象指针。
	/// \return 没有返回。
	virtual void release (file_reader * reader) = 0;
	/// \brief
	/// 释放文件对象
	/// \param[in] writer 对象指针。
	/// \return 没有返回。
	virtual void release (file_writer * writer) = 0;
	/// \brief
	/// 文件更名
	/// \param[in] old		旧文件名。
	/// \param[in] newname  新文件名。
	/// \return 没有返回。
	virtual void rename (const std::string & old, const std::string & newname) = 0;
	/// \brief
	/// 删除文件
	/// \param[in] filename		文件名。
	/// \return 没有返回。
	virtual void rm_file (const std::string & filename) = 0; 
};

bool exist_file (const std::string & filename);

/// \class
/// 本地文件操作类型。
class DLL_EXPORT f_local_creator : public file_operator
{
	const std::string path_;
public:
	f_local_creator (const std::string & path) : path_ (path) {}
	~f_local_creator () {}
	virtual file_reader * create_reader (const std::string & filename)
	{
		std::string pathname = path_ + SEPERATOR + filename;
		return new f_local_freader (path_, filename);
	}
	virtual file_writer * create_writer (const std::string & filename)
	{
		return new f_local_fwriter (path_, filename);
	}

	virtual void release (file_reader * reader)
	{
		if (reader)
			delete reader;
	}
	virtual void release (file_writer * writer)
	{
		if (writer)
			delete writer;
	}
	virtual void rename (const std::string & old, const std::string & newname)
	{
		std::string oldname = path_ + SEPERATOR + old,
			newname1 = path_ + SEPERATOR + newname;
		::remove (newname1.c_str ());
		::rename (oldname.c_str (), newname1.c_str ());
	}
	virtual void rm_file (const std::string & filename)
	{
		std::string name = path_ + SEPERATOR + filename;
#ifdef _WIN32
		DeleteFileA (name.c_str ());
#else
		unlink (name.c_str ());
#endif
	}
};

} // namespace xdelta
#endif //__RW_H__

