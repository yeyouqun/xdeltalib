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
	/// \brief
	/// 计算两个对像之间的相似度，这个相似度不同时 rollsim 中所定义的 similarity = （A 交 B） / （A 并 B）
	/// 而是：Hash 长度 - 汉明距离与 Hash 长度之比。如汉明距离为 8，长度为 128，则相似度为：
	/// ((128 - 8) / 128 ) * 100%
	float similarity (const simhash<HashType, hash_len> & sh);
	///\brief
	/// 计算两个对象之间的没明距离。
	uint32_t distance (const simhash<HashType, hash_len> & sh);
private:
	char_buffer<char_t>	buff_;
	file_reader & reader_;
};

}

#endif //__SIMHASH_H__
