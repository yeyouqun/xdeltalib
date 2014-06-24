/**
* 实现通过 SIMHASH 算法计算 Fingerprint。
*/

#ifndef __SIMHASH_H__
#define __SIMHASH_H__

namespace xdelta {

/// \class
/// HashType 是使用的 Hash 类型，hash_len 表示在计算 Hash 时的字串长度，一般为 2 ~ 6，
/// 过长的 hash_len 会导致计算结果失真。
template <typename HashType, int hash_len = 3>
class simhash {
public:
	simhash (file_reader * reader);
	///\brief
	/// 计算两个对象之间的汉明距离。
	uint32_t distance (const simhash<HashType, hash_len> & sh)
	{
	    return 0;
	}
private:
	file_reader & reader_;
};

}

#endif //__SIMHASH_H__
