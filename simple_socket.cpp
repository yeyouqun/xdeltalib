/*---------------------------------------------------------------------------*/
/*                                                                           */
/* CSimpleSocket.cpp - CSimpleSocket Implementation							 */
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
#include <stdlib.h>
#include <string.h>
#include <string>

#include "mytypes.h"
#include "platform.h"
#include "buffer.h"
#include "simple_socket.h"
#include "lz4.h"
#include "lz4hc.h"

namespace xdelta {

CSimpleSocket::CSimpleSocket(CSocketType nType) :
    m_socket(INVALID_SOCKET), 
    m_socketErrno(CSimpleSocket::SocketInvalidSocket), 
    m_nSocketDomain(AF_INET), 
    m_nSocketType(SocketTypeTcp),
    m_bIsBlocking(true)
{
    memset(&m_stLinger, 0, sizeof(struct linger));

    switch(nType)
    {
	//----------------------------------------------------------------------
	// Declare socket type stream - TCP
	//----------------------------------------------------------------------
    case CSimpleSocket::SocketTypeTcp: 
        {
            m_nSocketDomain = AF_INET;
            m_nSocketType = CSimpleSocket::SocketTypeTcp;
            break;
        }
    case CSimpleSocket::SocketTypeTcp6: 
        {
            m_nSocketDomain = AF_INET6;
            m_nSocketType = CSimpleSocket::SocketTypeTcp6;
            break;
        }
	default:
	    m_nSocketType = CSimpleSocket::SocketTypeTcp;
	    break;
    }
}

//------------------------------------------------------------------------------
//
// Initialize() - Initialize socket class
//
//------------------------------------------------------------------------------
bool CSimpleSocket::Initialize()
{
    errno = CSimpleSocket::SocketSuccess;

#ifdef _WIN32
    //-------------------------------------------------------------------------
    // Data structure containing general Windows Sockets Info                
    //-------------------------------------------------------------------------
    memset(&m_hWSAData, 0, sizeof(m_hWSAData));
    WSAStartup(MAKEWORD(2, 0), &m_hWSAData);
#endif

    //-------------------------------------------------------------------------
    // Create the basic Socket Handle										 
    //-------------------------------------------------------------------------
    m_socket = socket(m_nSocketDomain, SOCK_STREAM, 0);
    TranslateSocketError();
	SetBlocking ();
    return (IsSocketValid());
}

//------------------------------------------------------------------------------
//
// GetWindowSize() 
//
//------------------------------------------------------------------------------
uint16_t CSimpleSocket::GetWindowSize(uint32_t nOptionName)
{
    uint32_t nTcpWinSize = 0;

    //-------------------------------------------------------------------------
    // no socket given, return system default allocate our own new socket
    //-------------------------------------------------------------------------
    if (m_socket != CSimpleSocket::SocketError)
    {
        socklen_t nLen = sizeof(nTcpWinSize);

        //---------------------------------------------------------------------
        // query for buffer size 
        //---------------------------------------------------------------------
        GETSOCKOPT(m_socket, SOL_SOCKET, nOptionName, &nTcpWinSize, &nLen);
        TranslateSocketError();
    }
    else
    {
        SetSocketError(CSimpleSocket::SocketInvalidSocket);
    }

    return nTcpWinSize;
}


//------------------------------------------------------------------------------
//
// SetWindowSize()
//
//------------------------------------------------------------------------------
uint16_t CSimpleSocket::SetWindowSize(uint32_t nOptionName, uint32_t nWindowSize)
{
    //-------------------------------------------------------------------------
    // no socket given, return system default allocate our own new socket
    //-------------------------------------------------------------------------
    if (m_socket != CSimpleSocket::SocketError)
    {
        SETSOCKOPT(m_socket, SOL_SOCKET, nOptionName, &nWindowSize, sizeof(nWindowSize));
        TranslateSocketError();
    }
    else
    {
        SetSocketError(CSimpleSocket::SocketInvalidSocket);
    }

    return (uint16_t)nWindowSize;
}



//------------------------------------------------------------------------------
//
// Send() - Send data on a valid socket
//
//------------------------------------------------------------------------------
int32_t CSimpleSocket::Send(const uchar_t *pBuf, int32_t bytesToSend)
{
    SetSocketError(SocketSuccess);
    int32_t BytesSent = 0;

	while (bytesToSend > 0) {
		if (IsSocketValid() && (bytesToSend > 0) && (pBuf != NULL)) {
			int32_t block2sent = bytesToSend > (1024*1024) ? (1024*1024) : bytesToSend;
			switch(m_nSocketType)
			{
				case CSimpleSocket::SocketTypeTcp:
				{
					//---------------------------------------------------------
					// Check error condition and attempt to resend if call
					// was interrupted by a signal.
					//---------------------------------------------------------
					do
					{
						int32_t bytes = SEND(m_socket, pBuf + BytesSent, block2sent, 0);
						if (bytes > 0) {
							bytesToSend -= bytes;
							BytesSent += bytes;
						}
						else {
							TranslateSocketError();
							return BytesSent;
						}

						TranslateSocketError();
					} while (GetSocketError() == CSimpleSocket::SocketInterrupted);
					break;
				}
				case CSimpleSocket::SocketTypeTcp6:
					break;
				default:
					break;
			}
		}
	}

    return BytesSent;
}


//------------------------------------------------------------------------------
//
// Close() - Close socket and free up any memory allocated for the socket
//
//------------------------------------------------------------------------------
bool CSimpleSocket::Close(void)
{
	bool bRetVal = false;

	//--------------------------------------------------------------------------
	// if socket handle is currently valid, close and then invalidate
	//--------------------------------------------------------------------------
	if (IsSocketValid())
	{
		if (CLOSE(m_socket) != CSimpleSocket::SocketError)
		{
			m_socket = INVALID_SOCKET;
			bRetVal = true;
		}
	}

	TranslateSocketError();

	return bRetVal;
}

//------------------------------------------------------------------------------
//
// Receive() - Attempts to receive a block of data on an established		
//			   connection.	Data is received in an internal buffer managed	
//			   by the class.  This buffer is only valid until the next call	
//			   to Receive(), a call to Close(), or until the object goes out
//			   of scope.													
//																			
//------------------------------------------------------------------------------
int32_t CSimpleSocket::Receive(char_buffer<uchar_t> & buff, int32_t nMaxBytes)
{
    int32_t BytesReceived = 0;
    //--------------------------------------------------------------------------
    // If the socket is invalid then return false.
    //--------------------------------------------------------------------------
    if (IsSocketValid() == false)
    {
        return BytesReceived;
    }

    SetSocketError(SocketSuccess);

	while (nMaxBytes > 0 && Select ()) {
		switch (m_nSocketType) {
			//----------------------------------------------------------------------
			// If zero bytes are received, then return.  If SocketERROR is 
			// received, free buffer and return CSocket::SocketError (-1) to caller.	
			//----------------------------------------------------------------------
			case CSimpleSocket::SocketTypeTcp:
			{
				int32_t bytes = RECV(m_socket, buff.wr_ptr (), nMaxBytes, 0);
				if (bytes > 0) {
					buff.wr_ptr (bytes);
					BytesReceived += bytes;
					nMaxBytes -= bytes;
				}
				else {
					TranslateSocketError();
					return BytesReceived;
				}
				break;
			}
			case CSimpleSocket::SocketTypeTcp6:
			{
				break;
			}
			default:
				break;
		}
	}

    return BytesReceived;
}

//------------------------------------------------------------------------------
//
// SetBlocking()
//
//------------------------------------------------------------------------------
bool CSimpleSocket::SetBlocking(void)
{
    int32_t nCurFlags;

#if _WIN32
    nCurFlags = 0;

    if (ioctlsocket(m_socket, FIONBIO, (ULONG *)&nCurFlags) != 0)
    {
        return false;
    }
#else
    if ((nCurFlags = fcntl(m_socket, F_GETFL)) < 0)
    {
        TranslateSocketError();
        return false;
    }

    nCurFlags &= (~O_NONBLOCK);

    if (fcntl(m_socket, F_SETFL, nCurFlags) != 0)
    {
        TranslateSocketError();
        return false;
    }
#endif
    m_bIsBlocking = true;

    return true;
}


//------------------------------------------------------------------------------
//
// TranslateSocketError() -					
//
//------------------------------------------------------------------------------
void CSimpleSocket::TranslateSocketError(void)
{
#if defined (_LINUX) || defined (_UNIX)
    switch (errno)
    {
        case EXIT_SUCCESS:
	    SetSocketError(CSimpleSocket::SocketSuccess);
            break;
        case ENOTCONN:
            SetSocketError(CSimpleSocket::SocketNotconnected);
            break;
        case ENOTSOCK:
        case EBADF:
        case EACCES:
        case EAFNOSUPPORT:
        case EMFILE:
        case ENFILE:
        case ENOBUFS:
        case ENOMEM:
        case EPROTONOSUPPORT:
	    SetSocketError(CSimpleSocket::SocketInvalidSocket);
            break;
	case ECONNREFUSED :
	    SetSocketError(CSimpleSocket::SocketConnectionRefused);
	    break;
	case ETIMEDOUT:
	    SetSocketError(CSimpleSocket::SocketTimedout);
	    break;
	case EINPROGRESS:
	    SetSocketError(CSimpleSocket::SocketEinprogress);
	    break;
	case EWOULDBLOCK:
            //		case EAGAIN:
	    SetSocketError(CSimpleSocket::SocketEwouldblock);
	    break;
        case EINTR:
	    SetSocketError(CSimpleSocket::SocketInterrupted);
            break;
        case ECONNABORTED:
	    SetSocketError(CSimpleSocket::SocketConnectionAborted);
            break;
        case EINVAL:
        case EPROTO:
	    SetSocketError(CSimpleSocket::SocketProtocolError);
            break;
        case EPERM:
	    SetSocketError(CSimpleSocket::SocketFirewallError);
            break;
        case EFAULT:
	    SetSocketError(CSimpleSocket::SocketInvalidSocketBuffer);
            break;
        case ECONNRESET:
            SetSocketError(CSimpleSocket::SocketConnectionReset);
            break;
        case ENOPROTOOPT: 
            SetSocketError(CSimpleSocket::SocketConnectionReset);
            break;
        default:
            SetSocketError(CSimpleSocket::SocketEunknown);
            break;	
    }
#else
    int32_t nError = WSAGetLastError();
    switch (nError)
    {
        case EXIT_SUCCESS:
            SetSocketError(CSimpleSocket::SocketSuccess);
            break;
        case WSAEBADF:
        case WSAENOTCONN:
            SetSocketError(CSimpleSocket::SocketNotconnected);
            break;
        case WSAEINTR:
            SetSocketError(CSimpleSocket::SocketInterrupted);
            break;
        case WSAEACCES:
        case WSAEAFNOSUPPORT:
        case WSAEINVAL:
        case WSAEMFILE:
        case WSAENOBUFS:
        case WSAEPROTONOSUPPORT:
            SetSocketError(CSimpleSocket::SocketInvalidSocket);
            break;
        case WSAECONNREFUSED :
            SetSocketError(CSimpleSocket::SocketConnectionRefused);
            break;
        case WSAETIMEDOUT:
            SetSocketError(CSimpleSocket::SocketTimedout);
            break;
        case WSAEINPROGRESS:
            SetSocketError(CSimpleSocket::SocketEinprogress);
            break;
        case WSAECONNABORTED:
            SetSocketError(CSimpleSocket::SocketConnectionAborted);
            break;
        case WSAEWOULDBLOCK:
            SetSocketError(CSimpleSocket::SocketEwouldblock);
            break;
        case WSAENOTSOCK:
            SetSocketError(CSimpleSocket::SocketInvalidSocket);
            break;
        case WSAECONNRESET:
            SetSocketError(CSimpleSocket::SocketConnectionReset);
            break;
        case WSANO_DATA:
            SetSocketError(CSimpleSocket::SocketInvalidAddress);
            break;
        case WSAEADDRINUSE:
            SetSocketError(CSimpleSocket::SocketAddressInUse);
            break;
        case WSAEFAULT:
            SetSocketError(CSimpleSocket::SocketInvalidPointer);
            break;
        default:
            SetSocketError(CSimpleSocket::SocketEunknown);
            break;	
    }
#endif
}


//------------------------------------------------------------------------------
//
// Select()
//
//------------------------------------------------------------------------------
bool CSimpleSocket::Select(int32_t nTimeoutSec, int32_t nTimeoutUSec)
{
    bool            bRetVal = false;
    struct timeval *pTimeout = NULL;
    struct timeval  timeout;
    int32_t           nNumDescriptors = -1;
    int32_t           nError = 0;

    FD_ZERO(&m_errorFds);
    FD_ZERO(&m_readFds);
    FD_ZERO(&m_writeFds);
    FD_SET(m_socket, &m_errorFds);
    FD_SET(m_socket, &m_readFds);
    FD_SET(m_socket, &m_writeFds);
    
    //---------------------------------------------------------------------
    // If timeout has been specified then set value, otherwise set timeout
    // to NULL which will block until a descriptor is ready for read/write
    // or an error has occurred.
    //---------------------------------------------------------------------
    if ((nTimeoutSec > 0) || (nTimeoutUSec > 0))
    {
        timeout.tv_sec = nTimeoutSec;
        timeout.tv_usec = nTimeoutUSec;
        pTimeout = &timeout;
    }
    
    nNumDescriptors = SELECT(m_socket+1, &m_readFds, &m_writeFds, &m_errorFds, pTimeout);
//    nNumDescriptors = SELECT(m_socket+1, &m_readFds, NULL, NULL, pTimeout);
    
    //----------------------------------------------------------------------
    // Handle timeout
    //----------------------------------------------------------------------
    if (nNumDescriptors == 0) 
    {
        SetSocketError(CSimpleSocket::SocketTimedout);
    }
    //----------------------------------------------------------------------
    // If a file descriptor (read/write) is set then check the
    // socket error (SO_ERROR) to see if there is a pending error.
    //----------------------------------------------------------------------
    else if ((FD_ISSET(m_socket, &m_readFds)) || (FD_ISSET(m_socket, &m_writeFds)))
    {
        int32_t nLen = sizeof(nError);
        
        if (GETSOCKOPT(m_socket, SOL_SOCKET, SO_ERROR, &nError, &nLen) == 0)
        {
            errno = nError;
            
            if (nError == 0)
            {
                bRetVal = true;
            }
        }
        
        TranslateSocketError();
    }

    return bRetVal;
}

} //namespace xdelta

