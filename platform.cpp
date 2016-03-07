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

#include <string>
#include <stdio.h>
#include <stdarg.h>
#include <vector>

#ifdef _WIN32
	#include <windows.h>
	#include <functional>
	#include <hash_map>
	#include <io.h>
	#include <time.h>
#else
	#include <unistd.h>
	#include <memory>
	#include <ext/functional>
    #if !defined (__CXX_11__)
    	#include <ext/hash_map>
    #else
    	#include <unordered_map>
    #endif
    #include <errno.h>
    #include <string.h>
#endif


#include "mytypes.h"
#include "platform.h"

namespace xdelta {

std::string fmt_string (const char * fmt, ...)
{
	std::vector<char> buf (512);

	va_list vl;
    va_start (vl, fmt);
	while (true) {
#ifdef _WIN32
		int nsize = vsnprintf_s (&buf[0], buf.size (), _TRUNCATE, fmt, vl);
#else
		int nsize = vsnprintf (&buf[0], buf.size (), fmt, vl);
#endif
		if (buf.size () <= (size_t)nsize) { //fail delete buffer and try again
			buf.resize (buf.size () * 2);
			continue;
		}
		va_end(vl);
		break;
	}

    return std::string (&buf[0]);
}

std::string error_msg ()
{
#ifdef _WIN32
	char buf[512];
	strerror_s (buf, 512, errno);
	return std::string (buf);
#else
	return std::string (strerror (errno));
#endif
}

std::string get_tmp_fname (const std::string & fname)
{
	//
	// 这里要特别注意，不能这么来写代码：
	// temp = "xdeltaXXXXXX";
	// char * result = _mktemp (temp);
	// 上面的代码会有 Bug，因为 temp 是一个常量字符，编译器可能会把它放在 ro.data 的段中
	// 而 _mktemp 会修改这个变量，则会在这里产生一个权限不匹配的问题。
	//
	char temp[13];
	strcpy (temp, "xdeltaXXXXXX");

#ifdef _WIN32
	char * result = _mktemp (temp);
	if (result == 0)
		return fname + "-" + fmt_string ("xdelta-%lld", time (0));
	return fname + "-" + temp;
#else
	int result = mkstemp (temp); 
	if (result == 0)
		return fname + "-" + temp;
	return fname + "-" + fmt_string ("xdelta-%lld", time (0));
#endif
}

} // namespace xdelta

