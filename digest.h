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
#ifndef __DIGEST_H__
#define __DIGEST_H__

/// \file
/// \brief
/// 本文件提供基本的 MD4 Hash 算法接口。

namespace xdelta {

/// \def MD4_BLOCK_LENGTH
/// MD4 摘要计算块长度
#define	MD4_BLOCK_LENGTH		64
/// \def MD4_DIGEST_LENGTH
/// MD4 摘要长度
#define	MD4_DIGEST_LENGTH		16
/// \def MD4_DIGEST_LENGTH
/// MD4 摘要长度
#define DIGEST_BYTES			MD4_DIGEST_LENGTH


typedef struct rs_mdfour {
    int                 A, B, C, D;
#if HAVE_UINT64
    uint64_t            totalN;
#else
    uint32_t            totalN_hi, totalN_lo;
#endif
    int                 tail_len;
    uchar_t       tail[64];
}rs_mdfour_t;

/// \fn void DLL_EXPORT rs_mdfour(uchar_t *out, void const *in, size_t);
/// \brief 计算数据块的 MD 值。
/// \param[in] out MD4 输出缓存
/// \param[in] in  输入数据块指针。
/// \param[in] bytes  数据块长度。
/// \return 无返回
void DLL_EXPORT rs_mdfour(uchar_t *out, void const *in, size_t bytes);

/// \fn void DLL_EXPORT rs_mdfour_begin(rs_mdfour_t * md);
/// \brief 初始化 MD4 上下文。
/// \param[out] md MD4 计算上下文件
/// \return 无返回
void DLL_EXPORT rs_mdfour_begin(rs_mdfour_t * md);
/// \fn void void DLL_EXPORT rs_mdfour_update(rs_mdfour_t * md, void const * buf,size_t n);
/// \brief 将数据块中的数据写入到 MD 上下文中。
/// \param[in] md   MD4 计算上下文件。
/// \param[in] buf	数据块指针。
/// \param[in] n	数据块长度。
/// \return 无返回
void DLL_EXPORT rs_mdfour_update(rs_mdfour_t * md, void const * buf,size_t n);
/// \fn rs_mdfour_result(rs_mdfour_t * md, uchar_t *out);
/// \brief 输出 MD44 上下文件中的 MD4 值。
/// \param[in] md   MD4 计算上下文。
/// \param[out] out  输出缓冲。
/// \return 无返回
void DLL_EXPORT rs_mdfour_result(rs_mdfour_t * md, uchar_t *out);

/// \fn void get_slow_hash (const uchar_t *buf1, uint32_t len, uchar_t hash[DIGEST_BYTES])
/// \brief 计算数据块的慢 Hash 值。
/// \param[in] buf1 数据块指针。
/// \param[in] len  块长度。
/// \param[out] hash  Hash 输出缓冲区。
/// \return 无返回
inline void get_slow_hash (const uchar_t *buf1, uint32_t len, uchar_t hash[DIGEST_BYTES])
{
	rs_mdfour (hash, buf1, len);
}

} // namespace xdelta

#endif //__DIGEST_H__

