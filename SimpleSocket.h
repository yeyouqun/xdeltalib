/*---------------------------------------------------------------------------*/
/*                                                                           */
/* SimpleSocket.h - Simple Socket base class decleration.                    */
/*                                                                           */
/* Author : Mark Carrier (mark@carrierlabs.com)                              */
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
#ifndef __SOCKET_H__
#define __SOCKET_H__

#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>

#if defined(_LINUX) || defined (_DARWIN)
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <netinet/tcp.h>
	#include <netinet/ip.h>
	#include <netdb.h>
#endif
#ifdef _LINUX
	#include <linux/if_packet.h>
	#include <linux/if_packet.h>
	#include <linux/if_ether.h>
	#include <linux/if.h>
	#include <sys/sendfile.h>
#endif
#if defined(_LINUX) || defined (_DARWIN)
	#include <sys/time.h>
	#include <sys/uio.h>
	#include <unistd.h>	
	#include <fcntl.h>
#elif defined (_WIN32)
	#include <io.h>
	#include <winsock2.h>
	#include <Ws2tcpip.h>

	#define IPTOS_LOWDELAY  0x10  /// 

#endif

#include "Host.h"

/// @file
/// 本文件中的类提供 Socket 基础类声明及接口。

namespace xdelta {
//-----------------------------------------------------------------------------
// General class macro definitions and typedefs
//-----------------------------------------------------------------------------
#ifndef INVALID_SOCKET
  #define INVALID_SOCKET    ~(0)
#endif

/// Provides a platform independent class to for socket development.
/// This class is designed to abstract socket communication development in a 
/// platform independent manner. 
/// - Socket types
///  -# CActiveSocket Class
///  -# CPassiveSocket Class
class DLL_EXPORT CSimpleSocket {
public:
    /// Defines the three possible states for shuting down a socket.
    typedef enum 
    {
        Receives = SHUT_RD, ///< Shutdown passive socket.
        Sends = SHUT_WR,    ///< Shutdown active socket.
        Both = SHUT_RDWR    ///< Shutdown both active and passive sockets.
    } CShutdownMode; 

    /// Defines the socket types defined by CSimpleSocket class.
    typedef enum  
    {
        SocketTypeInvalid,   ///< Invalid socket type.
        SocketTypeTcp,       ///< Defines socket as TCP socket.
        SocketTypeUdp,       ///< Defines socket as UDP socket.
        SocketTypeTcp6,      ///< Defines socket as IPv6 TCP socket.
        SocketTypeUdp6,      ///< Defines socket as IPv6 UDP socket.
        SocketTypeRaw        ///< Provides raw network protocol access.
    } CSocketType;

    /// Defines all error codes handled by the CSimpleSocket class.
    typedef enum 
    {
        SocketError = -1,          ///< Generic socket error translates to error below.
        SocketSuccess = 0,         ///< No socket error.
        SocketInvalidSocket,       ///< Invalid socket handle.
        SocketInvalidAddress,      ///< Invalid destination address specified.
        SocketInvalidPort,         ///< Invalid destination port specified.
        SocketConnectionRefused,   ///< No server is listening at remote address.
        SocketTimedout,            ///< Timed out while attempting operation.
        SocketEwouldblock,         ///< Operation would block if socket were blocking.
        SocketNotconnected,        ///< Currently not connected.
        SocketEinprogress,         ///< Socket is non-blocking and the connection cannot be completed immediately
        SocketInterrupted,         ///< Call was interrupted by a signal that was caught before a valid connection arrived.
        SocketConnectionAborted,   ///< The connection has been aborted.
        SocketProtocolError,       ///< Invalid protocol for operation.
        SocketFirewallError,       ///< Firewall rules forbid connection.
        SocketInvalidSocketBuffer, ///< The receive buffer point outside the process's address space.
        SocketConnectionReset,     ///< Connection was forcibly closed by the remote host.
        SocketAddressInUse,        ///< Address already in use.
        SocketInvalidPointer,      ///< Pointer type supplied as argument is invalid.
        SocketEunknown             ///< Unknown error please report to mark@carrierlabs.com
    } CSocketError;

public:
    CSimpleSocket(CSocketType type = SocketTypeTcp);

