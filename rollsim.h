/**
* 实现通过 Rolling 算法计算 Fingerprint。
*/
#ifndef __ROLLSIM_H__
#define __ROLLSIM_H__

namespace xdelta {
void init_char_array(BigUnsigned chararr[256], int prim, int hashchars);
template <unsigned hashchars = 40
		, unsigned prim = 3
		, unsigned modvalue = (1 << 30)
		, unsigned selector = 0x00000001>
class rollsim {
	enum { BUFLEN = 10 * 1024 * 1024, }; // 10MB
	struct init_chararr { init_chararr () { init_char_array (chararr_, prim, hashchars); } };
public:
	rollsim (file_reader & reader) : reader_ (reader)
	{
		 // just to inistilize charvalue * prim ^ (hashchars - 1)
		static const init_chararr ic;
	}
	/// \brief
	float similarity (rollsim<hashchars, prim, modvalue, selector> & compobj)
	{
		hash_set<unsigned> h1;
		hash_set<unsigned> h2;
		calc_fingerprints (h1);
		compobj.calc_fingerprints (h2);
		unsigned count = 0;
		for (hash_set<unsigned>::iterator begin = h1.begin ();
		    begin != h1.end (); ++begin) {
		    if (h2.find (*begin) != h2.end ())
		        ++count;
		}

		size_t sum = h1.size () + h2.size () - count;
		if (sum == 0)
			return 0.0;

		return 	((float)count/sum);
	}
private:
	/// \brief
	BigUnsigned horner(const char * buff)
	{
		BigUnsigned hv = 0;
		for (int i = 0; i < hashchars; ++i) {
			BigUnsigned t1 = (((int)buff[i]) & 0x000000ff);
			hv = (hv * prim + t1);
		}

		return hv;
	}
	/// \brief
	void calc_fingerprints(hash_set<unsigned> & fps)
	{
		reader_.open_file ();
		bool starthash = true;
		BigUnsigned figervalue = 0;
		int pos = 0, toread = BUFLEN;

	    char_buffer<char_t>	buffer (BUFLEN);
		char * buff = buffer.begin ();
		do {
			char buff[BUFLEN];
			const int bytes = reader_.read_file (buff.begin () + pos, toread);
			if (bytes <= 0)
				break;

			int len = bytes > hashchars ? hashchars : bytes;

			if (starthash) {
				figervalue = horner (&buff[0], len);
				unsigned checksum = (figervalue % modvalue).toUnsignedInt();
				starthash = false;
				if ((checksum & selector) == selector)
					fps.insert(checksum);
			}

			int prev = 0;
			int i = 0;
			for (i = len; i < bytes; ++i) {
				int charidx = (int)(buff[prev++]);
				charidx &= 0x000000ff;
				BigUnsigned t50 = (((int)buff[i]) & 0x000000ff);
				figervalue -= chararr_[charidx]/*val * p ^ (len - 1)*/;
				figervalue *= prim;
				figervalue += t50;
				unsigned checksum = (figervalue % modvalue).toUnsignedInt();
				if ((checksum & selector) == selector)
					fps.insert(checksum);
			}
			if (buff != &buff[prev])
				memmove((void*)buff, &buff[prev], i - prev);

			pos = i - prev;
			toread = BUFLEN - pos;
		} while (true);

		return;
	}
private:// data members
	file_reader & reader_;
	static BigUnsigned chararr_[256];
};

}

#endif //__ROLLSIM_H__
