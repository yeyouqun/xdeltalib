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
#include <set>
#include <string>
#include <list>
#include <algorithm>

#ifdef _WIN32
	#include <windows.h>
	#include <functional>
	#include <hash_map>
	#include <errno.h>
#else
	#include <unistd.h>
	#include <memory>
	#include <ext/functional>
    #if !defined (__CXX_11__)
    	#include <ext/hash_map>
    #else
    	#include <unordered_map>
    #endif
    #include <memory.h>
    #include <stdio.h>
#endif
#include <assert.h>

#include "mytypes.h"
#include "digest.h"
#include "rw.h"
#include "rollsum.h"
#include "buffer.h"
#include "xdeltalib.h"
#include "reconstruct.h"

namespace xdelta {
void reconstructor::start_hash_stream (const std::string & fname
								, const int32_t blk_len)
{
	reader_ = foperator_.create_reader (fname);
	if (!reader_->exist_file ()) {
		foperator_.release (reader_);
		reader_ = 0;
		writer_ = foperator_.create_writer (fname);
	}
	else {
		reader_->open_file ();
		ftmpname_ = get_tmp_fname (fname);
		writer_ = foperator_.create_writer (ftmpname_);
	}

	if (writer_ == 0) {
		std::string errmsg = fmt_string ("Can't create file %s."
										, fname.c_str ());
		THROW_XDELTA_EXCEPTION_NO_ERRNO (errmsg);
	}

	writer_->open_file ();
	fname_ = fname;
}

void reconstructor::add_block (const target_pos & tpos
							, const uint32_t blk_len
							, const uint64_t s_offset)
{
	if (reader_ == 0) {
		std::string errmsg = fmt_string ("Original file %s not open.", fname_.c_str ());
		THROW_XDELTA_EXCEPTION_NO_ERRNO (errmsg);
	}

	uint64_t t_offset = tpos.t_offset + tpos.index * blk_len;
	uint64_t offset = reader_->seek_file (t_offset, FILE_BEGIN);
	if (offset != t_offset) {
		std::string errmsg = fmt_string ("Can't seek file %s(%s)."
										, fname_.c_str ()
										, error_msg ().c_str ());
		THROW_XDELTA_EXCEPTION (errmsg);
	}

	uint32_t ret = reader_->read_file (buff_.begin (), blk_len);
	if (ret < blk_len) {
		std::string errmsg = fmt_string ("Can't read file %s(%s)."
										, fname_.c_str ()
										, error_msg ().c_str ());
		THROW_XDELTA_EXCEPTION (errmsg);
	}

	offset = writer_->seek_file (s_offset, FILE_BEGIN);
	if (offset != s_offset) {
		std::string errmsg = fmt_string ("Can't seek file %s(%s)."
										, fname_.c_str ()
										, error_msg ().c_str ());
		THROW_XDELTA_EXCEPTION (errmsg);
	}

	ret = writer_->write_file (buff_.begin (), blk_len);
	if (ret < blk_len) {
		std::string errmsg = fmt_string ("Can't write file %s(%s)."
										, fname_.c_str ()
										, error_msg ().c_str ());
		THROW_XDELTA_EXCEPTION (errmsg);
	}
}

void reconstructor::add_block (const uchar_t * data, const uint32_t blk_len, const uint64_t s_offset)
{
	int ret = writer_->write_file (data, blk_len);
	if (ret < 0) {
		std::string errmsg = fmt_string ("Can't not write file %s(%s)."
										, fname_.c_str ()
										, error_msg ().c_str ());
		THROW_XDELTA_EXCEPTION (errmsg);
	}
}

void reconstructor::end_hash_stream (const uint64_t filsize)
{
	if (reader_) {
		reader_->close_file ();
		foperator_.release (reader_);
	}
	if (writer_) {
		writer_->close_file ();
		foperator_.release (writer_);
	}

	if (!ftmpname_.empty ())
		foperator_.rename (ftmpname_, fname_);

	fname_.clear ();
	ftmpname_.clear ();
}

void reconstructor::on_error (const std::string & errmsg, const int errorno)
{
	if (errorno != 0 && errorno == ENOENT
#ifdef _WIN32
		|| errorno == ERROR_FILE_NOT_FOUND
		|| errorno == ERROR_PATH_NOT_FOUND
#endif
		) {
		return;
	}

	if (reader_)
		reader_->close_file ();
	if (writer_)
		writer_->close_file ();
	if (!ftmpname_.empty ())
		foperator_.rm_file (ftmpname_);
}

} //namespace xdelta