    virtual ~CSimpleSocket() 
	{
	};

    /// Initialize instance of CSocket.  This method MUST be called before an
    /// object can be used. Errors : CSocket::SocketProtocolError, 
    /// CSocket::SocketInvalidSocket,
    /// @return true if properly initialized.
    virtual bool Initialize(void);

	/// Close socket
	/// @return true if successfully closed otherwise returns false.
	virtual bool Close(void);

	/// Shutdown shut down socket send and receive operations
	///	CShutdownMode::Receives - Disables further receive operations.
	///	CShutdownMode::Sends    - Disables further send operations.
	///	CShutdownBoth::         - Disables further send and receive operations.
	/// \param nShutdown specifies the type of shutdown. 
	/// @return true if successfully shutdown otherwise returns false.
	virtual bool Shutdown(CShutdownMode nShutdown);

    /// Examine the socket descriptor sets currently owned by the instance of
    /// the socket class (the readfds, writefds, and errorfds parameters) to 
    /// see whether some of their descriptors are ready for reading, are ready 
    /// for writing, or have an exceptional condition pending, respectively.
	/// Block until an event happens on the specified file descriptors.
    /// @return true if socket has data ready, or false if not ready or timed out.
    virtual bool Select(void) { return Select(0,0); };

    /// Examine the socket descriptor sets currently owned by the instance of
    /// the socket class (the readfds, writefds, and errorfds parameters) to 
    /// see whether some of their descriptors are ready for reading, are ready 
    /// for writing, or have an exceptional condition pending, respectively.
	/// \param nTimeoutSec timeout in seconds for select.
    /// \param nTimeoutUSec timeout in micro seconds for select.
    /// @return true if socket has data ready, or false if not ready or timed out.
    virtual bool Select(int32_t nTimeoutSec, int32_t nTimeoutUSec);

    /// Does the current instance of the socket object contain a valid socket
    /// descriptor.
    ///  @return true if the socket object contains a valid socket descriptor.
    virtual bool IsSocketValid(void) { return (m_socket != SocketError); };

    /// Provides a standard error code for cross platform development by
    /// mapping the operating system error to an error defined by the CSocket
    /// class.
    void TranslateSocketError(void);

    /// Attempts to receive a block of data on an established connection.	
    /// \param nMaxBytes maximum number of bytes to receive.
    /// @return number of bytes actually received.
	/// @return of zero means the connection has been shutdown on the other side.
	/// @return of -1 means that an error has occurred.
    virtual int32_t Receive(char_buffer<uchar_t> & buff, int32_t nMaxBytes);

    /// Attempts to send a block of data on an established connection.
    /// \param pBuf block of data to be sent.
    /// \param bytesToSend size of data block to be sent.
    /// @return number of bytes actually sent.
	/// @return of zero means the connection has been shutdown on the other side.
	/// @return of -1 means that an error has occurred.
    virtual int32_t Send(const uchar_t *pBuf, int32_t bytesToSend);

    /// Attempts to send at most nNumItem blocks described by sendVector 
    /// to the socket descriptor associated with the socket object.
    /// \param sendVector pointer to an array of iovec structures
    /// \param nNumItems number of items in the vector to process
    /// <br>\b NOTE: Buffers are processed in the order specified.
    /// @return number of bytes actually sent, return of zero means the
    /// connection has been shutdown on the other side, and a return of -1
    /// means that an error has occurred.
    virtual int32_t Send(const struct iovec *sendVector, int32_t nNumItems);

    /// Returns blocking/non-blocking state of socket.
    /// @return true if the socket is non-blocking, else return false.
    bool IsNonblocking(void) { return (m_bIsBlocking == false); };

