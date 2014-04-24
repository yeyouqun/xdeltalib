/*
 * MD5.H - header file for MD5.C
 */

/*
 * Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
 * rights reserved.
 *
 * License to copy and use this software is granted provided that it
 * is identified as the "RSA Data Security, Inc. MD5 Message-Digest
 * Algorithm" in all material mentioning or referencing this software
 * or this function.
 *
 * License is also granted to make and use derivative works provided
 * that such works are identified as "derived from the RSA Data
 * Security, Inc. MD5 Message-Digest Algorithm" in all material
 * mentioning or referencing the derived work.
 *
 * RSA Data Security, Inc. makes no representations concerning either
 * the merchantability of this software or the suitability of this
 * software for any particular purpose. It is provided "as is"
 * without express or implied warranty of any kind.
 *
 * These notices must be retained in any copies of any part of this
 * documentation and/or software.
 */

#ifndef _CRYPTO_MD5_H_
#define	_CRYPTO_MD5_H_

#include <sys/types.h>
namespace xdelta {
#define	MD5_DIGEST_LENGTH	16

/* MD5 context. */
typedef struct {
	unsigned int state[4];	/* state (ABCD) */
	unsigned int count[2];	/* number of bits, modulo 2^64 (lsb first) */
	unsigned char buffer[64];	/* input buffer */
} MD5_CTX;

extern void MD5Init(MD5_CTX *);
extern void MD5Update(MD5_CTX *, const void *, unsigned int);
extern void MD5Final(unsigned char [MD5_DIGEST_LENGTH], MD5_CTX *);

static inline void md5 (const char * buff, int len, char digest[16])
{
	MD5_CTX ctx;
	MD5Init (&ctx);
	MD5Update (&ctx, (unsigned char*)buff, len);
	MD5Final ((unsigned char *)digest, &ctx);
}
}
#endif /* _CRYPTO_MD5_H_ */

