/*
* Copyright (C) 2016- yeyouqun@163.com
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

#ifndef __XDELTALIB_C_API_H__
#define __XDELTALIB_C_API_H__
/**
 * 为了适应库在不同的存储平台上使用，库的内部与外部的通信采用匿名 PIPE 进行。所有的要计算的数据
 * 都从 PIPE 中输入到库中。
 *
 *   在使用本库前，请阅读本说明及各接口详细说明：
 *
 *	1. 角色定义：如果有文件 A 与 B，需要将 A 差异同步到 B，或者要计算 A 与 B 的差异数据，我们这里
 *		称 B 为目标文件，A 为源文件。
 *
 *  2. 在多轮同步或者差异计算中，会计算得出目标文件与源文件有多个相同的块，则这些相同的块会导致文件形成不同的“洞”，
 *		再依次对这些“洞”进行差异计算，如下示：
 *		a、开始时 A 文件与 B 文件计算，整个文件可以默认为一个从0到文件大小的洞，如有一块相同的块计算出为：
 *			源文件A：  |                           |SSSSSSSSSSSSSSS|                                               |
 *							                       ^
 *							           |-----------|
 *			目标文件B：|     |SSSSSSSSSSSSSSS|                                                                        |
 *		b、如计算结果如 a 中所示 S 块相同，则会形成如下的洞：
 *			源文件A：|           洞1             |SSSSSSSSSSSSSSS|          洞2                                  |
 *							                       ^
 *							           |-----------|
 *			目标文件B：| 洞3 |SSSSSSSSSSSSSSS|                                    洞4                                 |
 *		c、这里需要将“洞3”与“同4”再用更小的块计算哈希，并供传输给文件 A，同时对“洞1”与“洞2”进行差异计算，并将计算结果回传。
 *		d、不断计算，会形成更多更小的洞，直到最小的块达到，则停止计算。
 *		e、每一轮计算时，就可将计算结果回传，除了最后一轮会传递差异数据外，其他的轮都是传输的相同的数据块记录信息。
 *
 *  3. 对于就地生成文件，可能会导致同步结果的退化。想了解详细的信息，可以参考：
 *		In-Place Rsync: File Synchronization for Mobile and Wireless Devices，
 *				David Rasch and Randal Burns Department of Computer Science Johns Hopkins University {frasch,randalg}@cs.jhu.edu。
 *
 *	4. 多线程支持。接口中的所有接口都是线程安全的，你可以自己通过多线程的方法利用多核能力。
 */
 
#ifdef _WIN32
	#define PIPE_HANDLE HANDLE
#else
	#define PIPE_HANDLE int
#endif

