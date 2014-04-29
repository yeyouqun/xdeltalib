#include <string> 
#ifdef _WIN32 
	#include <windows.h> 
	#include <errno.h> 
	#include <io.h> 
	#include <share.h> 
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
#include "mytypes.h " 
#include "platform.h " 
#include "rw.h " 
#include "buffer.h " 
#include "fuzzy.h " 
#include "fuzzysim.h " 


namespace xdelta {


struct auto_fuzzy_state { 
	fuzzy_state   * ctx ; 
	auto_fuzzy_state (fuzzy_state * ctx ) : ctx (ctx ) { } 
	 ~auto_fuzzy_state () 
	 { 
	 	if (ctx) fuzzy_free (ctx) ; 
	 } 
}; 



int fuzzy::similarity (fuzzy & f) 
{ 
	std::string result = f.calc_digest (); 
	std::string myresult = calc_digest (); 
	return fuzzy_compare (result.c_str (), myresult.c_str ()); 
} 


std::string fuzzy::calc_digest () 
{
	fuzzy_state * ctx = fuzzy_new () ; 
	auto_fuzzy_state afs (ctx) ; 
	if (ctx == 0) 
		 THROW_XDELTA_EXCEPTION_NO_ERRNO ("Out of memory") ; 

	reader_.open_file () ; 
	while (true ) { 
		 int bytes = reader_.read_file (buff_.begin (), (uint32_t)buff_.size ()); 
		 if (bytes == 0 ) 
		 	 break ; 

		 if (0 != fuzzy_update (afs.ctx, buff_.begin (), bytes)) 
		 	 THROW_XDELTA_EXCEPTION ("fuzzy update failed.") ; 
	} 
	char result [FUZZY_MAX_RESULT]; 
	if (0 != fuzzy_digest (afs.ctx, result, FUZZY_FLAG_ELIMSEQ)) 
		 THROW_XDELTA_EXCEPTION ("fuzzy digest failed.");
		 
	reader_.close_file () ; 
	return std::string(result) ; 
} 

} 
