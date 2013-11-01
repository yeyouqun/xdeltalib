// authour:yeyouqun@163.com

#include <string>
#include <stdio.h>
#include <stdarg.h>
#include <vector>

#ifdef _WIN32
	#include <windows.h>
	#include <functional>
	#include <hash_map>
#else
	#include <unistd.h>
	#include <memory>
	#include <ext/functional>
    #if !defined (__CXX_11__)
    	#include <ext/hash_map>
    #else
    	#include <unordered_map>
    #endif
    #include <error.h>
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
#ifdef _WIN32
	char tmp[MAX_PATH + 1];
	const char * strp;
	GetTempFileNameA (".", "xdelta", 1, tmp);
	if (strp = strstr (tmp, ".\\"))
		return fname + "-" + (tmp + 2);
	return fname + "-" + tmp;
#else
	do {
		char * tmp = "XXXXXX";
		tmp = mktemp (tmp); 
		if (strlen (tmp) != 0)
			return fname + "-" + tmp;
	} while (1);

#endif
}

} // namespace xdelta