#ifdef __cplusplus
extern "C"
{
#endif
	
	typedef struct file_hole {
		unsigned long long pos; // 洞在文件中的绝对位置。
		unsigned long long len; 			// 洞的长度。
		struct file_hole * next; // 相邻的下一个洞。
	}fh_t;
	
	typedef struct hash_item {
		unsigned fast_hash;
		unsigned char	 slow_hash[DIGEST_BYTES]; // 16 Bytes
		unsigned long long t_offset; //target offset;
		unsigned t_index; // block index, 基于 t_offset。之所以采用这么复杂的结构，是为了与多轮计算使用相同的代码。
		struct hash_item * next;
	}hit_t;

	#define DT_DIFF		((unsigned short)0x0)
	#define DT_IDENT	((unsigned short)0xffff)
	/**
	 *		数据类型为：DT_DIFF,DT_IDENT。
	 *			DT_DIFF：表示相应的数据块是差异的，长度为差异数据的长度。在构造新文件数据时，
	 *					 你应该从源数据文件的**源起始位置**开始读取指定长度的数据，并写入临
	 *					 时构造文件的 **源起始位置** 处。
	 *			DT_IDENT：表示相应的数据块是相同的，起始位置记录索引，长度统一为计算时所使用的块长度。
	 *					 在构造新文件数据时，你应该从计算原始块哈希值的文件（目标文件）**起始位置** 中读取数据,
	 *					 并写入临时构造文件的 **源起始位置**。
	 */
	typedef struct xdelta_item
	{
		unsigned short type;
		unsigned long long s_offset; // item 所指示的块在源文件中的位置，这个数据指示数据写入文件的位置。
		unsigned long long t_offset; // type == DT_DIFF，该值无意义，否则为 DT_IDENT 时，指的是目标文件的起始位置。
									 // 可以从该处读取 blklen 长度的数据来生成同步的临时文件。
		unsigned index;				 // 相对于 t_offset  的索引，索引大小为 blklen；
		unsigned blklen; 	// 数据块的长度。
		struct xdelta_item * next;
	}xit_t;

	inline unsigned long long get_target_offset (xit_t * head)
	{
		return head->t_offset + head->blklen * head->index;
	}
	
	/**
	 * 取得文件大小对应的快长度
	 * @filesize		对应文件大小。你也可以不采用这个接口来取得自己更合适的块大小。
	 */
	DLL_EXPORT unsigned xdelta_calc_block_len (unsigned long long filesize);
	
	/**
	 * 单轮计算时使用的接口。
	 */
	/**
	 * 开始一轮 HASH 计算。
	 * @blklen		计算哈希的块长度。最大的块长不能超过 MAX_XDELTA_BLOCK_BYTES(1mb)，最小不得小于 
	 *				XDELTA_BLOCK_SIZE(400) 字节。
	 * @return 	返回一个内部使用的数据结构的指针，并用在 calc_hash， get_hash_result_and_free_inner_data 接口中。
	 */
	DLL_EXPORT void * xdelta_start_hash (unsigned blklen);
	/**
	 * 使用如下接口计算指定数据流的快慢 HASH 值。
	 *
	 * @ptgthole	 所计算的哈希是属于哪个洞（目标文件洞）。如果是单轮差异计算，则这个洞就是整个文件（0，filesize）。
	 * @inner_data	 内部数据，在调用 get_hash_result_and_free_inner_data 时需要用到。
	 * @return		返回一个可以写的管道句柄。调用者可以利用这个句柄，将对应的洞的数据传递给库的接口。
	 *				调用都必须保证数据与洞的一一对应关系，否则会出现数据错误，或者未定义的行为。
	 */
	DLL_EXPORT PIPE_HANDLE xdelta_run_hash (fh_t * ptgthole, void * inner_data);
	
	/**
	 * 取得最近一次 xdelta_calc_hash 执行循环的执行结果。
	 * @inner_data	 	内部数据，调用时 calc_hash 产生。在接口返回后，这个对象将全部被释放，调用者不能再使用这个
	 *					对象进行接口调用。
	 * @return			哈希输出对象的链表头，这个参数需要传递给 start_xdelta  。
	 */
	DLL_EXPORT hit_t * xdelta_get_hashes_free_inner (void * inner_data);
	/**
	 * 释放哈希计算结果。
	 * @head		哈希结果头。
	 *				注意：由于哈希结果有可能比较大（视文件大小而定，如1TB的文件，可能有几百万个项），在使用完后
	 *					 应该释放这个结果链表。
	 */
	DLL_EXPORT void xdelta_free_hashes (hit_t * head);
	
	/**
	 * 针对每个文件计算差异数据时，必须按如下步骤进行：
	 *
	 *  @blklen		哈希块所对应的数据块的长度。内部需要用这个参数来计算数据移动窗口。这个参数值
	 *				必须与在同一轮中调用 xdelta_start_hash 时输出的参数一样。
	 *  @head		哈希数据块，xdelta_get_hash_result_and_free_inner_data 接口返回的数据 
	 *				在接口返回后，这个对象将全部被释放，调用者不能再使用这个
	 *				对象进行接口调用或者操作其中的数据。
	 *  @return		返回一个内部使用的数据结构的指针，并在 xdelta_run_xdelta， xdelta_get_xdelta_result_and_free_inner_data
	 *				接口使用。
	 *
	 */
	DLL_EXPORT void * xdelta_start_xdelta (hit_t * head, unsigned blklen);
	
	/**
	 * 当正确执行了 xdelta_start_xdelta 后，调用本接口。本接口用来执行差异数据计算。
	 * 
	 *  @srchole	源文件的洞，在多轮计算中，有可能源文件会生成多个洞。
	 *  @inner_data	内部数据，由 xdelta_start_xdelta 接口产生。
	 *  @return		返回一个用于向里写数所的句柄。使用者通过这个句柄将要分析的数据写入
	 *				到这个句柄中。写的数据长度必须是 srchole 的长度，否则要么是程序死锁，
	 *				会者会导致分析结果不正确。所以用户要保证数据长度的匹配。
	 */
	DLL_EXPORT PIPE_HANDLE xdelta_run_xdelta (fh_t * srchole, void * inner_data);

	/**
	 * 取得最近一次 xdelta_run_xdelta 执行循环的执行结果。
	 * @inner_data	 	内部数据，调用时 calc_hash 产生。在接口返回后，这个对象将全部被释放，调用者不能再使用这个
	 *					对象进行接口调用。
	 * @return			差异数据块输出对象的链表头，这个参数用户可用来进行新文件的构造或者其他用途。
	 */
	DLL_EXPORT xit_t * xdelta_get_xdeltas_free_inner (void * inner_data);
	/**
	 * 释放差异计算结果。
	 * @head		差异结果头。
	 *				注意：由于差异结果有可能比较大（视文件大小而定，如1TB的文件，可能有几百万个项），在使用完后
	 *					 应该释放这个结果链表。
	 */
	DLL_EXPORT void xdelta_free_xdeltas (xit_t * head);
	
	
	/********************************************* API 分隔 *********************************************************/
	/**
	 * 多轮计算的第一轮可以由上面的 “单轮计算时使用的接口” 所述的方式进行。单轮计算只是多轮计算中的一个特例。
	 * （单轮）多轮计算的计算流程如下：
	 * 
	 *  1. 初始化目标与源文件的洞为 0 到文件大小，即 (0, filesize)
	 *  2. 计算目标文件洞中不同块的哈希值 xdelta_run_hash
	 *  3. 取得哈希结果，调用 xdelta_start_xdelta，并传入哈希结果。
	 *  4. 对源文件的所有洞执行 xdelta_run_xdelta。
	 *  5. 取得执行结果，对结果进行应用。
	 *  6. 多轮计算中对目标文件与源文件执行 xdelta_divide_hole。然后回到 2。如果块已经最小了，就执行 5，然后结束。
	 *
	 *  伪代码如下：
	 *
	 * 			// initialize source file hole and target file hole as (0, source filesize) and (0, target filesize)
	 *			srchole = (0, source filesize);
	 *			tgthole = (0, target filesize);
	 *			blklen = origin_len;
	 *
	 *			tmpfile = create_a_temp_file ();
	 *			for (blklen >= minimal len) { // 当执行单轮计算时，将 minimal len 设定为 blklen。
	 *				void *inner_data = start_hash ();
	 *				for (each hole in tgthole) {
	 *					pipehandle = xdelta_run_hash (hole, blklen, inner_data);
	 *					write_file_data_in_this_hole_to_pipe (pipehandle);
	 *				}
	 *				hash_result = xdelta_get_hashes_free_inner (inner_data);
	 *
	 *				inner_data = xdelta_start_xdelta (hash_result);
	 *				xdelta_free_hashes (hash_result);
	 *	 
	 *				for (each hole in srchole) {
	 *					pipehandle = xdelta_run_xdeltas (blklen, hole, inner_data);
	 *					write_file_data_in_this_hole_to_pipe (pipehandle);
	 *				}
	 *				xdelta_result = xdelta_get_xdeltas_free_inner (inner_data);
	 *
	 *				for (item in xdelta_result) {
	 *					if (item.type == DT_IDENT) {
	 *						data = copy_from (tgtfile);
	 *						seek (tmpfile, block.s_offset);
	 *						write (tmpfile, data, block.len);
	 *						xdelta_divide_hole (tgthole, item.t_offset, item.blklen);
	 *						xdelta_divide_hole (srchole, item.s_offset, item.blklen);
	 *					}
	 *				}
	 *				xdelta_free_xdeltas (xdelta_result);
	 *
	 *				blklen = next_block_len; // next_block_len < blklen
	 *			}
	 * 
	 * 			for (item in xdelta_result) {
	 *				if (item.type == DT_DIFF) {
	 *					data = copy_from (srcfile);
	 *					seek (tmpfile, block.s_offset);
	 *					write (tmpfile, data, block.len);
	 *				}
	 *			}
	 *			close (tmpfile);
	 *				
	 */
	
	/**
	 * 分隔洞。
	 * @head			洞头，所有洞都通过单向链表连接，有可能值为变动。
	 * @pos				分隔的新位置
	 * @len				分隔的新长度。
	 * 					注意：分开有可能产生新的洞结构，新的洞结构体通过 malloc 来进行分配，因此调用者
	 *						要在结束时通过 free 对资源进行释放。用户构造的第一个洞，即全文件的洞，也需要用 malloc
	 *						进行分配，因为在调用这个接口的过程中，有可能会导致 head 被删除，这时就需要生成新的头
	 *						原来的头则通过 free 来释放。因为你需要保证最开始的 head 是通过 malloc 分配的。
	 *					注意：洞之间没有重叠部分，在拆分洞时，pos + len 这一长度的洞一定会处于现有的某个洞中，
	 *						  而不会只有部分重叠，出现这种情况，一定是发生了错误。
	 */
	DLL_EXPORT void xdelta_divide_hole (fh_t ** head, unsigned long long pos, unsigned len);
	
	/**
	 * 释放洞对象。
	 * @head 洞链表头。
	 */
	DLL_EXPORT void xdelta_free_hole (fh_t * head);
	 
	/********************************************* API 分隔 *********************************************************/
	/**
	 * Inplace 生成文件时的接口。因为就地造成与 Inplace 不同时应用，这个接口只针对同一轮计算结果进行。
	 */
	
	/**
	 * 解析就地生成时的 xdelta 结果。
	 *
	 * @head			xdelta 结果的表头。get_xdelta_result_and_free_inner_data 获取。
	 *					在调用这个接口后，*head 的值会有调整。按下面所述的过程来读写数据。
	 *					用完后，使用 free_xdelta_result 释放结果。
	 *
	 * 			当解析结束时，则对这个链表中的所有数据进行依次处理，处理为：
	 *				for (item in head list) {
	 *					if (link.item.type == DT_IDENT)
	 *						continue;
	 *					data = copy_from (srcfile);
	 *					seek (tmpfile, link.item.s_offset);
	 *					write (tmpfile, data, link.item.len);
	 *				}
	 */
	DLL_EXPORT void xdelta_resolve_inplace (xit_t ** head);
	
#ifdef __cplusplus
}
#endif
#endif //__XDELTALIB_C_API_H__