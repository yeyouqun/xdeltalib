/**
 * 实现通过 Rolling 算法计算 Fingerprint。
 */
#ifdef _WIN32
	#include <windows.h>
	#include <errno.h>
	#include <io.h>
	#include <share.h>
	#include <hash_set>
#else
	#if !defined (__CXX_11__)
		#include <ext/hash_set>
	#else
		#include <unordered_set>
	#endif
	#include <sys/types.h>
	#include <unistd.h>
	#include <sys/stat.h>
	#include <fcntl.h>
	#include <ext/functional>
	#include <memory.h>
	#include <stdio.h>
#endif

#include "BigIntegerLibrary.h"
#include "mytypes.h"
#include "platform.h"
#include "rw.h"
#include "buffer.h"
#include "rollsim.h"

namespace xdelta {
template <unsigned hashchars, unsigned prim
		, unsigned modvalue, unsigned selector>
BigUnsigned rollsim<hashchars, prim, modvalue, selector>::chararr_ = { 0 };

void init_char_array(BigUnsigned chararr[256], int prim, int hashchars)
{
	BigUnsigned exp = 1;
	for (int i = 0; i < hashchars - 1; ++i)
		exp *= prim;

	for (int i = 0; i < 256; ++i) {
		chararr[i] = exp * i;
	}
}

}