    /// Controls the actions taken when CSimpleSocket::Close is executed on a 
    /// socket object that has unsent data.  The default value for this option 
    /// is \b off.
    /// - Following are the three possible scenarios.
    ///  -# \b bEnable is false, CSimpleSocket::Close returns immediately, but 
    ///  any unset data is transmitted (after CSimpleSocket::Close returns)
    ///  -# \b bEnable is true and \b nTime is zero, CSimpleSocket::Close return 
    /// immediately and any unsent data is discarded.
    ///  -# \b bEnable is true and \b nTime is nonzero, CSimpleSocket::Close does
    ///  not return until all unsent data is transmitted (or the connection is 
    ///  Closed by the remote system).
    /// <br><p>
    /// \param bEnable true to enable option false to disable option.
    /// \param nTime time in seconds to linger.
    /// @return true if option successfully set
    bool SetOptionLinger(bool bEnable, uint16_t nTime);

    /// Tells the kernel that even if this port is busy (in the TIME_WAIT state),
    /// go ahead and reuse it anyway.  If it is busy, but with another state, 
    /// you will still get an address already in use error.
    /// @return true if option successfully set
    bool SetOptionReuseAddr();

    /// Enable/disable multicast for a socket.  This options is only valid for
    /// socket descriptors of type CSimpleSocket::SocketTypeUdp.
    /// @return true if multicast was enabled or false if socket type is not
    /// CSimpleSocket::SocketTypeUdp and the error will be set to 
    /// CSimpleSocket::SocketProtocolError 
    bool SetMulticast(bool bEnable, uchar_t multicastTTL = 1);

    /// Return true if socket is multicast or false is socket is unicast
    /// @return true if multicast is enabled
    bool GetMulticast() { return m_bIsMulticast; };

    /// Returns the last error that occured for the instace of the CSimpleSocket
    /// instance.  This method should be called immediately to retrieve the 
    /// error code for the failing mehtod call.
    ///  @return last error that occured.
    CSocketError GetSocketError(void) { return m_socketErrno; };

	/// Return Differentiated Services Code Point (DSCP) value currently set on the socket object.
    /// @return DSCP for current socket object.
    /// <br><br> \b NOTE: Windows special notes http://support.microsoft.com/kb/248611.
	int GetSocketDscp(void);

	/// Set Differentiated Services Code Point (DSCP) for socket object.
	///  \param nDscp value of TOS setting which will be converted to DSCP
	///  @return true if DSCP value was properly set
    /// <br><br> \b NOTE: Windows special notes http://support.microsoft.com/kb/248611.
	bool SetSocketDscp(int nDscp);

	/// Return socket descriptor
	///  @return socket descriptor which is a signed 32 bit integer.
    SOCKET GetSocketDescriptor() { return m_socket; };

	/// Return socket descriptor
	///  @return socket descriptor which is a signed 32 bit integer.
    CSocketType GetSocketType() { return m_nSocketType; };

    /// Returns clients Internet host address as a string in standard numbers-and-dots notation.
	///  @return NULL if invalid
    uchar_t *GetClientAddr() { return (uchar_t *)inet_ntoa(m_stClientSockaddr.sin_addr); };

	/// Returns the port number on which the client is connected.
	///  @return client port number.
    int16_t GetClientPort() { return m_stClientSockaddr.sin_port; };

    /// Returns server Internet host address as a string in standard numbers-and-dots notation.
	///  @return NULL if invalid
    uchar_t *GetServerAddr() { return (uchar_t *)inet_ntoa(m_stServerSockaddr.sin_addr); };

	/// Returns the port number on which the server is connected.
	///  @return server port number.
    uint16_t GetServerPort() { return ntohs(m_stServerSockaddr.sin_port); };

    /// Get the TCP receive buffer window size for the current socket object.
    /// <br><br>\b NOTE: Linux will set the receive buffer to twice the value passed.
    ///  @return zero on failure else the number of bytes of the TCP receive buffer window size if successful.
    uint16_t GetReceiveWindowSize() { return GetWindowSize(SO_RCVBUF); };

