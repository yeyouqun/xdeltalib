// author:yeyouqun@163.com
/// @file
/// 声明网络数据传输基类。
namespace xdelta {

class xdelta_observer;

/// \class
/// Tcp 传送 hasher 数据的类型。
class tcp_hasher_stream : public hasher_stream
{
	virtual void add_block (const uint32_t fhash, const slow_hash & shash);
	virtual void end_hash_stream (const uchar_t file_hash[DIGEST_BYTES], const uint64_t filsize);
	virtual void set_holes (std::set<hole_t> * holeset) { assert (false); }
	virtual bool end_first_round (const uchar_t file_hash[DIGEST_BYTES]);
	virtual void next_round (const int32_t blk_len);
	virtual void end_one_round ();
	void _reconstruct_it ();
protected:
	virtual void on_error (const std::string & errmsg, const int errorno);
	virtual void start_hash_stream (const std::string & fname, const int32_t blk_len);
	void _streamize ();
	void _end_one_stage (uint16_t endtype, const uchar_t file_hash[DIGEST_BYTES]);
	void _receive_construct_data (reconstructor & reconst);

	CActiveSocket		&	client_;			///< 服务端网络对象。
	char_buffer<uchar_t>	header_buff_;		///< 块头缓冲。
	char_buffer<uchar_t>	data_buff_;			///< 数据缓冲。
	char_buffer<uchar_t>	stream_buff_;		///< 流化缓存，所有的数据先缓存在本缓冲中，满发送或者结束发送。
	xdelta_observer &		observer_;			///< 同步观察器。
	file_operator &			fop_;				///< 文件操作对象。
	std::string				filename_;			///< 当前处理的文件名。
	int32_t					blk_len_;			///< 当前块长度。
	int32_t					error_no_;			///< 执行过程中的错误码。
	bool					multiround_;		///< 多轮 Hash 同步。
	bool					inplace_;			///< 就地构造文件，不能与 multiround_ 同时为真。
public:
	tcp_hasher_stream (CActiveSocket & client
					, file_operator & fop
					, xdelta_observer & observer
					, bool inplace = false);
	~tcp_hasher_stream () {}
};

/// \class
/// Tcp 传送多轮 hasher 数据的类型。
class multiround_tcp_stream : public tcp_hasher_stream
{
	virtual void start_hash_stream (const std::string & fname, const int32_t blk_len);
	virtual void end_hash_stream (const uchar_t file_hash[DIGEST_BYTES], const uint64_t filsize);
	virtual void set_holes (std::set<hole_t> * holeset);
	virtual bool end_first_round (const uchar_t file_hash[DIGEST_BYTES]);
	virtual void next_round (const int32_t blk_len);
	virtual void end_one_round ();
private:
	bool _end_first_round;
	bool _receive_equal_node ();
	multiround_reconstructor reconst_;
public:
	multiround_tcp_stream (CActiveSocket & client
					, file_operator & fop
					, xdelta_observer & observer);
	~multiround_tcp_stream () {}
};

/// \var MULTI_ROUND_FLAG
/// 多轮标志字节常量
extern const uint16_t MULTI_ROUND_FLAG;
/// \var SINGLE_ROUND_FLAG
/// 单轮标志字节常量
extern const uint16_t SINGLE_ROUND_FLAG;
/// \var INPLACE_FLAG
/// 就地构造标志字节常量
extern const uint16_t INPLACE_FLAG;

} // namespace xdelta