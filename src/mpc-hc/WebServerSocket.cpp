/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2012 see Authors.txt
 *
 * This file is part of MPC-HC.
 *
 * MPC-HC is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * MPC-HC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "stdafx.h"
#include "WebServer.h"
#include "WebServerSocket.h"


CWebServerSocket::CWebServerSocket(CWebServer* pWebServer, int port)
    : m_pWebServer(pWebServer)
{
    try {
        Create(port);
        Listen();
    } catch (...) {
        ASSERT(false);
        m_pWebServer = nullptr;
    }
}

CWebServerSocket::~CWebServerSocket()
{
}

void CWebServerSocket::OnAccept(int nErrorCode)
{
    if (nErrorCode == 0 && m_pWebServer) {
        m_pWebServer->OnAccept(this);
    }

    __super::OnAccept(nErrorCode);
}
