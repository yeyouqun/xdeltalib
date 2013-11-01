// authour:yeyouqun@163.com
#include <string>

#ifdef _WIN32
	#include <windows.h>
	#include <io.h>
	#include <direct.h>
#else
	#include <sys/types.h>
    #include <unistd.h>
    #include <sys/stat.h>
	#include <fcntl.h>
	#include <stdio.h>
	#include <dirent.h>
#endif
#include <errno.h>

#include "mytypes.h"
#include "platform.h"
#include "rw.h"

namespace xdelta {
static uint64_t seek_file (
#ifdef _WIN32
						   HANDLE handle,
#else
						   int fd, 
#endif
						   uint64_t offset, int whence, const std::string & filename)
{
#ifdef _WIN32
	if (handle == INVALID_HANDLE_VALUE) {
		std::string errmsg ("File not open.");
		THROW_XDELTA_EXCEPTION_NO_ERRNO (errmsg);
	}

	int hiOffset = (int) (offset >> 32);
	offset = ::SetFilePointer (handle, 
									 (int) (offset & 0xFFFFFFFF), 
									 (LPLONG) (&hiOffset), 
									 whence);

	if(offset == INVALID_SET_FILE_POINTER) {
		std::string errmsg = fmt_string ("Can't seek file %s.", filename.c_str ());
		THROW_XDELTA_EXCEPTION (errmsg);
	}
	
	return (offset | ((int64_t)hiOffset << 32));
#else
	if (fd == -1) {
		std::string errmsg ("File not open.");
		THROW_XDELTA_EXCEPTION_NO_ERRNO (errmsg);
	}
	off_t res = lseek (fd, offset, whence);
	if (res == -1) {
		std::string errmsg = fmt_string ("Can't seek file %s.", filename.c_str ());
		THROW_XDELTA_EXCEPTION (errmsg);
	}
	return res;
#endif
}

void f_local_freader::open_file ()
{
#ifdef _WIN32
	if (f_handle_ != INVALID_HANDLE_VALUE)
		return;

	f_handle_ = ::CreateFileA (f_name_.c_str (),
							   GENERIC_READ, 
							   FILE_SHARE_READ | FILE_SHARE_WRITE,
							   0,
							   OPEN_EXISTING,
							   FILE_ATTRIBUTE_NORMAL,
							   0);

	if(f_handle_ == INVALID_HANDLE_VALUE) {
		std::string errmsg = fmt_string ("Can't not open file %s.", f_name_.c_str ());
		THROW_XDELTA_EXCEPTION (errmsg);
	}
#else
	if (f_fd_ > 0)
		return;
	f_fd_ = open (f_name_.c_str (), O_RDONLY | O_BINARY);
	if (f_fd_ < 0) {
		std::string errmsg = fmt_string ("Can't not open file %s.", f_name_.c_str ());
		THROW_XDELTA_EXCEPTION (errmsg);
	}
#endif
}

bool exist_file (const std::string & filename)
{
#ifdef _WIN32
	DWORD res = GetFileAttributesA (filename.c_str ());
	if (res == INVALID_FILE_ATTRIBUTES) {
		UINT lasterror = GetLastError ();
		if (lasterror == ERROR_FILE_NOT_FOUND || lasterror == ERROR_PATH_NOT_FOUND)
			return false;
	}
#else
	struct stat st;
	int ret = lstat(filename.c_str (), &st);
	if (ret < 0 && errno == ENOENT)
		return false;
#endif
	return true;
}

bool f_local_freader::exist_file () const
{
	return xdelta::exist_file (f_name_);
}

int f_local_freader::read_file (uchar_t * data, const uint32_t len)
{
	if (data == 0 || len == 0) {
		std::string errmsg ("Parameter(s) not correct");
		THROW_XDELTA_EXCEPTION_NO_ERRNO (errmsg);
	}
#ifdef _WIN32
	if (f_handle_ == INVALID_HANDLE_VALUE) {
		std::string errmsg ("File not open.");
		THROW_XDELTA_EXCEPTION_NO_ERRNO (errmsg);
	}
	uint32_t bytes;
	int result = ::ReadFile (f_handle_, data, len, (LPDWORD)&bytes, NULL);
	
	if (result == FALSE) {
		std::string errmsg = fmt_string ("Can't read file %s", f_name_.c_str ());
		THROW_XDELTA_EXCEPTION (errmsg);
	}

	return (int)bytes;
#else
	if (f_fd_ == -1) {
		std::string errmsg ("File not open.");
		THROW_XDELTA_EXCEPTION_NO_ERRNO (errmsg);
	}
	return read (f_fd_, data, len);
#endif
}

#ifdef _WIN32
static uint64_t get_file_size (HANDLE handle)
#else
static uint64_t get_file_size (const std::string & filename)
#endif
{
#ifdef _WIN32
	if (handle == INVALID_HANDLE_VALUE) {
		std::string errmsg ("File not open.");
		THROW_XDELTA_EXCEPTION_NO_ERRNO (errmsg);
	}
	uint64_t filsiz;
	DWORD high, low;
	low = GetFileSize (handle, &high);
	filsiz = high;
	filsiz <<= 32;
	return (filsiz + low) ;
#else
	struct stat st;
	int ret = stat(filename.c_str (), &st);
	if (ret < 0) {
		std::string errmsg = fmt_string ("Can't not stat file %s.", filename.c_str ());
		THROW_XDELTA_EXCEPTION (errmsg);
	}
	return st.st_size;
#endif
}

uint64_t f_local_freader::get_file_size () const
{
#ifdef _WIN32
	uint64_t filsize = xdelta::get_file_size (f_handle_);
	return filsize;
#else
	return xdelta::get_file_size (f_name_);
#endif
}


void f_local_freader::close_file ()
{
#ifdef _WIN32
	if (f_handle_ != INVALID_HANDLE_VALUE) {
		::CloseHandle (f_handle_);
		f_handle_ = INVALID_HANDLE_VALUE;
	}
#else
	if (f_fd_ != -1) {
		close (f_fd_);
		f_fd_ = -1;
	}
#endif
}

f_local_freader::f_local_freader (const std::string & path, const std::string & fname) : 
#ifdef _WIN32
					f_handle_ (INVALID_HANDLE_VALUE), 
#else
					f_fd_ (-1), 
#endif
					f_name_ (path + "/" + fname),
					f_path_ (path),
					f_filename_ (fname)
{
}

f_local_freader::~f_local_freader ()
{
	close_file ();
}

uint64_t f_local_freader::seek_file (const uint64_t offset, const int whence)
{
#ifdef _WIN32
	return xdelta::seek_file (f_handle_, offset, whence, f_name_);
#else
	return xdelta::seek_file (f_fd_, offset, whence, f_name_);
#endif
}

//////////// f_local_fwriter


static std::string get_dirname (const std::string & pathname)
{
	size_t atn = pathname.length () - 1;
	while (atn >= 0) {
		if (pathname.at (atn) == '/' || pathname.at (atn) == '\\')
			return pathname.substr (0, atn);
		--atn;
	}

	return pathname;
}

static void create_directory (const std::string & path)
{
	if (!exist_file (path)) {
		int ret;
		create_directory (get_dirname (path));
#ifdef _WIN32
		ret = _mkdir (path.c_str ());
#else
		ret = mkdir (path.c_str (), S_IREAD | S_IWRITE);
#endif
		if (ret < 0 && errno != EEXIST) {
			std::string errmsg = xdelta::fmt_string ("Can't create directory %s(%s)."
											, path.c_str ()
											, xdelta::error_msg ().c_str ());
			THROW_XDELTA_EXCEPTION (errmsg);
		}
	}

	return;
}


void f_local_fwriter::open_file ()
{
#ifdef _WIN32
	if (f_handle_ != INVALID_HANDLE_VALUE)
		return;

	create_directory (get_dirname (f_name_));

	int flag = OPEN_EXISTING;
open_again:
	f_handle_ = ::CreateFileA (f_name_.c_str (),
							   GENERIC_WRITE, 
							   FILE_SHARE_READ | FILE_SHARE_WRITE,
							   0,
							   flag,
							   FILE_ATTRIBUTE_NORMAL,
							   0);

	if(f_handle_ == INVALID_HANDLE_VALUE) {
		UINT lasterror = GetLastError ();
		if (lasterror == ERROR_FILE_NOT_FOUND || lasterror == ERROR_PATH_NOT_FOUND) {
			flag = CREATE_NEW;
			goto open_again;
		}
		else {
			std::string errmsg = fmt_string ("Can't not open file %s.", f_name_.c_str ());
			THROW_XDELTA_EXCEPTION (errmsg);
		}
	}
#else
	if (f_fd_ > 0)
		return;
	int flag = O_WRONLY | O_BINARY;
	int mode = 0;
open_again:
	f_fd_ = open (f_name_.c_str (), flag, mode);
	if (f_fd_ < 0) {
		if (errno == ENOENT) {
			flag = O_CREAT | O_EXCL | O_RDWR | O_BINARY;
			mode = S_IREAD | S_IWRITE;
			goto open_again;
		}
		else {
			std::string errmsg = fmt_string ("Can't not open file %s.", f_name_.c_str ());
			THROW_XDELTA_EXCEPTION (errmsg);
		}
	}
#endif
}

int f_local_fwriter::write_file (const uchar_t * data, const uint32_t length)
{
	uint32_t len = length;
	if (data == 0) {
		std::string errmsg ("Parameter(s) not correct");
		THROW_XDELTA_EXCEPTION_NO_ERRNO (errmsg);
	}
	if (len == 0)
	    return 0;
	    
#ifdef _WIN32
	if (f_handle_ == INVALID_HANDLE_VALUE) {
		std::string errmsg ("File not open.");
		THROW_XDELTA_EXCEPTION_NO_ERRNO (errmsg);
	}

	uint32_t dwWritten;
	while (true) {
		int result = ::WriteFile (f_handle_, data, (unsigned) len, (LPDWORD)(&dwWritten), 0);
		//TODO: check the dwWritten
		if (!result) {
			std::string errmsg = fmt_string ("Can't not write file %s.", f_name_.c_str ());
			THROW_XDELTA_EXCEPTION (errmsg);
		}
		
		if (dwWritten == len) {
			break;
		}
		else {
			len -= dwWritten;
			data += dwWritten;
		}
	}

	return len;
#else
	if (f_fd_ == -1) {
		std::string errmsg ("File not open.");
		THROW_XDELTA_EXCEPTION_NO_ERRNO (errmsg);
	}
	return write (f_fd_, data, len);
#endif
}

uint64_t f_local_fwriter::get_file_size () const
{
#ifdef _WIN32
	uint64_t filsize = xdelta::get_file_size (f_handle_);
	return filsize;
#else
	return xdelta::get_file_size (f_name_);
#endif
}


void f_local_fwriter::close_file ()
{
#ifdef _WIN32
	if (f_handle_ != INVALID_HANDLE_VALUE) {
		::CloseHandle (f_handle_);
		f_handle_ = INVALID_HANDLE_VALUE;
	}
#else
	if (f_fd_ != -1) {
		close (f_fd_);
		f_fd_ = -1;
	}
#endif
}

uint64_t f_local_fwriter::seek_file (uint64_t offset, int whence)
{
#ifdef _WIN32
	return xdelta::seek_file (f_handle_, offset, whence, f_name_);
#else
	return xdelta::seek_file (f_fd_, offset, whence, f_name_);
#endif
}

f_local_fwriter::f_local_fwriter (const std::string & path, const std::string & fname) : 
#ifdef _WIN32
					f_handle_ (INVALID_HANDLE_VALUE), 
#else
					f_fd_ (-1), 
#endif
					f_name_ (path + "/" + fname),
					f_path_ (path),
					f_filename_ (fname)
{
}

f_local_fwriter::~f_local_fwriter ()
{
	close_file ();
}

bool f_local_fwriter::exist_file () const
{
	return xdelta::exist_file (f_name_);
}

void f_local_fwriter::set_file_size (uint64_t filsize)
{
	open_file ();
#ifdef _WIN32
	uint64_t offset = this->seek_file (filsize, FILE_BEGIN);
	if (offset != filsize) {
		std::string errmsg = fmt_string ("Can't seek file %s(%s)."
			, f_name_.c_str (), error_msg ().c_str ());
		THROW_XDELTA_EXCEPTION (errmsg);
	}
	BOOL success = SetEndOfFile (f_handle_);
#else
	int success = ftruncate (f_fd_, filsize);
	success = success == 0 ? 1 : 0;
#endif
	if (!success) {
		std::string errmsg = fmt_string ("Can't set file  %s's size.", f_name_.c_str ());
		THROW_XDELTA_EXCEPTION (errmsg);
	}	
}

} //namespace xdelta