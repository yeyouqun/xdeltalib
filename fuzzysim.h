/**
* 实现通过 SIMHASH 算法计算 Fingerprint。
*/
#ifndef __FUZZY_HASH_SIM_H__
#define __FUZZY_HASH_SIM_H__

namespace xdelta {
class fuzzy {
public:
	fuzzy (file_reader & reader) : reader_ (reader), buff_ (BUFLEN) {}
	/// \brief
	/// 返回两个对象之间的相似度，0 ~ 100，当为 100 时，表示完全相似。
	int similarity (fuzzy & f);
private:
	enum { BUFLEN = 10 * 1024 * 1024, }; // 10MB
	std::string calc_digest ();
	char_buffer<uchar_t>	buff_;
	file_reader & reader_;
};

}

#endif //__FUZZY_HASH_SIM_H__