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
#include <limits.h>

#ifndef __PLATFORM_H__
#define __PLATFORM_H__
/// @file
/// 类型及平台控制文件

namespace xdelta {
#ifdef _WIN32
	#define NAMESPACE_STD_BEGIN _STD_BEGIN
	#define NAMESPACE_STD_END	_STD_END
	#define hash_map			_STDEXT hash_map
	#define hash_set			_STDEXT hash_set
	
	#ifdef XDELTALIB_EXPORTS
	#define DLL_EXPORT				__declspec(dllexport)
	#else
	#define DLL_EXPORT				__declspec(dllimport)
	#endif
#else
	#define NAMESPACE_STD_BEGIN namespace std {
    #define NAMESPACE_STD_END	}
    #if !defined (__CXX_11__)
    	#define hash_map			__gnu_cxx::hash_map
		#define hash_set			__gnu_cxx::hash_set
    #else
    	#define hash_map			std::unordered_map
		#define hash_set			std::unordered_set
    #endif
	#define O_BINARY            0
	#define DLL_EXPORT
	#define FILE_BEGIN SEEK_SET
	#define FILE_CURRENT SEEK_CUR 
	#define FILE_END SEEK_END 
#endif


/// \fn std::string DLL_EXPORT fmt_string (const char * fmt, ...);
/// \brief 格式化错误信息。
/// \param[in] fmt   格式字串
/// \return 返回 string 表示的错误信息。
std::string DLL_EXPORT fmt_string (const char * fmt, ...);
/// \fn std::string DLL_EXPORT error_msg ();
/// \brief 返回当前错误的字串表示。
/// \return 返回当前错误的字串表示。
std::string DLL_EXPORT error_msg ();

/// \fn std::string get_tmp_fname (const std::string & fname);
/// \brief 从输入的文件名取得临时文件名。
/// \param[in] fname   输入的文件名。
/// \return 临时文件名。
std::string get_tmp_fname (const std::string & fname);

#ifdef _WIN32
#define SEPERATOR "\\"
#else
#define SEPERATOR "/"
#endif
/*---------------------------------------------------------------------------*/
/*                                                                           */
/* Socket Macros                                                             */
/*                                                                           */
/*---------------------------------------------------------------------------*/
#ifdef _WIN32
	#define SHUT_RD                0
	#define SHUT_WR                1
	#define SHUT_RDWR              2
	#define ACCEPT(a,b,c)          accept(a,b,c)
	#define CONNECT(a,b,c)         connect(a,b,c)
	#define CLOSE(a)               closesocket(a)
	#define READ(a,b,c)            read(a,b,c)
	#define RECV(a,b,c,d)          recv(a, (char *)b, c, d)
	#define RECVFROM(a,b,c,d,e,f)  recvfrom(a, (char *)b, c, d, (sockaddr *)e, (int *)f)
	#define RECV_FLAGS             MSG_WAITALL
	#define SELECT(a,b,c,d,e)      select((int32_t)a,b,c,d,e)
	#define SEND(a,b,c,d)          send(a, (const char *)b, (int)c, d)
	#define SENDTO(a,b,c,d,e,f)    sendto(a, (const char *)b, (int)c, d, e, f)
	#define SEND_FLAGS             0
	#define SENDFILE(a,b,c,d)      sendfile(a, b, c, d)
	#define SET_SOCKET_ERROR(x,y)  errno=y
	#define SOCKET_ERROR_INTERUPT  EINTR
	#define SOCKET_ERROR_TIMEDOUT  EAGAIN
	#define WRITE(a,b,c)           write(a,b,c)
	#define WRITEV(a,b,c)          Writev(b, c)
	#define GETSOCKOPT(a,b,c,d,e)  getsockopt(a,b,c,(char *)d, (int *)e)
	#define SETSOCKOPT(a,b,c,d,e)  setsockopt(a,b,c,(char *)d, (int)e)
	#define GETHOSTBYNAME(a)       gethostbyname((const char *)a)
#elif defined (_LINUX) || defined (_UNIX)
	#define ACCEPT(a,b,c)          accept(a,b,c)
	#define CONNECT(a,b,c)         connect(a,b,c)
	#define CLOSE(a)               close(a)
	#define READ(a,b,c)            read(a,b,c)
	#define RECV(a,b,c,d)          recv(a, (void *)b, c, d)
	#define RECVFROM(a,b,c,d,e,f)  recvfrom(a, (char *)b, c, d, (sockaddr *)e, f)
	#define RECV_FLAGS             MSG_WAITALL
	#define SELECT(a,b,c,d,e)      select(a,b,c,d,e)
	#define SEND(a,b,c,d)          send(a, (const char_t *)b, c, d)
	#define SENDTO(a,b,c,d,e,f)    sendto(a, (const char_t *)b, c, d, e, f)
	#define SEND_FLAGS             0
	#define SENDFILE(a,b,c,d)      sendfile(a, b, c, d)
	#define SET_SOCKET_ERROR(x,y)  errno=y
	#define SOCKET_ERROR_INTERUPT  EINTR
	#define SOCKET_ERROR_TIMEDOUT  EAGAIN
	#define WRITE(a,b,c)           write(a,b,c)
	#define WRITEV(a,b,c)          writev(a, b, c)
	#define GETSOCKOPT(a,b,c,d,e)  getsockopt((int)a,(int)b,(int)c,(void *)d,(socklen_t *)e)
	#define SETSOCKOPT(a,b,c,d,e)  setsockopt((int)a,(int)b,(int)c,(const void *)d,(int)e)
	#define GETHOSTBYNAME(a)       gethostbyname((const char *)a)
#endif


/*---------------------------------------------------------------------------*/
/*                                                                           */
/* File Macros                                                               */
/*                                                                           */
/*---------------------------------------------------------------------------*/
#define STRUCT_STAT         struct stat
#define LSTAT(x,y)          lstat(x,y)
#define FILE_HANDLE         FILE *
#define CLEARERR(x)         clearerr(x)
#define FCLOSE(x)           fclose(x)
#define FEOF(x)             feof(x)
#define FERROR(x)           ferror(x)
#define FFLUSH(x)           fflush(x)
#define FILENO(s)           fileno(s)
#define FOPEN(x,y)          fopen(x, y)
#define FREAD(a,b,c,d)      fread(a, b, c, d)
#define FSTAT(s, st)        fstat(FILENO(s), st)
#define FWRITE(a,b,c,d)     fwrite(a, b, c, d)
#define STAT_BLK_SIZE(x)    ((x).st_blksize)


/*---------------------------------------------------------------------------*/
/*                                                                           */
/* Misc Macros                                                               */
/*                                                                           */
/*---------------------------------------------------------------------------*/
#if defined(_WIN32)
  #define GET_CLOCK_COUNT(x) QueryPerformanceCounter((LARGE_INTEGER *)x)
#else
  #define GET_CLOCK_COUNT(x) gettimeofday(x, NULL)
#endif

#if defined(_WIN32)
  #define STRTOULL(x) _atoi64(x)
#else
  #define STRTOULL(x) strtoull(x, NULL, 10)
#endif

#if defined(_WIN32)
  #define SNPRINTF _snprintf
  #define PRINTF   printf
  #define VPRINTF  vprintf
  #define FPRINTF  fprintf
#else 
  #define SNPRINTF snprintf
  #define PRINTF   printf
  #define VPRINTF  vprintf
  #define FPRINTF  fprintf
#endif

} // namespace xdelta
#endif //__PLATFORM_H__

