/*---------------------------------------------------------------------------*/
/*                                                                           */
/* Socket.h - Passive Socket Decleration.                                    */
/*                                                                           */
/* Author : Mark Carrier (mark@carrierlabs.com)                              */
/* Modified by yeyouqun (yeyouqun@163.com) to implement xdeltalib's needs.	 */
/*                                                                           */
/*---------------------------------------------------------------------------*/
/* Copyright (c) 2007-2009 CarrierLabs, LLC.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * 4. The name "CarrierLabs" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    mark@carrierlabs.com.
 *
 * THIS SOFTWARE IS PROVIDED BY MARK CARRIER ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL MARK CARRIER OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *----------------------------------------------------------------------------*/
#ifndef __PASSIVESOCKET_H__
#define __PASSIVESOCKET_H__
#include "active_socket.h"

/// @file
/// 本文件中的类提供被动连接端的 Socket 类型定义及接口声明。

namespace xdelta {
/// Provides a platform independent class to create a passive socket.
/// A passive socket is used to create a "listening" socket.  This type
/// of object would be used when an application needs to wait for 
/// inbound connections.  Support for CSimpleSocket::SocketTypeTcp, 
/// CSimpleSocket::SocketTypeUdp, and CSimpleSocket::SocketTypeRaw is handled
/// in a similar fashion.  The big difference is that the method 
/// CPassiveSocket::Accept should not be called on the latter two socket
/// types. 
class DLL_EXPORT CPassiveSocket : public CSimpleSocket {
public:
    CPassiveSocket(bool compress, CSocketType type = SocketTypeTcp);
    virtual ~CPassiveSocket() { Close(); };

    /// Extracts the first connection request on the queue of pending 
    /// connections and creates a newly connected socket.  Used with 
    /// CSocketType CSimpleSocket::SocketTypeTcp.  It is the responsibility of
    /// the caller to delete the returned object when finished.
    ///  @return if successful a pointer to a newly created CActiveSocket object
    ///          will be returned and the internal error condition of the CPassiveSocket 
    ///          object will be CPassiveSocket::SocketSuccess.  If an error condition was encountered
    ///          the NULL will be returned and one of the following error conditions will be set:
    ///    CPassiveSocket::SocketEwouldblock, CPassiveSocket::SocketInvalidSocket, 
    ///    CPassiveSocket::SocketConnectionAborted, CPassiveSocket::SocketInterrupted
    ///    CPassiveSocket::SocketProtocolError, CPassiveSocket::SocketFirewallError
	//@yeyouqun: timeout_sec			0 is forever, other than timeout to accept a connection.
	//									if timeout, a 0 pointer return and GetSocketError
	//									return SocketTimedout.
    virtual CActiveSocket *Accept(uint32_t timeout_sec = 0);

    /// Create a listening socket at local ip address 'x.x.x.x' or 'localhost'
    /// if pAddr is NULL on port nPort.
    /// 
    ///  \param pAddr specifies the IP address on which to listen.
    ///  \param nPort specifies the port on which to listen.
    ///  \param nConnectionBacklog specifies connection queue backlog (default 30,000)
    ///  @return true if a listening socket was created.  
    ///      If not successful, the false is returned and one of the following error
    ///      condiitions will be set: CPassiveSocket::SocketAddressInUse, CPassiveSocket::SocketProtocolError, 
    ///      CPassiveSocket::SocketInvalidSocket.  The following socket errors are for Linux/Unix
    ///      derived systems only: CPassiveSocket::SocketInvalidSocketBuffer
    virtual bool Listen(const uchar_t *pAddr, int16_t nPort, int32_t nConnectionBacklog = 30000);
};

} //namespace xdelta

#endif // __PASSIVESOCKET_H__
