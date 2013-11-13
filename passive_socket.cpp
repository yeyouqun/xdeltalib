/*---------------------------------------------------------------------------*/
/*                                                                           */
/* PassiveSocket.cpp - Passive Socket Implementation		 			     */
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
#include <memory>

#include "mytypes.h"
#include "platform.h"
#include "buffer.h"
#include "passive_socket.h"

namespace xdelta {

CPassiveSocket::CPassiveSocket(bool compress, CSocketType nType) : CSimpleSocket(compress, nType)
{
}

//------------------------------------------------------------------------------
//
// Listen() - 
//
//------------------------------------------------------------------------------
bool CPassiveSocket::Listen(const uchar_t *pAddr, int16_t nPort, int32_t nConnectionBacklog)
{
    bool		   bRetVal = false;
#ifdef _WIN32
    ULONG          inAddr;
#else
    int32_t          nReuse;
    in_addr_t      inAddr;
    nReuse = IPTOS_LOWDELAY;
#endif

    //--------------------------------------------------------------------------
    // Set the following socket option SO_REUSEADDR.  This will allow the file
    // descriptor to be reused immediately after the socket is closed instead
    // of setting in a TIMED_WAIT state.
    //--------------------------------------------------------------------------
#ifdef _LINUX
    SETSOCKOPT(m_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&nReuse, sizeof(int32_t));
    SETSOCKOPT(m_socket, IPPROTO_TCP, IP_TOS, &nReuse, sizeof(int32_t));
#endif

    memset(&m_stServerSockaddr,0,sizeof(m_stServerSockaddr));
    m_stServerSockaddr.sin_family = AF_INET;
    m_stServerSockaddr.sin_port = htons(nPort);
    
    //--------------------------------------------------------------------------
    // If no IP Address (interface ethn) is supplied, or the loop back is 
    // specified then bind to any interface, else bind to specified interface.
    //--------------------------------------------------------------------------
    if ((pAddr == NULL) || (!strlen((const char *)pAddr)))
    {
        m_stServerSockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    }
    else
    {
        if ((inAddr = inet_addr((const char *)pAddr)) != INADDR_NONE)
        {
            m_stServerSockaddr.sin_addr.s_addr = inAddr;
        }
    }
    
    //--------------------------------------------------------------------------
    // Bind to the specified port 
    //--------------------------------------------------------------------------
    if (bind(m_socket, (struct sockaddr *)&m_stServerSockaddr, sizeof(m_stServerSockaddr)) != CSimpleSocket::SocketError)
    {
        socklen_t nSockLen = sizeof(struct sockaddr);
        memset(&m_stServerSockaddr, 0, nSockLen);
        getsockname(m_socket, (struct sockaddr *)&m_stServerSockaddr, &nSockLen);

        if (m_nSocketType == CSimpleSocket::SocketTypeTcp)
        {
            if (listen(m_socket, nConnectionBacklog) != CSimpleSocket::SocketError)
            {
                bRetVal = true;
            }
        }  
        else      
        {
            bRetVal = true;
        }
    }

    //--------------------------------------------------------------------------
    // If there was a socket error then close the socket to clean out the 
    // connection in the backlog.
    //--------------------------------------------------------------------------
    TranslateSocketError();

    if (bRetVal == false)
    {
        Close();
    }

    return bRetVal;
}


//------------------------------------------------------------------------------
//
// Accept() - 
//
//------------------------------------------------------------------------------
CActiveSocket *CPassiveSocket::Accept(uint32_t timeout_sec)
{
    uint32_t         nSockLen;
	std::auto_ptr<CActiveSocket> ClientSocket;
    SOCKET         socket = CSimpleSocket::SocketError;

    if (m_nSocketType != CSimpleSocket::SocketTypeTcp) {
        SetSocketError(CSimpleSocket::SocketProtocolError);
        return 0;
    }

    ClientSocket.reset (new CActiveSocket(m_compress_, m_nSocketType));

    //--------------------------------------------------------------------------
    // Wait for incoming connection.
    //--------------------------------------------------------------------------
    CSocketError socketErrno = SocketSuccess;
    nSockLen = sizeof(m_stClientSockaddr);
    do {
        errno = 0;
		if (!Select (timeout_sec, 0))
			return 0;

		socket = accept(m_socket, (struct sockaddr *)&m_stClientSockaddr, (socklen_t *)&nSockLen);
        if (socket != -1) {
            ClientSocket->SetSocketHandle(socket);
            ClientSocket->TranslateSocketError();
            socketErrno = ClientSocket->GetSocketError();
            socklen_t nSockLen = sizeof(struct sockaddr);

            //-------------------------------------------------------------
            // Store client and server IP and port information for this
            // connection.
            //-------------------------------------------------------------
            getpeername(m_socket, (struct sockaddr *)&ClientSocket->m_stClientSockaddr, &nSockLen);
            memcpy((void *)&ClientSocket->m_stClientSockaddr, (void *)&m_stClientSockaddr, nSockLen);

            memset(&ClientSocket->m_stServerSockaddr, 0, nSockLen);
            getsockname(m_socket, (struct sockaddr *)&ClientSocket->m_stServerSockaddr, &nSockLen);
        }
        else {
            TranslateSocketError();
            socketErrno = GetSocketError();
        }
    } while (socketErrno == CSimpleSocket::SocketInterrupted);
    
    if (socketErrno != CSimpleSocket::SocketSuccess)
		return 0;

	return ClientSocket.release ();
}

} // namespace xdelta

