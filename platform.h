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

#ifndef __PLATFORM_H__
#define __PLATFORM_H__
/// @file
/// 类型及平台控制文件

namespace xdelta {
#ifdef _WIN32
	#define NAMESPACE_STD_BEGIN _STD_BEGIN
	#define NAMESPACE_STD_END	_STD_END
	#define hash_map			_STDEXT hash_map
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
    #else
    	#define hash_map			std::unordered_map
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
} // namespace xdelta
#endif //__PLATFORM_H__

