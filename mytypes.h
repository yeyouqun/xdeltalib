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

#ifndef __MYTYPES_H__
#define __MYTYPES_H__

#include <exception>
#ifndef _WIN32
    #include <errno.h>
	extern int errno;
#endif
#include "platform.h"
/// @file
/// 类型及平台控制文件

namespace xdelta {

#define HAVE_UINT64 1


/*---------------------------------------------------------------------------*/
/*                                                                           */
/* Type Definition Macros                                                    */
/*                                                                           */
/*---------------------------------------------------------------------------*/
#ifdef _WIN32
	#if defined (_M_X64) || defined (_M_AMD64)
		#define __WORDSIZE 64
	#else
		#define __WORDSIZE 32
	#endif
#pragma warning (disable:4251)
#elif defined (_LINUX) || defined (_UNIX)
	#ifdef __GNUC__
		#if defined (__LP64__) || defined (_LP64)
			#define __WORDSIZE 64
		#else
			#define __WORDSIZE 32
		#endif
	#else
		#error Not support compiler.
	#endif
#else
	#error Current platform not support temporary.
#endif

#ifdef _WIN32
    struct iovec {
        void  *iov_base;
        size_t iov_len;
    };

    typedef int socklen_t;
	#define ssize_t size_t
#else
    typedef int            SOCKET;
#endif


typedef unsigned long long	uint64_t;
typedef long long			int64_t;

typedef unsigned int		uint32_t;
typedef int					int32_t;

typedef unsigned char		uchar_t;
typedef char				char_t;

typedef unsigned short		uint16_t;
typedef short				int16_t;

#ifndef TRUE
  #define TRUE 1
#endif

#ifndef FALSE
  #define FALSE 0
#endif

class DLL_EXPORT xdelta_exception : public ::std::exception
{
    std::string what_;
    int         errno_;
#ifndef NDEBUG
	std::string fname_;
	int			fline_;
#endif
public:
    xdelta_exception (const std::string & what, int error_no
#ifndef NDEBUG
					, const std::string & fname
					, int fline
#endif
		) : what_ (what), errno_ (error_no)
#ifndef NDEBUG
					, fname_ (fname)
					, fline_ (fline)
#endif
	{} 
    virtual ~xdelta_exception () throw () {}
    virtual const char* what() const throw()
	{
#ifndef NDEBUG
		const_cast<std::string&> (what_) = 
			fmt_string ("exception:%s, at file (%s), line (%d).", what_.c_str ()
			, fname_.c_str (), fline_).c_str ();
#endif
		return what_.c_str ();

	}
	int get_errno () const { return errno_; }
};

#ifndef NDEBUG
	#ifdef _WIN32
		#define THROW_XDELTA_EXCEPTION(errmsg) \
			throw xdelta::xdelta_exception ((errmsg), ::GetLastError (), __FILE__, __LINE__)
	#else
		#define THROW_XDELTA_EXCEPTION(errmsg) \
			throw xdelta::xdelta_exception ((errmsg), errno, __FILE__, __LINE__)
	#endif
	#define THROW_XDELTA_EXCEPTION_NO_ERRNO(errmsg) \
		throw xdelta::xdelta_exception ((errmsg), 0, __FILE__, __LINE__)
#else
	#ifdef _WIN32
		#define THROW_XDELTA_EXCEPTION(errmsg) \
			throw xdelta::xdelta_exception ((errmsg), ::GetLastError ())
	#else
		#define THROW_XDELTA_EXCEPTION(errmsg) \
			throw xdelta::xdelta_exception ((errmsg), errno)
	#endif
	#define THROW_XDELTA_EXCEPTION_NO_ERRNO(errmsg) \
			throw xdelta::xdelta_exception ((errmsg), 0)
#endif

#ifndef BYTE_ORDER
	# define LITTLE_ENDIAN 0x4321
	# define BIG_ENDIAN    0x1234

	#ifdef _WIN32
		# if defined (_M_AMD64) || defined (_M_IX86)
			#define BYTE_ORDER LITTLE_ENDIAN
		# else
			#define BYTE_ORDER BIG_ENDIAN
			#define WORDS_BIGENDIAN 1
		# endif
	#elif defined (_LINUX)
		#ifdef __ORDER_LITTLE_ENDIAN__ == __BYTE_ORDER__
			#define BYTE_ORDER LITTLE_ENDIAN
		#else
			#define BYTE_ORDER BIG_ENDIAN
			#define WORDS_BIGENDIAN 1
		#endif
	#elif defined (_UNIX)
		#if defined(__i386__) || defined (__amd64__)
			#define BYTE_ORDER LITTLE_ENDIAN
		#else
			#define BYTE_ORDER BIG_ENDIAN
			#define WORDS_BIGENDIAN 1
		#endif
	#endif
#endif

template<class obj_t>
static inline void delete_obj (obj_t * pobj)
{
	delete pobj;
}

#define BUG(mesg) do { \
		std::string errmsg = fmt_string ("BUG %s IN FILE %s, LINE %d." \
										, mesg, __FILE__, __LINE__);\
		THROW_XDELTA_EXCEPTION_NO_ERRNO (errmsg);\
}while (0)

} //namespace xdelta
#endif //__MYTYPES_H__
