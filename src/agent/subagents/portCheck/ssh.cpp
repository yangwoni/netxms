/* $Id: ssh.cpp,v 1.3 2005-01-28 23:19:36 alk Exp $ */

#include <nms_common.h>
#include <nms_agent.h>

#include "main.h"
#include "net.h"

LONG H_CheckSSH(char *pszParam, char *pArg, char *pValue)
{
	LONG nRet = SYSINFO_RC_SUCCESS;

	char szHost[256];
	char szPort[256];
	unsigned short nPort;

   NxGetParameterArg(pszParam, 1, szHost, sizeof(szHost));
   NxGetParameterArg(pszParam, 2, szPort, sizeof(szPort));

	if (szHost[0] == 0)
	{
		return SYSINFO_RC_ERROR;
	}

	nPort = (unsigned short)atoi(szPort);
	if (nPort == 0)
	{
		nPort = 22;
	}

	if (CheckSSH(szHost, 0, nPort, NULL, NULL) == 0)
	{
		ret_int(pValue, 1);
	}
	else
	{
		ret_int(pValue, 0);
	}

	return nRet;
}

int CheckSSH(char *szAddr, DWORD dwAddr, short nPort, char *szUser, char *szPass)
{
	int nRet = 0;
	int nSd;

	nSd = NetConnectTCP(szAddr, dwAddr, nPort);
	if (nSd > 0)
	{
		char szBuff[512];
		char szTmp[128];

		nRet = PC_ERR_HANDSHAKE;

		if (NetRead(nSd, szBuff, sizeof(szBuff)) >= 8)
		{
			int nMajor, nMinor;

			if (sscanf(szBuff, "SSH-%d.%d-", &nMajor, &nMinor) == 2)
			{
				snprintf(szTmp, sizeof(szTmp), "SSH-%d.%d-NetXMS\n",
						nMajor, nMinor);
				if (NetWrite(nSd, szTmp, strlen(szTmp)) > 0)
				{
					nRet = PC_ERR_NONE;
				}
			}
		}

		NetClose(nSd);
	}
	else
	{
		nRet = PC_ERR_CONNECT;
	}

	return nRet;
}

///////////////////////////////////////////////////////////////////////////////
/*

$Log: not supported by cvs2svn $
Revision 1.2  2005/01/28 02:50:32  alk
added support for CMD_CHECK_NETWORK_SERVICE
suported:
	ssh: host/port req.
	pop3: host/port/request string req. request string format: "login:password"

Revision 1.1  2005/01/19 13:42:47  alk
+ ServiceCheck.SSH(host[, port]) Added


*/