    /// Get the TCP send buffer window size for the current socket object.
    /// <br><br>\b NOTE: Linux will set the send buffer to twice the value passed.
    ///  @return zero on failure else the number of bytes of the TCP receive buffer window size if successful.
    uint16_t GetSendWindowSize() { return GetWindowSize(SO_SNDBUF); };

    /// Set the TCP receive buffer window size for the current socket object.
    /// <br><br>\b NOTE: Linux will set the receive buffer to twice the value passed.
    ///  @return zero on failure else the number of bytes of the TCP send buffer window size if successful.
    uint16_t SetReceiveWindowSize(uint16_t nWindowSize) { return SetWindowSize(SO_RCVBUF, nWindowSize); };

    /// Set the TCP send buffer window size for the current socket object.
    /// <br><br>\b NOTE: Linux will set the send buffer to twice the value passed.
    ///  @return zero on failure else the number of bytes of the TCP send buffer window size if successful.
    uint16_t SetSendWindowSize(uint16_t nWindowSize) { return SetWindowSize(SO_SNDBUF, nWindowSize); };

protected:
    /// Set the socket to blocking.
    /// @return true if successful set to blocking, else return false;
    bool SetBlocking(void);
    /// Set internal socket error to that specified error
    ///  \param error type of error
    void SetSocketError(CSimpleSocket::CSocketError error) { m_socketErrno = error; };

    /// Set object socket handle to that specified as parameter 
    ///  \param socket value of socket descriptor
    void SetSocketHandle(SOCKET socket) { m_socket = socket; };

private:
    /// Generic function used to get the send/receive window size
    ///  @return zero on failure else the number of bytes of the TCP window size if successful.
    uint16_t GetWindowSize(uint32_t nOptionName);

    /// Generic function used to set the send/receive window size
    ///  @return zero on failure else the number of bytes of the TCP window size if successful.
    uint16_t SetWindowSize(uint32_t nOptionName, uint32_t nWindowSize);


    /// Attempts to send at most nNumItem blocks described by sendVector 
    /// to the socket descriptor associated with the socket object.
    /// \param sendVector pointer to an array of iovec structures
    /// \param nNumItems number of items in the vector to process
    /// <br>\b Note: This implementation is for systems that don't natively
    /// support this functionality.
    /// @return number of bytes actually sent, return of zero means the
    /// connection has been shutdown on the other side, and a return of -1
    /// means that an error has occurred.
    int32_t Writev(const struct iovec *pVector, size_t nCount);

    /// Flush the socket descriptor owned by the object.
	/// @return true data was successfully sent, else return false;
    bool Flush();

	CSimpleSocket *operator=(CSimpleSocket &socket);
	CSimpleSocket(CSimpleSocket &socket);
protected:
    SOCKET               m_socket;            /// socket handle
    CSocketError         m_socketErrno;       /// number of last error
    int32_t                m_nSocketDomain;     /// socket type PF_INET, PF_INET6
    CSocketType          m_nSocketType;       /// socket type - UDP, TCP or RAW
    bool                 m_bIsBlocking;       /// is socket blocking
    bool                 m_bIsMulticast;      /// is the UDP socket multicast;
    struct sockaddr_in   m_stServerSockaddr;  /// server address
    struct sockaddr_in   m_stClientSockaddr;  /// client address
    struct sockaddr_in   m_stMulticastGroup;  /// multicast group to bind to
    struct linger        m_stLinger;          /// linger flag
#ifdef _WIN32
    WSADATA              m_hWSAData;          /// Windows
#endif 
    fd_set               m_writeFds;		  /// write file descriptor set
    fd_set               m_readFds;		      /// read file descriptor set
    fd_set               m_errorFds;		  /// error file descriptor set
};

} // namespace xdelta
#endif /*  __SOCKET_H__  */

