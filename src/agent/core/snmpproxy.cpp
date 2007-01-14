/* $Id: snmpproxy.cpp,v 1.2 2007-01-14 20:55:08 victor Exp $ */
/* 
** NetXMS multiplatform core agent
** Copyright (C) 2003, 2004, 2005, 2006 Victor Kirhenshtein
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** File: snmpproxy.cpp
**
**/

#include "nxagentd.h"


//
// Constants
//

#define SNMP_BUFFER_SIZE		65536


//
// Read PDU from network
//

static BOOL ReadPDU(SOCKET hSocket, BYTE *pdu, DWORD *pdwSize)
{
   fd_set rdfs;
   struct timeval tv;
	int nBytes;
#ifndef _WIN32
   int iErr;
	DWORD dwTimeout = g_dwSNMPTimeout;
   QWORD qwTime;

   do
   {
#endif
      FD_ZERO(&rdfs);
      FD_SET(hSocket, &rdfs);
      tv.tv_sec = g_dwSNMPTimeout / 1000;
      tv.tv_usec = (g_dwSNMPTimeout % 1000) * 1000;
#ifdef _WIN32
      if (select(hSocket + 1, &rdfs, NULL, NULL, &tv) <= 0)
         return FALSE;
#else
      qwTime = GetCurrentTimeMs();
      if ((iErr = select(hSocket + 1, &rdfs, NULL, NULL, &tv)) <= 0)
      {
         if (((iErr == -1) && (errno != EINTR)) ||
             (iErr == 0))
         {
            return FALSE;
         }
      }
      qwTime = GetCurrentTimeMs() - qwTime;  // Elapsed time
      dwTimeout -= min(((DWORD)qwTime), dwTimeout);
   } while(iErr < 0);
#endif

	nBytes = recv(hSocket, (char *)pdu, SNMP_BUFFER_SIZE, 0);
	if (nBytes >= 0)
	{
		*pdwSize = nBytes;
		return TRUE;
	}
	return FALSE;
}


//
// Send SNMP request to target, receive response, and send it to server
//

void ProxySNMPRequest(CSCPMessage *pRequest, CSCPMessage *pResponse)
{
	BYTE *pduIn, *pduOut;
	DWORD dwSize;
	SOCKET hSocket;
	int nRetries;
	struct sockaddr_in addr;

	dwSize = pRequest->GetVariableLong(VID_PDU_SIZE);
	if (dwSize > 0)
	{
		pduIn = (BYTE *)malloc(dwSize);
		pRequest->GetVariableBinary(VID_PDU, pduIn, dwSize);

		hSocket = socket(AF_INET, SOCK_DGRAM, 0);
		if (hSocket != -1)
		{
			memset(&addr, 0, sizeof(struct sockaddr_in));
			addr.sin_family = AF_INET;
			addr.sin_addr.s_addr = htonl(pRequest->GetVariableLong(VID_IP_ADDRESS));
			addr.sin_port = htons(pRequest->GetVariableShort(VID_PORT));
			if (connect(hSocket, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) != -1)
			{
				pduOut = (BYTE *)malloc(SNMP_BUFFER_SIZE);
				for(nRetries = 0; nRetries < 3; nRetries++)
				{
					if (send(hSocket, (char *)pduIn, dwSize, 0) == (int)dwSize)
					{
						if (ReadPDU(hSocket, pduOut, &dwSize))
						{
							pResponse->SetVariable(VID_PDU_SIZE, dwSize);
							pResponse->SetVariable(VID_PDU, pduOut, dwSize);
							break;
						}
					}
				}
				free(pduOut);
				pResponse->SetVariable(VID_RCC, (nRetries == 3) ? ERR_REQUEST_TIMEOUT : ERR_SUCCESS);
			}
			else
			{
				pResponse->SetVariable(VID_RCC, ERR_SOCKET_ERROR);
			}
			closesocket(hSocket);
		}
		else
		{
			pResponse->SetVariable(VID_RCC, ERR_SOCKET_ERROR);
		}
		free(pduIn);
	}
	else
	{
		pResponse->SetVariable(VID_RCC, ERR_MAILFORMED_COMMAND);
	}
}
