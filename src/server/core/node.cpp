/* 
** NetXMS - Network Management System
** Copyright (C) 2003-2010 Victor Kirhenshtein
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
** File: node.cpp
**
**/

#include "nxcore.h"


//
// Node class default constructor
//

Node::Node()
     :Template()
{
   m_iStatus = STATUS_UNKNOWN;
   m_dwFlags = 0;
   m_dwDynamicFlags = 0;
   m_dwZoneGUID = 0;
   m_dwNodeType = NODE_TYPE_GENERIC;
   m_wAgentPort = AGENT_LISTEN_PORT;
   m_wAuthMethod = AUTH_NONE;
   m_szSharedSecret[0] = 0;
   m_iStatusPollType = POLL_ICMP_PING;
   m_snmpVersion = SNMP_VERSION_1;
   m_wSNMPPort = SNMP_DEFAULT_PORT;
	TCHAR community[MAX_COMMUNITY_LENGTH];
   ConfigReadStr("DefaultCommunityString", community,
                 MAX_COMMUNITY_LENGTH, "public");
	m_snmpSecurity = new SNMP_SecurityContext(community);
   m_szObjectId[0] = 0;
   m_tLastDiscoveryPoll = 0;
   m_tLastStatusPoll = 0;
   m_tLastConfigurationPoll = 0;
   m_tLastRTUpdate = 0;
   m_hPollerMutex = MutexCreate();
   m_hAgentAccessMutex = MutexCreate();
   m_mutexRTAccess = MutexCreate();
	m_mutexTopoAccess = MutexCreate();
   m_pAgentConnection = NULL;
   m_szAgentVersion[0] = 0;
   m_szPlatformName[0] = 0;
	m_szSysDescription[0] = 0;
   m_dwNumParams = 0;
   m_pParamList = NULL;
   m_dwPollerNode = 0;
   m_dwProxyNode = 0;
	m_dwSNMPProxy = 0;
   memset(m_qwLastEvents, 0, sizeof(QWORD) * MAX_LAST_EVENTS);
   m_pRoutingTable = NULL;
   m_tFailTimeSNMP = 0;
   m_tFailTimeAgent = 0;
	m_pTopology = NULL;
	m_tLastTopologyPoll = 0;
	m_iPendingStatus = -1;
	m_iPollCount = 0;
	m_iRequiredPollCount = 0;	// Use system default
	m_nUseIfXTable = IFXTABLE_DEFAULT;	// Use system default
	m_jobQueue = new ServerJobQueue();
}


//
// Constructor for new node object
//

Node::Node(DWORD dwAddr, DWORD dwFlags, DWORD dwProxyNode, DWORD dwSNMPProxy, DWORD dwZone)
     :Template()
{
   m_iStatus = STATUS_UNKNOWN;
   m_dwIpAddr = dwAddr;
   m_dwFlags = dwFlags;
   m_dwDynamicFlags = 0;
   m_dwZoneGUID = dwZone;
   m_dwNodeType = NODE_TYPE_GENERIC;
   m_wAgentPort = AGENT_LISTEN_PORT;
   m_wAuthMethod = AUTH_NONE;
   m_szSharedSecret[0] = 0;
   m_iStatusPollType = POLL_ICMP_PING;
   m_snmpVersion = SNMP_VERSION_1;
   m_wSNMPPort = SNMP_DEFAULT_PORT;
	TCHAR community[MAX_COMMUNITY_LENGTH];
   ConfigReadStr("DefaultCommunityString", community,
                 MAX_COMMUNITY_LENGTH, "public");
	m_snmpSecurity = new SNMP_SecurityContext(community);
   IpToStr(dwAddr, m_szName);    // Make default name from IP address
   m_szObjectId[0] = 0;
   m_tLastDiscoveryPoll = 0;
   m_tLastStatusPoll = 0;
   m_tLastConfigurationPoll = 0;
   m_tLastRTUpdate = 0;
   m_hPollerMutex = MutexCreate();
   m_hAgentAccessMutex = MutexCreate();
   m_mutexRTAccess = MutexCreate();
	m_mutexTopoAccess = MutexCreate();
   m_pAgentConnection = NULL;
   m_szAgentVersion[0] = 0;
   m_szPlatformName[0] = 0;
	m_szSysDescription[0] = 0;
   m_dwNumParams = 0;
   m_pParamList = NULL;
   m_dwPollerNode = 0;
   m_dwProxyNode = dwProxyNode;
	m_dwSNMPProxy = dwSNMPProxy;
   memset(m_qwLastEvents, 0, sizeof(QWORD) * MAX_LAST_EVENTS);
   m_bIsHidden = TRUE;
   m_pRoutingTable = NULL;
   m_tFailTimeSNMP = 0;
   m_tFailTimeAgent = 0;
	m_pTopology = NULL;
	m_tLastTopologyPoll = 0;
	m_iPendingStatus = -1;
	m_iPollCount = 0;
	m_iRequiredPollCount = 0;	// Use system default
	m_nUseIfXTable = IFXTABLE_DEFAULT;	// Use system default
	m_jobQueue = new ServerJobQueue();
}


//
// Node destructor
//

Node::~Node()
{
   MutexDestroy(m_hPollerMutex);
   MutexDestroy(m_hAgentAccessMutex);
   MutexDestroy(m_mutexRTAccess);
	MutexDestroy(m_mutexTopoAccess);
   delete m_pAgentConnection;
   safe_free(m_pParamList);
   DestroyRoutingTable(m_pRoutingTable);
	delete m_pTopology;
	delete m_jobQueue;
	delete m_snmpSecurity;
}


//
// Create object from database data
//

BOOL Node::CreateFromDB(DWORD dwId)
{
   TCHAR query[1024];
   DB_RESULT hResult;
   int i, iNumRows;
   DWORD dwSubnetId;
   NetObj *pObject;
   BOOL bResult = FALSE;

   m_dwId = dwId;

   if (!LoadCommonProperties())
   {
      DbgPrintf(2, "Cannot load common properties for node object %d", dwId);
      return FALSE;
   }

   _sntprintf(query, 1024, _T("SELECT primary_ip,node_flags,")
                           _T("snmp_version,auth_method,secret,")
                           _T("agent_port,status_poll_type,snmp_oid,")
                           _T("node_type,agent_version,")
                           _T("platform_name,poller_node_id,zone_guid,")
                           _T("proxy_node,snmp_proxy,required_polls,uname,")
									_T("use_ifxtable,snmp_port,community,usm_auth_password,")
									_T("usm_priv_password,usm_methods")
	                        _T(" FROM nodes WHERE id=%d"), dwId);
   hResult = DBSelect(g_hCoreDB, query);
   if (hResult == NULL)
      return FALSE;     // Query failed

   if (DBGetNumRows(hResult) == 0)
   {
      DBFreeResult(hResult);
      DbgPrintf(2, _T("Missing record in \"nodes\" table for node object %d"), dwId);
      return FALSE;
   }

   m_dwIpAddr = DBGetFieldIPAddr(hResult, 0, 0);
   m_dwFlags = DBGetFieldULong(hResult, 0, 1);
   m_snmpVersion = DBGetFieldLong(hResult, 0, 2);
   m_wAuthMethod = (WORD)DBGetFieldLong(hResult, 0, 3);
   DBGetField(hResult, 0, 4, m_szSharedSecret, MAX_SECRET_LENGTH);
   m_wAgentPort = (WORD)DBGetFieldLong(hResult, 0, 5);
   m_iStatusPollType = DBGetFieldLong(hResult, 0, 6);
   DBGetField(hResult, 0, 7, m_szObjectId, MAX_OID_LEN * 4);
   m_dwNodeType = DBGetFieldULong(hResult, 0, 8);
   DBGetField(hResult, 0, 9, m_szAgentVersion, MAX_AGENT_VERSION_LEN);
   DBGetField(hResult, 0, 10, m_szPlatformName, MAX_PLATFORM_NAME_LEN);
   m_dwPollerNode = DBGetFieldULong(hResult, 0, 11);
   m_dwZoneGUID = DBGetFieldULong(hResult, 0, 12);
   m_dwProxyNode = DBGetFieldULong(hResult, 0, 13);
   m_dwSNMPProxy = DBGetFieldULong(hResult, 0, 14);
   m_iRequiredPollCount = DBGetFieldLong(hResult, 0, 15);
   DBGetField(hResult, 0, 16, m_szSysDescription, MAX_DB_STRING);
   m_nUseIfXTable = (BYTE)DBGetFieldLong(hResult, 0, 17);
	m_wSNMPPort = (WORD)DBGetFieldLong(hResult, 0, 18);

	// SNMP authentication parameters
	TCHAR snmpAuthObject[256], snmpAuthPassword[256], snmpPrivPassword[256];
	DBGetField(hResult, 0, 19, snmpAuthObject, 256);
	DBGetField(hResult, 0, 20, snmpAuthPassword, 256);
	DBGetField(hResult, 0, 21, snmpPrivPassword, 256);
	int snmpMethods = DBGetFieldLong(hResult, 0, 21);
	delete m_snmpSecurity;
	m_snmpSecurity = new SNMP_SecurityContext(snmpAuthObject, snmpAuthPassword, snmpPrivPassword, snmpMethods & 0xFF, snmpMethods >> 8);
	m_snmpSecurity->setSecurityModel((m_snmpVersion == SNMP_VERSION_3) ? SNMP_SECURITY_MODEL_USM : SNMP_SECURITY_MODEL_V2C);

   DBFreeResult(hResult);

   if (!m_bIsDeleted)
   {
      // Link node to subnets
      _sntprintf(query, 1024, _T("SELECT subnet_id FROM nsmap WHERE node_id=%d"), dwId);
      hResult = DBSelect(g_hCoreDB, query);
      if (hResult == NULL)
         return FALSE;     // Query failed

      iNumRows = DBGetNumRows(hResult);
      for(i = 0; i < iNumRows; i++)
      {
         dwSubnetId = DBGetFieldULong(hResult, i, 0);
         pObject = FindObjectById(dwSubnetId);
         if (pObject == NULL)
         {
            nxlog_write(MSG_INVALID_SUBNET_ID, EVENTLOG_ERROR_TYPE, "dd", dwId, dwSubnetId);
            break;
         }
         else if (pObject->Type() != OBJECT_SUBNET)
         {
            nxlog_write(MSG_SUBNET_NOT_SUBNET, EVENTLOG_ERROR_TYPE, "dd", dwId, dwSubnetId);
            break;
         }
         else
         {
            pObject->AddChild(this);
            AddParent(pObject);
            bResult = TRUE;
         }
      }

		if ((iNumRows == 0) && (m_dwIpAddr == 0))
		{
			bResult = TRUE;	// It's OK to don't have parent subnets for node without IP
		}

      DBFreeResult(hResult);
      loadItemsFromDB();
      LoadACLFromDB();

      // Walk through all items in the node and load appropriate thresholds
      for(i = 0; i < (int)m_dwNumItems; i++)
         if (!m_ppItems[i]->loadThresholdsFromDB())
         {
            DbgPrintf(3, _T("Cannot load thresholds for DCI %d of node %d (%s)"),
                      m_ppItems[i]->getId(), dwId, m_szName);
            bResult = FALSE;
         }
   }
   else
   {
      bResult = TRUE;
   }

   return bResult;
}


//
// Save object to database
//

BOOL Node::SaveToDB(DB_HANDLE hdb)
{
   TCHAR szQuery[4096], szIpAddr[16];
   DB_RESULT hResult;
   BOOL bNewObject = TRUE;
   BOOL bResult;

   // Lock object's access
   LockData();

   SaveCommonProperties(hdb);

   // Check for object's existence in database
   _sntprintf(szQuery, 4096, _T("SELECT id FROM nodes WHERE id=%d"), m_dwId);
   hResult = DBSelect(hdb, szQuery);
   if (hResult != 0)
   {
      if (DBGetNumRows(hResult) > 0)
         bNewObject = FALSE;
      DBFreeResult(hResult);
   }

   // Form and execute INSERT or UPDATE query
	int snmpMethods = m_snmpSecurity->getAuthMethod() | (m_snmpSecurity->getPrivMethod() << 8);
   if (bNewObject)
	{
      _sntprintf(szQuery, 4096,
                 _T("INSERT INTO nodes (id,primary_ip,snmp_port,")
                 _T("node_flags,snmp_version,community,status_poll_type,")
                 _T("agent_port,auth_method,secret,snmp_oid,proxy_node,")
                 _T("node_type,agent_version,platform_name,uname,")
                 _T("poller_node_id,zone_guid,snmp_proxy,required_polls,")
		           _T("use_ifxtable,usm_auth_password,usm_priv_password,usm_methods) VALUES ")
		           _T("(%d,'%s',%d,%d,%d,%s,%d,%d,%d,%s,%s,%d,%d,%s,%s,%s,%d,%d,%d,%d,%d,%s,%s,%d)"),
                 m_dwId, IpToStr(m_dwIpAddr, szIpAddr), (int)m_wSNMPPort, m_dwFlags,
                 m_snmpVersion, (const TCHAR *)DBPrepareString(hdb, m_snmpSecurity->getCommunity()),
					  m_iStatusPollType, (int)m_wAgentPort, m_wAuthMethod, 
					  (const TCHAR *)DBPrepareString(hdb, m_szSharedSecret),
					  (const TCHAR *)DBPrepareString(hdb, m_szObjectId),
                 m_dwProxyNode, m_dwNodeType, (const TCHAR *)DBPrepareString(hdb, m_szAgentVersion),
                 (const TCHAR *)DBPrepareString(hdb, m_szPlatformName),
					  (const TCHAR *)DBPrepareString(hdb, m_szSysDescription),
		           m_dwPollerNode, m_dwZoneGUID, m_dwSNMPProxy, m_iRequiredPollCount, m_nUseIfXTable,
					  (const TCHAR *)DBPrepareString(hdb, m_snmpSecurity->getAuthPassword()),
					  (const TCHAR *)DBPrepareString(hdb, m_snmpSecurity->getPrivPassword()), snmpMethods);
	}
   else
	{
      _sntprintf(szQuery, 4096,
                 _T("UPDATE nodes SET primary_ip='%s',snmp_port=%d,")
                 _T("node_flags=%d,snmp_version=%d,community=%s,")
                 _T("status_poll_type=%d,agent_port=%d,auth_method=%d,secret=%s,")
                 _T("snmp_oid=%s,node_type=%d,uname=%s,")
                 _T("agent_version=%s,platform_name=%s,poller_node_id=%d,")
                 _T("zone_guid=%d,proxy_node=%d,snmp_proxy=%d,")
					  _T("required_polls=%d,use_ifxtable=%d,usm_auth_password=%s,")
					  _T("usm_priv_password=%s,usm_methods=%d WHERE id=%d"),
                 IpToStr(m_dwIpAddr, szIpAddr), m_wSNMPPort, 
                 m_dwFlags, m_snmpVersion, (const TCHAR *)DBPrepareString(hdb, m_snmpSecurity->getCommunity()),
                 m_iStatusPollType, m_wAgentPort, m_wAuthMethod,
					  (const TCHAR *)DBPrepareString(hdb, m_szSharedSecret), 
                 (const TCHAR *)DBPrepareString(hdb, m_szObjectId), m_dwNodeType,
					  (const TCHAR *)DBPrepareString(hdb, m_szSysDescription),
                 (const TCHAR *)DBPrepareString(hdb, m_szAgentVersion),
					  (const TCHAR *)DBPrepareString(hdb, m_szPlatformName), m_dwPollerNode, m_dwZoneGUID,
                 m_dwProxyNode, m_dwSNMPProxy, m_iRequiredPollCount,
					  m_nUseIfXTable, (const TCHAR *)DBPrepareString(hdb, m_snmpSecurity->getAuthPassword()),
					  (const TCHAR *)DBPrepareString(hdb, m_snmpSecurity->getPrivPassword()), snmpMethods, m_dwId);
	}
   bResult = DBQuery(hdb, szQuery);

   // Save access list
   SaveACLToDB(hdb);

   UnlockData();

   // Save data collection items
   if (bResult)
   {
      DWORD i;

		lockDciAccess();
      for(i = 0; i < m_dwNumItems; i++)
         m_ppItems[i]->saveToDB(hdb);
		unlockDciAccess();
   }

	// Clear modifications flag
	LockData();
   m_bIsModified = FALSE;
	UnlockData();

	return bResult;
}


//
// Delete object from database
//

BOOL Node::DeleteFromDB()
{
   char szQuery[256];
   BOOL bSuccess;

   bSuccess = Template::DeleteFromDB();
   if (bSuccess)
   {
      _sntprintf(szQuery, 256, _T("DELETE FROM nodes WHERE id=%d"), m_dwId);
      QueueSQLRequest(szQuery);
      _sntprintf(szQuery, 256, _T("DELETE FROM nsmap WHERE node_id=%d"), m_dwId);
      QueueSQLRequest(szQuery);
      _sntprintf(szQuery, 256, _T("DROP TABLE idata_%d"), m_dwId);
      QueueSQLRequest(szQuery);
   }
   return bSuccess;
}


//
// Get ARP cache from node
//

ARP_CACHE *Node::getArpCache()
{
   ARP_CACHE *pArpCache = NULL;

   if (m_dwFlags & NF_IS_LOCAL_MGMT)
   {
      pArpCache = GetLocalArpCache();
   }
   else if (m_dwFlags & NF_IS_NATIVE_AGENT)
   {
      agentLock();
      if (connectToAgent())
         pArpCache = m_pAgentConnection->GetArpCache();
      agentUnlock();
   }
   else if (m_dwFlags & NF_IS_SNMP)
   {
		SNMP_Transport *pTransport;

		pTransport = createSnmpTransport();
		if (pTransport != NULL)
		{
			pArpCache = SnmpGetArpCache(m_snmpVersion, pTransport);
			delete pTransport;
		}
   }

   return pArpCache;
}


//
// Get list of interfaces from node
//

INTERFACE_LIST *Node::getInterfaceList()
{
   INTERFACE_LIST *pIfList = NULL;

   if ((m_dwFlags & NF_IS_NATIVE_AGENT) && (!(m_dwFlags & NF_DISABLE_NXCP)))
   {
      agentLock();
      if (connectToAgent())
      {
         pIfList = m_pAgentConnection->GetInterfaceList();
         CleanInterfaceList(pIfList);
      }
      agentUnlock();
   }
   if ((pIfList == NULL) && (m_dwFlags & NF_IS_LOCAL_MGMT))
   {
      pIfList = GetLocalInterfaceList();
   }
   if ((pIfList == NULL) && (m_dwFlags & NF_IS_SNMP) &&
       (!(m_dwFlags & NF_DISABLE_SNMP)))
   {
		SNMP_Transport *pTransport;
		BOOL useIfXTable;

		pTransport = createSnmpTransport();
		if (pTransport != NULL)
		{
			if (m_nUseIfXTable == IFXTABLE_DEFAULT)
			{
				useIfXTable = (ConfigReadInt(_T("UseIfXTable"), 1) != 0);
			}
			else
			{
				useIfXTable = (m_nUseIfXTable == IFXTABLE_ENABLED);
			}
			pIfList = SnmpGetInterfaceList(m_snmpVersion, pTransport, m_dwNodeType, useIfXTable);
			delete pTransport;
		}
   }

   if (pIfList != NULL)
      CheckInterfaceNames(pIfList);

   return pIfList;
}


//
// Find interface by index and node IP
// Returns pointer to interface object or NULL if appropriate interface couldn't be found
//

Interface *Node::findInterface(DWORD dwIndex, DWORD dwHostAddr)
{
   DWORD i;
   Interface *pInterface;

   LockChildList(FALSE);
   for(i = 0; i < m_dwChildCount; i++)
      if (m_pChildList[i]->Type() == OBJECT_INTERFACE)
      {
         pInterface = (Interface *)m_pChildList[i];
         if (pInterface->IfIndex() == dwIndex)
         {
            if (((pInterface->IpAddr() & pInterface->IpNetMask()) ==
                 (dwHostAddr & pInterface->IpNetMask())) ||
                (dwHostAddr == INADDR_ANY))
            {
               UnlockChildList();
               return pInterface;
            }
         }
      }
   UnlockChildList();
   return NULL;
}


//
// Check if given IP address is one of node's interfaces
//

BOOL Node::isMyIP(DWORD dwIpAddr)
{
   DWORD i;

   LockChildList(FALSE);
   for(i = 0; i < m_dwChildCount; i++)
      if (m_pChildList[i]->Type() == OBJECT_INTERFACE)
      {
         if (((Interface *)m_pChildList[i])->IpAddr() == dwIpAddr)
         {
            UnlockChildList();
            return TRUE;
         }
      }
   UnlockChildList();
   return FALSE;
}


//
// Create new interface
//

void Node::CreateNewInterface(DWORD dwIpAddr, DWORD dwNetMask, char *szName, 
                              DWORD dwIndex, DWORD dwType, BYTE *pbMacAddr)
{
   Interface *pInterface;
   Subnet *pSubnet = NULL;
	Cluster *pCluster;
	BOOL bAddToSubnet, bSyntheticMask = FALSE;

	DbgPrintf(5, _T("Node::CreateNewInterface(%08X, %08X, %s, %d, %d) called for node %s [%d]"),
	          dwIpAddr, dwNetMask, szName, dwIndex, dwType, m_szName, m_dwId);

   // Find subnet to place interface object to
   if (dwIpAddr != 0)
   {
		pCluster = getMyCluster();
		bAddToSubnet = (pCluster != NULL) ? !pCluster->isSyncAddr(dwIpAddr) : TRUE;
		DbgPrintf(5, _T("Node::CreateNewInterface: node=%s [%d] cluster=%s [%d] add=%d"),
		          m_szName, m_dwId, (pCluster != NULL) ? pCluster->Name() : _T("(null)"),
					 (pCluster != NULL) ? pCluster->Id() : 0, bAddToSubnet);
		if (bAddToSubnet)
		{
			pSubnet = FindSubnetForNode(dwIpAddr);
			if (pSubnet == NULL)
			{
				// Check if netmask is 0 (detect), and if yes, create
				// new subnet with class mask
				if (dwNetMask == 0)
				{
					bSyntheticMask = TRUE;
					if (dwIpAddr < 0xE0000000)
					{
						dwNetMask = 0xFFFFFF00;   // Class A, B or C
					}
					else
					{
						TCHAR szBuffer[16];

						// Multicast address??
						DbgPrintf(2, _T("Attempt to create interface object with multicast address %s"), 
									 IpToStr(dwIpAddr, szBuffer));
					}
				}

				// Create new subnet object
				if (dwIpAddr < 0xE0000000)
				{
					pSubnet = new Subnet(dwIpAddr & dwNetMask, dwNetMask, m_dwZoneGUID, bSyntheticMask);
					NetObjInsert(pSubnet, TRUE);
					g_pEntireNet->AddSubnet(pSubnet);
				}
			}
			else
			{
				// Set correct netmask if we was asked for it
				if (dwNetMask == 0)
				{
					dwNetMask = pSubnet->IpNetMask();
					bSyntheticMask = pSubnet->IsSyntheticMask();
				}
			}
		}
   }

   // Create interface object
   if (szName != NULL)
      pInterface = new Interface(szName, dwIndex, dwIpAddr, dwNetMask, dwType);
   else
      pInterface = new Interface(dwIpAddr, dwNetMask, bSyntheticMask);
   if (pbMacAddr != NULL)
      pInterface->SetMacAddr(pbMacAddr);

   // Insert to objects' list and generate event
   NetObjInsert(pInterface, TRUE);
   AddInterface(pInterface);
   if (!m_bIsHidden)
      pInterface->Unhide();
   PostEvent(EVENT_INTERFACE_ADDED, m_dwId, "dsaad", pInterface->Id(),
             pInterface->Name(), pInterface->IpAddr(),
             pInterface->IpNetMask(), pInterface->IfIndex());

   // Bind node to appropriate subnet
   if (pSubnet != NULL)
   {
      pSubnet->AddNode(this);
      
      // Check if subnet mask is correct on interface
      if ((pSubnet->IpNetMask() != pInterface->IpNetMask()) &&
			 (!pSubnet->IsSyntheticMask()))
		{
         PostEvent(EVENT_INCORRECT_NETMASK, m_dwId, "idsaa", pInterface->Id(),
                   pInterface->IfIndex(), pInterface->Name(),
                   pInterface->IpNetMask(), pSubnet->IpNetMask());
		}
   }
}


//
// Delete interface from node
//

void Node::DeleteInterface(Interface *pInterface)
{
   DWORD i;

   // Check if we should unlink node from interface's subnet
   if (pInterface->IpAddr() != 0)
   {
      BOOL bUnlink = TRUE;

      LockChildList(FALSE);
      for(i = 0; i < m_dwChildCount; i++)
         if (m_pChildList[i]->Type() == OBJECT_INTERFACE)
            if (m_pChildList[i] != pInterface)
               if ((((Interface *)m_pChildList[i])->IpAddr() & ((Interface *)m_pChildList[i])->IpNetMask()) ==
                   (pInterface->IpAddr() & pInterface->IpNetMask()))
               {
                  bUnlink = FALSE;
                  break;
               }
      UnlockChildList();
      
      if (bUnlink)
      {
         // Last interface in subnet, should unlink node
         Subnet *pSubnet = FindSubnetByIP(pInterface->IpAddr() & pInterface->IpNetMask());
         if (pSubnet != NULL)
         {
            DeleteParent(pSubnet);
            pSubnet->DeleteChild(this);
         }
      }
   }
   pInterface->Delete(FALSE);
}


//
// Calculate node status based on child objects status
//

void Node::CalculateCompoundStatus(BOOL bForcedRecalc)
{
   int iOldStatus = m_iStatus;
   static DWORD dwEventCodes[] = { EVENT_NODE_NORMAL, EVENT_NODE_MINOR,
      EVENT_NODE_WARNING, EVENT_NODE_MAJOR, EVENT_NODE_CRITICAL,
      EVENT_NODE_UNKNOWN, EVENT_NODE_UNMANAGED };

   NetObj::CalculateCompoundStatus(bForcedRecalc);
   if (m_iStatus != iOldStatus)
      PostEvent(dwEventCodes[m_iStatus], m_dwId, "d", iOldStatus);
}


//
// Perform status poll on node
//

void Node::statusPoll(ClientSession *pSession, DWORD dwRqId, int nPoller)
{
   DWORD i, dwPollListSize, dwOldFlags = m_dwFlags;
   NetObj *pPollerNode = NULL, **ppPollList;
   BOOL bAllDown;
   Queue *pQueue;    // Delayed event queue
	SNMP_Transport *pTransport;
	Cluster *pCluster;
   time_t tNow, tExpire;

   pQueue = new Queue;
   SetPollerInfo(nPoller, "wait for lock");
   pollerLock();
   m_pPollRequestor = pSession;
   SendPollerMsg(dwRqId, "Starting status poll for node %s\r\n", m_szName);
   DbgPrintf(5, "Starting status poll for node %s (ID: %d)", m_szName, m_dwId);

   // Read capability expiration time and current time
   tExpire = (time_t)ConfigReadULong(_T("CapabilityExpirationTime"), 604800);
   tNow = time(NULL);

   // Check SNMP agent connectivity
restart_agent_check:
   if ((m_dwFlags & NF_IS_SNMP) && (!(m_dwFlags & NF_DISABLE_SNMP)) && (m_dwIpAddr != 0))
   {
      TCHAR szBuffer[256];
      DWORD dwResult;

      DbgPrintf(6, "StatusPoll(%s): check SNMP", m_szName);
		pTransport = createSnmpTransport();
		if (pTransport == NULL)
		{
	      DbgPrintf(6, "StatusPoll(%s): cannot create SNMP transport", m_szName);
			goto skip_snmp_check;
		}

      SetPollerInfo(nPoller, "check SNMP");
      SendPollerMsg(dwRqId, "Checking SNMP agent connectivity\r\n");
      dwResult = SnmpGet(m_snmpVersion, pTransport, ".1.3.6.1.2.1.1.2.0",
		                   NULL, 0, szBuffer, 256, FALSE, FALSE);
      if ((dwResult == SNMP_ERR_SUCCESS) || (dwResult == SNMP_ERR_NO_OBJECT))
      {
         if (m_dwDynamicFlags & NDF_SNMP_UNREACHABLE)
         {
            m_dwDynamicFlags &= ~NDF_SNMP_UNREACHABLE;
            PostEventEx(pQueue, EVENT_SNMP_OK, m_dwId, NULL);
            SendPollerMsg(dwRqId, POLLER_INFO "Connectivity with SNMP agent restored\r\n");
         }
      }
      else
      {
         SendPollerMsg(dwRqId, POLLER_ERROR "SNMP agent unreachable\r\n");
         if (m_dwDynamicFlags & NDF_SNMP_UNREACHABLE)
         {
            if ((tNow > m_tFailTimeSNMP + tExpire) &&
                (!(m_dwDynamicFlags & NDF_UNREACHABLE)))
            {
               m_dwFlags &= ~NF_IS_SNMP;
               m_dwDynamicFlags &= ~NDF_SNMP_UNREACHABLE;
               m_szObjectId[0] = 0;
               SendPollerMsg(dwRqId, POLLER_WARNING "Attribute isSNMP set to FALSE\r\n");
            }
         }
         else
         {
            m_dwDynamicFlags |= NDF_SNMP_UNREACHABLE;
            PostEventEx(pQueue, EVENT_SNMP_FAIL, m_dwId, NULL);
            m_tFailTimeSNMP = tNow;
         }
      }
		delete pTransport;
      DbgPrintf(6, "StatusPoll(%s): SNMP check finished", m_szName);
   }

skip_snmp_check:
   // Check native agent connectivity
   if ((m_dwFlags & NF_IS_NATIVE_AGENT) && (!(m_dwFlags & NF_DISABLE_NXCP)) && (m_dwIpAddr != 0))
   {
      DbgPrintf(6, "StatusPoll(%s): checking agent", m_szName);
      SetPollerInfo(nPoller, "check agent");
      SendPollerMsg(dwRqId, "Checking NetXMS agent connectivity\r\n");

		DWORD error, socketError;
		agentLock();
      if (connectToAgent(&error, &socketError))
      {
         DbgPrintf(7, "StatusPoll(%s): connected to agent", m_szName);
         if (m_dwDynamicFlags & NDF_AGENT_UNREACHABLE)
         {
            m_dwDynamicFlags &= ~NDF_AGENT_UNREACHABLE;
            PostEventEx(pQueue, EVENT_AGENT_OK, m_dwId, NULL);
            SendPollerMsg(dwRqId, POLLER_INFO "Connectivity with NetXMS agent restored\r\n");
         }
      }
      else
      {
         DbgPrintf(6, "StatusPoll(%s): agent unreachable, error=%d, socketError=%d", m_szName, (int)error, (int)socketError);
         SendPollerMsg(dwRqId, POLLER_ERROR "NetXMS agent unreachable\r\n");
         if (m_dwDynamicFlags & NDF_AGENT_UNREACHABLE)
         {
            if ((tNow > m_tFailTimeAgent + tExpire) &&
                (!(m_dwDynamicFlags & NDF_UNREACHABLE)))
            {
               m_dwFlags &= ~NF_IS_NATIVE_AGENT;
               m_dwDynamicFlags &= ~NDF_AGENT_UNREACHABLE;
               m_szPlatformName[0] = 0;
               m_szAgentVersion[0] = 0;
               SendPollerMsg(dwRqId, POLLER_WARNING "Attribute isNetXMSAgent set to FALSE\r\n");
            }
         }
         else
         {
            m_dwDynamicFlags |= NDF_AGENT_UNREACHABLE;
            PostEventEx(pQueue, EVENT_AGENT_FAIL, m_dwId, NULL);
            m_tFailTimeAgent = tNow;
         }
      }
		agentUnlock();
      DbgPrintf(7, "StatusPoll(%s): agent check finished", m_szName);
   }

   SetPollerInfo(nPoller, "prepare polling list");

   // Find service poller node object
   LockData();
   if (m_dwPollerNode != 0)
   {
      pPollerNode = FindObjectById(m_dwPollerNode);
      if (pPollerNode != NULL)
      {
         if (pPollerNode->Type() != OBJECT_NODE)
            pPollerNode = NULL;
      }
   }
   UnlockData();

   // If nothing found, use management server
   if (pPollerNode == NULL)
   {
      pPollerNode = FindObjectById(g_dwMgmtNode);
      if (pPollerNode != NULL)
         pPollerNode->IncRefCount();
   }
   else
   {
      pPollerNode->IncRefCount();
   }

   // Create polling list
   ppPollList = (NetObj **)malloc(sizeof(NetObj *) * m_dwChildCount);
   LockChildList(FALSE);
   for(i = 0, dwPollListSize = 0; i < m_dwChildCount; i++)
      if (m_pChildList[i]->Status() != STATUS_UNMANAGED)
      {
         m_pChildList[i]->IncRefCount();
         ppPollList[dwPollListSize++] = m_pChildList[i];
      }
   UnlockChildList();

   // Poll interfaces and services
   SetPollerInfo(nPoller, "child poll");
   DbgPrintf(7, "StatusPoll(%s): starting child object poll", m_szName);
	pCluster = getMyCluster();
	pTransport = createSnmpTransport();
   for(i = 0; i < dwPollListSize; i++)
   {
      switch(ppPollList[i]->Type())
      {
         case OBJECT_INTERFACE:
			   DbgPrintf(7, "StatusPoll(%s): polling interface %d [%s]", m_szName, ppPollList[i]->Id(), ppPollList[i]->Name());
            ((Interface *)ppPollList[i])->StatusPoll(pSession, dwRqId, pQueue,
					(pCluster != NULL) ? pCluster->isSyncAddr(((Interface *)ppPollList[i])->IpAddr()) : FALSE,
					pTransport);
            break;
         case OBJECT_NETWORKSERVICE:
			   DbgPrintf(7, "StatusPoll(%s): polling network service %d [%s]", m_szName, ppPollList[i]->Id(), ppPollList[i]->Name());
            ((NetworkService *)ppPollList[i])->StatusPoll(pSession, dwRqId,
                                                          (Node *)pPollerNode, pQueue);
            break;
         default:
            break;
      }
      ppPollList[i]->DecRefCount();
   }
	delete pTransport;
   safe_free(ppPollList);
   DbgPrintf(7, "StatusPoll(%s): finished child object poll", m_szName);

   // Check if entire node is down
	// This check is disabled for nodes without IP address
	if (m_dwIpAddr != 0)
	{
		LockChildList(FALSE);
		if (m_dwChildCount > 0)
		{
			for(i = 0, bAllDown = TRUE; i < m_dwChildCount; i++)
				if ((m_pChildList[i]->Type() == OBJECT_INTERFACE) &&
					 (m_pChildList[i]->Status() != STATUS_CRITICAL) &&
					 (m_pChildList[i]->Status() != STATUS_UNKNOWN) &&
					 (m_pChildList[i]->Status() != STATUS_UNMANAGED) &&
					 (m_pChildList[i]->Status() != STATUS_DISABLED))
				{
					bAllDown = FALSE;
					break;
				}
		}
		else
		{
			bAllDown = FALSE;
		}
		UnlockChildList();
		if (bAllDown && (m_dwFlags & NF_IS_NATIVE_AGENT) &&
		    (!(m_dwFlags & NF_DISABLE_NXCP)))
		   if (!(m_dwDynamicFlags & NDF_AGENT_UNREACHABLE))
		      bAllDown = FALSE;
		if (bAllDown && (m_dwFlags & NF_IS_SNMP) &&
		    (!(m_dwFlags & NF_DISABLE_SNMP)))
		   if (!(m_dwDynamicFlags & NDF_SNMP_UNREACHABLE))
		      bAllDown = FALSE;
		if (bAllDown)
		{
		   if (!(m_dwDynamicFlags & NDF_UNREACHABLE))
		   {
		      m_dwDynamicFlags |= NDF_UNREACHABLE;
		      PostEvent(EVENT_NODE_DOWN, m_dwId, NULL);
		      SendPollerMsg(dwRqId, POLLER_ERROR "Node is unreachable\r\n");
		   }
		   else
		   {
		      SendPollerMsg(dwRqId, POLLER_WARNING "Node is still unreachable\r\n");
		   }
		}
		else
		{
		   if (m_dwDynamicFlags & NDF_UNREACHABLE)
		   {
		      m_dwDynamicFlags &= ~(NDF_UNREACHABLE | NDF_SNMP_UNREACHABLE | NDF_AGENT_UNREACHABLE);
		      PostEvent(EVENT_NODE_UP, m_dwId, NULL);
		      SendPollerMsg(dwRqId, POLLER_INFO "Node recovered from unreachable state\r\n");
				goto restart_agent_check;
		   }
		   else
		   {
		      SendPollerMsg(dwRqId, POLLER_INFO "Node is connected\r\n");
		   }
		}
	}

   // Send delayed events and destroy delayed event queue
   ResendEvents(pQueue);
   delete pQueue;
   
   SetPollerInfo(nPoller, "cleanup");
   if (pPollerNode != NULL)
      pPollerNode->DecRefCount();

   if (dwOldFlags != m_dwFlags)
   {
      PostEvent(EVENT_NODE_FLAGS_CHANGED, m_dwId, "xx", dwOldFlags, m_dwFlags);
      LockData();
      Modify();
      UnlockData();
   }

   CalculateCompoundStatus();
   m_tLastStatusPoll = time(NULL);
   SendPollerMsg(dwRqId, "Finished status poll for node %s\r\n", m_szName);
   SendPollerMsg(dwRqId, "Node status after poll is %s\r\n", g_szStatusText[m_iStatus]);
   m_pPollRequestor = NULL;
   if (dwRqId == 0)
      m_dwDynamicFlags &= ~NDF_QUEUED_FOR_STATUS_POLL;
   pollerUnlock();
   DbgPrintf(5, "Finished status poll for node %s (ID: %d)", m_szName, m_dwId);
}


//
// Check agent policy binding
// Intended to be called only from configuration poller
//

void Node::checkAgentPolicyBinding(AgentConnection *conn)
{
	AgentPolicyInfo *ap;
	DWORD rcc = conn->getPolicyInventory(&ap);
	if (rcc == ERR_SUCCESS)
	{
		// Check for unbound but installed policies
		for(int i = 0; i < ap->getSize(); i++)
		{
			uuid_t guid;
			ap->getGuid(i, guid);
			NetObj *object = FindObjectByGUID(guid, -1);
			if ((object != NULL) && (!object->IsChild(m_dwId)))
			{
				object->AddChild(this);
				AddParent(object);
				DbgPrintf(5, _T("ConfPoll(%s): bound to policy object %s [%d]"), m_szName, object->Name(), object->Id());
			}
		}

		// Check for bound but not installed policies
		LockParentList(FALSE);
		NetObj **unbindList = (NetObj **)malloc(sizeof(NetObj *) * m_dwParentCount);
		int unbindListSize = 0;
		for(DWORD i = 0; i < m_dwParentCount; i++)
		{
			if (IsAgentPolicyObject(m_pParentList[i]))
			{
				uuid_t guid1, guid2;
				int j;

				m_pParentList[i]->getGuid(guid1);
				for(j = 0; j < ap->getSize(); j++)
				{
					ap->getGuid(j, guid2);
					if (!uuid_compare(guid1, guid2))
						break;
				}
				if (j == ap->getSize())
					unbindList[unbindListSize++] = m_pParentList[i];
			}
		}
		UnlockParentList();

		for(int i = 0; i < unbindListSize; i++)
		{
			unbindList[i]->DeleteChild(this);
			DeleteParent(unbindList[i]);
			DbgPrintf(5, _T("ConfPoll(%s): unbound from policy object %s [%d]"), m_szName, unbindList[i]->Name(), unbindList[i]->Id());
		}
		safe_free(unbindList);

		delete ap;
	}
	else
	{
	   DbgPrintf(5, _T("ConfPoll(%s): AgentConnection::getPolicyInventory() failed: rcc=%d"), m_szName, rcc);
	}
}


//
// Perform configuration poll on node
//

void Node::configurationPoll(ClientSession *pSession, DWORD dwRqId,
                             int nPoller, DWORD dwNetMask)
{
   DWORD i, dwOldFlags = m_dwFlags, dwAddr, rcc, dwNumParams;
	NXC_AGENT_PARAM *pParamList;
   Interface **ppDeleteList;
   int j, iDelCount;
   AgentConnection *pAgentConn;
   INTERFACE_LIST *pIfList;
   char szBuffer[4096];
	Cluster *pCluster;
	SNMP_Transport *pTransport;
   BOOL bHasChanges = FALSE;

   SetPollerInfo(nPoller, "wait for lock");
   pollerLock();
   m_pPollRequestor = pSession;
   SendPollerMsg(dwRqId, _T("Starting configuration poll for node %s\r\n"), m_szName);
   DbgPrintf(4, "Starting configuration poll for node %s (ID: %d)", m_szName, m_dwId);

   // Check for forced capabilities recheck
   if (m_dwDynamicFlags & NDF_RECHECK_CAPABILITIES)
   {
      m_dwFlags &= ~(NF_IS_NATIVE_AGENT | NF_IS_SNMP | NF_IS_CPSNMP |
                     NF_IS_BRIDGE | NF_IS_ROUTER | NF_IS_OSPF);
		m_dwDynamicFlags &= ~NDF_CONFIGURATION_POLL_PASSED;
      m_szObjectId[0] = 0;
      m_szPlatformName[0] = 0;
      m_szAgentVersion[0] = 0;
		m_szSysDescription[0] = 0;
   }

   // Check if node is marked as unreachable
   if ((m_dwDynamicFlags & NDF_UNREACHABLE) && !(m_dwDynamicFlags & NDF_RECHECK_CAPABILITIES))
   {
      SendPollerMsg(dwRqId, POLLER_WARNING _T("Node is marked as unreachable, configuration poll aborted\r\n"));
      DbgPrintf(4, "Node is marked as unreachable, configuration poll aborted");
      m_tLastConfigurationPoll = time(NULL);
   }
   else
   {
      // Check node's capabilities
      SetPollerInfo(nPoller, "capability check");
      SendPollerMsg(dwRqId, _T("Checking node's capabilities...\r\n"));
      
		// ***** NetXMS agent check *****
      DbgPrintf(5, "ConfPoll(%s): checking for NetXMS agent Flags={%08X} DynamicFlags={%08X}", m_szName, m_dwFlags, m_dwDynamicFlags);
      if ((!((m_dwFlags & NF_IS_NATIVE_AGENT) && (m_dwDynamicFlags & NDF_AGENT_UNREACHABLE))) &&
          (!(m_dwFlags & NF_DISABLE_NXCP)) && (m_dwIpAddr != 0))
      {
	      SendPollerMsg(dwRqId, _T("   Checking NetXMS agent...\r\n"));
         pAgentConn = new AgentConnection(htonl(m_dwIpAddr), m_wAgentPort,
                                          m_wAuthMethod, m_szSharedSecret);
         setAgentProxy(pAgentConn);
         DbgPrintf(5, "ConfPoll(%s): checking for NetXMS agent - connecting", m_szName);
         if (pAgentConn->connect(g_pServerKey))
         {
            DbgPrintf(5, "ConfPoll(%s): checking for NetXMS agent - connected", m_szName);
            LockData();
            m_dwFlags |= NF_IS_NATIVE_AGENT;
            if (m_dwDynamicFlags & NDF_AGENT_UNREACHABLE)
            {
               m_dwDynamicFlags &= ~NDF_AGENT_UNREACHABLE;
               PostEvent(EVENT_AGENT_OK, m_dwId, NULL);
               SendPollerMsg(dwRqId, POLLER_INFO _T("   Connectivity with NetXMS agent restored\r\n"));
            }
				else
				{
	            SendPollerMsg(dwRqId, POLLER_INFO _T("   NetXMS native agent is active\r\n"));
				}
            UnlockData();
      
            if (pAgentConn->GetParameter("Agent.Version", MAX_AGENT_VERSION_LEN, szBuffer) == ERR_SUCCESS)
            {
               LockData();
               if (strcmp(m_szAgentVersion, szBuffer))
               {
                  strcpy(m_szAgentVersion, szBuffer);
                  bHasChanges = TRUE;
                  SendPollerMsg(dwRqId, _T("   NetXMS agent version changed to %s\r\n"), m_szAgentVersion);
               }
               UnlockData();
            }

            if (pAgentConn->GetParameter("System.PlatformName", MAX_PLATFORM_NAME_LEN, szBuffer) == ERR_SUCCESS)
            {
               LockData();
               if (strcmp(m_szPlatformName, szBuffer))
               {
                  strcpy(m_szPlatformName, szBuffer);
                  bHasChanges = TRUE;
                  SendPollerMsg(dwRqId, _T("   Platform name changed to %s\r\n"), m_szPlatformName);
               }
               UnlockData();
            }

            // Check IP forwarding status
            if (pAgentConn->GetParameter("Net.IP.Forwarding", 16, szBuffer) == ERR_SUCCESS)
            {
               if (_tcstoul(szBuffer, NULL, 10) != 0)
                  m_dwFlags |= NF_IS_ROUTER;
               else
                  m_dwFlags &= ~NF_IS_ROUTER;
            }

				// Get uname
				if (pAgentConn->GetParameter("System.Uname", MAX_DB_STRING, szBuffer) == ERR_SUCCESS)
				{
					TranslateStr(szBuffer, "\r\n", " ");
					TranslateStr(szBuffer, "\n", " ");
					TranslateStr(szBuffer, "\r", " ");
               LockData();
               if (strcmp(m_szSysDescription, szBuffer))
               {
                  strcpy(m_szSysDescription, szBuffer);
                  bHasChanges = TRUE;
                  SendPollerMsg(dwRqId, _T("   System description changed to %s\r\n"), m_szSysDescription);
               }
               UnlockData();
				}

            rcc = pAgentConn->GetSupportedParameters(&dwNumParams, &pParamList);
				if (rcc == ERR_SUCCESS)
				{
					LockData();
					safe_free(m_pParamList);
					m_dwNumParams = dwNumParams;
					m_pParamList = pParamList;
					UnlockData();
				}
				else
				{
				   DbgPrintf(5, _T("ConfPoll(%s): AgentConnection::GetSupportedParameters() failed: rcc=%d"), m_szName, rcc);
				}

				checkAgentPolicyBinding(pAgentConn);

            pAgentConn->disconnect();
         }
         delete pAgentConn;
         DbgPrintf(5, "ConfPoll(%s): checking for NetXMS agent - finished", m_szName);
      }

		// ***** SNMP check *****
		if ((!((m_dwFlags & NF_IS_SNMP) && (m_dwDynamicFlags & NDF_SNMP_UNREACHABLE))) &&
          (!(m_dwFlags & NF_DISABLE_SNMP)) && (m_dwIpAddr != 0))
      {
	      SendPollerMsg(dwRqId, _T("   Checking SNMP...\r\n"));
         DbgPrintf(5, "ConfPoll(%s): calling SnmpCheckCommSettings()", m_szName);
			pTransport = createSnmpTransport();
			if (pTransport == NULL)
			{
				DbgPrintf(5, "ConfPoll(%s): unable to create SNMP transport", m_szName);
				goto skip_snmp_checks;
			}

			SNMP_SecurityContext *newCtx = SnmpCheckCommSettings(pTransport, &m_snmpVersion, m_snmpSecurity);
			if (newCtx != NULL)
         {
            DWORD dwNodeFlags, dwNodeType;

            LockData();
				delete m_snmpSecurity;
				m_snmpSecurity = newCtx;
            m_dwFlags |= NF_IS_SNMP;
            if (m_dwDynamicFlags & NDF_SNMP_UNREACHABLE)
            {
               m_dwDynamicFlags &= ~NDF_SNMP_UNREACHABLE;
               PostEvent(EVENT_SNMP_OK, m_dwId, NULL);
               SendPollerMsg(dwRqId, POLLER_INFO _T("   Connectivity with SNMP agent restored\r\n"));
            }
				UnlockData();
            SendPollerMsg(dwRqId, _T("   SNMP agent is active (version %s)\r\n"), 
					(m_snmpVersion == SNMP_VERSION_3) ? _T("3") : ((m_snmpVersion == SNMP_VERSION_2C) ? _T("2c") : _T("1")));

				if (SnmpGet(m_snmpVersion, pTransport,
								".1.3.6.1.2.1.1.2.0", NULL, 0, szBuffer, 4096,
								FALSE, FALSE) == SNMP_ERR_SUCCESS)
				{
					LockData();
					if (strcmp(m_szObjectId, szBuffer))
					{
						nx_strncpy(m_szObjectId, szBuffer, MAX_OID_LEN * 4);
						bHasChanges = TRUE;
					}
					UnlockData();
				}

            // Check node type
				LockData();
            dwNodeType = OidToType(m_szObjectId, &dwNodeFlags);
            if (m_dwNodeType != dwNodeType)
            {
               m_dwFlags |= dwNodeFlags;
               m_dwNodeType = dwNodeType;
               SendPollerMsg(dwRqId, POLLER_WARNING _T("   Node type has been changed to %d\r\n"), m_dwNodeType);
               bHasChanges = TRUE;
            }
				UnlockData();

            // Check IP forwarding
            if (CheckSNMPIntegerValue(pTransport, ".1.3.6.1.2.1.4.1.0", 1))
            {
					LockData();
               m_dwFlags |= NF_IS_ROUTER;
					UnlockData();
            }
            else
            {
					LockData();
               m_dwFlags &= ~NF_IS_ROUTER;
					UnlockData();
            }

            // Check for bridge MIB support
            if (SnmpGet(m_snmpVersion, pTransport,
                        ".1.3.6.1.2.1.17.1.1.0", NULL, 0, szBuffer, 4096,
                        FALSE, FALSE) == SNMP_ERR_SUCCESS)
            {
					LockData();
               m_dwFlags |= NF_IS_BRIDGE;
					UnlockData();
            }
            else
            {
					LockData();
               m_dwFlags &= ~NF_IS_BRIDGE;
					UnlockData();
            }

            // Check for CDP (Cisco Discovery Protocol) support
            if (CheckSNMPIntegerValue(pTransport, ".1.3.6.1.4.1.9.9.23.1.3.1.0", 1))
            {
					LockData();
               m_dwFlags |= NF_IS_CDP;
					UnlockData();
            }
            else
            {
					LockData();
               m_dwFlags &= ~NF_IS_CDP;
					UnlockData();
            }

            // Check for SONMP (Nortel topology discovery discovery protocol) support
            if (CheckSNMPIntegerValue(pTransport, ".1.3.6.1.4.1.45.1.6.13.1.2.0", 1))
            {
					LockData();
               m_dwFlags |= NF_IS_SONMP;
					UnlockData();
            }
            else
            {
					LockData();
               m_dwFlags &= ~NF_IS_SONMP;
					UnlockData();
            }

		      // Check for LLDP (Link Layer Discovery Protocol) support
				if (SnmpGet(m_snmpVersion, pTransport,
								".1.0.8802.1.1.2.1.3.2.0", NULL, 0, szBuffer, 4096,
								FALSE, FALSE) == SNMP_ERR_SUCCESS)
				{
					LockData();
					m_dwFlags |= NF_IS_LLDP;
					UnlockData();
				}
				else
				{
					LockData();
					m_dwFlags &= ~NF_IS_LLDP;
					UnlockData();
				}

				// Get system description
				if (SnmpGet(m_snmpVersion, pTransport,
				            ".1.3.6.1.2.1.1.1.0", NULL, 0, szBuffer,
				            MAX_DB_STRING, FALSE, FALSE) == SNMP_ERR_SUCCESS)
				{
					TranslateStr(szBuffer, "\r\n", " ");
					TranslateStr(szBuffer, "\n", " ");
					TranslateStr(szBuffer, "\r", " ");
					LockData();
               if (strcmp(m_szSysDescription, szBuffer))
               {
                  strcpy(m_szSysDescription, szBuffer);
                  bHasChanges = TRUE;
                  SendPollerMsg(dwRqId, _T("   System description changed to %s\r\n"), m_szSysDescription);
               }
					UnlockData();
				}

            CheckOSPFSupport(pTransport);
         }
         else
         {
            // Check for CheckPoint SNMP agent on port 161
            DbgPrintf(5, "ConfPoll(%s): checking for CheckPoint SNMP", m_szName);
            if (SnmpGet(m_snmpVersion, pTransport,
                        ".1.3.6.1.4.1.2620.1.1.10.0", NULL, 0,
                        szBuffer, 4096, FALSE, FALSE) == SNMP_ERR_SUCCESS)
            {
               LockData();
               if (strcmp(m_szObjectId, ".1.3.6.1.4.1.2620.1.1"))
               {
                  nx_strncpy(m_szObjectId, ".1.3.6.1.4.1.2620.1.1", MAX_OID_LEN * 4);
                  bHasChanges = TRUE;
               }

               m_dwFlags |= NF_IS_SNMP | NF_IS_ROUTER;
               m_dwDynamicFlags &= ~NDF_SNMP_UNREACHABLE;
               UnlockData();
               SendPollerMsg(dwRqId, POLLER_INFO _T("   CheckPoint SNMP agent on port 161 is active\r\n"));
            }
         }
			delete pTransport;
      }

skip_snmp_checks:
      // Check for CheckPoint SNMP agent on port 260
      DbgPrintf(5, "ConfPoll(%s): checking for CheckPoint SNMP on port 260", m_szName);
      if (!((m_dwFlags & NF_IS_CPSNMP) && (m_dwDynamicFlags & NDF_CPSNMP_UNREACHABLE)) && (m_dwIpAddr != 0))
      {
			pTransport = new SNMP_UDPTransport;
			((SNMP_UDPTransport *)pTransport)->createUDPTransport(NULL, htonl(m_dwIpAddr), CHECKPOINT_SNMP_PORT);
         if (SnmpGet(SNMP_VERSION_1, pTransport,
                     ".1.3.6.1.4.1.2620.1.1.10.0", NULL, 0,
                     szBuffer, 4096, FALSE, FALSE) == SNMP_ERR_SUCCESS)
         {
            LockData();
            m_dwFlags |= NF_IS_CPSNMP | NF_IS_ROUTER;
            m_dwDynamicFlags &= ~NDF_CPSNMP_UNREACHABLE;
            UnlockData();
            SendPollerMsg(dwRqId, POLLER_INFO _T("   CheckPoint SNMP agent on port 260 is active\r\n"));
         }
			delete pTransport;
      }

      // Generate event if node flags has been changed
      if (dwOldFlags != m_dwFlags)
      {
         PostEvent(EVENT_NODE_FLAGS_CHANGED, m_dwId, "xx", dwOldFlags, m_dwFlags);
         bHasChanges = TRUE;
      }

		// Get parent cluster object, if any
		pCluster = getMyCluster();

      // Retrieve interface list
      SetPollerInfo(nPoller, "interface check");
      SendPollerMsg(dwRqId, _T("Capability check finished\r\n"));
      SendPollerMsg(dwRqId, _T("Checking interface configuration...\r\n"));
      pIfList = getInterfaceList();
      if (pIfList != NULL)
      {
			// Remove cluster virtual interfaces from list
			if (pCluster != NULL)
			{
				for(i = 0; i < (DWORD)pIfList->iNumEntries; i++)
				{
					if (pCluster->isVirtualAddr(pIfList->pInterfaces[i].dwIpAddr))
					{
						pIfList->iNumEntries--;
						memmove(&pIfList->pInterfaces[i], &pIfList->pInterfaces[i + 1],
						        sizeof(INTERFACE_INFO) * (pIfList->iNumEntries - i));
						i--;
					}
				}
			}

         // Find non-existing interfaces
         LockChildList(FALSE);
         ppDeleteList = (Interface **)malloc(sizeof(Interface *) * m_dwChildCount);
         for(i = 0, iDelCount = 0; i < m_dwChildCount; i++)
         {
            if (m_pChildList[i]->Type() == OBJECT_INTERFACE)
            {
               Interface *pInterface = (Interface *)m_pChildList[i];

               if (pInterface->IfType() != IFTYPE_NETXMS_NAT_ADAPTER)
               {
                  for(j = 0; j < pIfList->iNumEntries; j++)
                  {
                     if ((pIfList->pInterfaces[j].dwIndex == pInterface->IfIndex()) &&
                         (pIfList->pInterfaces[j].dwIpAddr == pInterface->IpAddr()) &&
                         (pIfList->pInterfaces[j].dwIpNetMask == pInterface->IpNetMask()))
                        break;
                  }

                  if (j == pIfList->iNumEntries)
                  {
                     // No such interface in current configuration, add it to delete list
                     ppDeleteList[iDelCount++] = pInterface;
                  }
               }
            }
         }
         UnlockChildList();

         // Delete non-existent interfaces
         if (iDelCount > 0)
         {
            for(j = 0; j < iDelCount; j++)
            {
               SendPollerMsg(dwRqId, POLLER_WARNING _T("   Interface \"%s\" is no longer exist\r\n"), 
                             ppDeleteList[j]->Name());
               PostEvent(EVENT_INTERFACE_DELETED, m_dwId, "dsaa", ppDeleteList[j]->IfIndex(),
                         ppDeleteList[j]->Name(), ppDeleteList[j]->IpAddr(), ppDeleteList[j]->IpNetMask());
               DeleteInterface(ppDeleteList[j]);
            }
            bHasChanges = TRUE;
         }
         safe_free(ppDeleteList);

         // Add new interfaces and check configuration of existing
         for(j = 0; j < pIfList->iNumEntries; j++)
         {
            BOOL bNewInterface = TRUE;

            LockChildList(FALSE);
            for(i = 0; i < m_dwChildCount; i++)
            {
               if (m_pChildList[i]->Type() == OBJECT_INTERFACE)
               {
                  Interface *pInterface = (Interface *)m_pChildList[i];

                  if ((pIfList->pInterfaces[j].dwIndex == pInterface->IfIndex()) &&
                      (pIfList->pInterfaces[j].dwIpAddr == pInterface->IpAddr()) &&
                      (pIfList->pInterfaces[j].dwIpNetMask == pInterface->IpNetMask()))
                  {
                     // Existing interface, check configuration
                     if (memcmp(pIfList->pInterfaces[j].bMacAddr, pInterface->MacAddr(), MAC_ADDR_LENGTH))
                     {
                        char szOldMac[16], szNewMac[16];

                        BinToStr((BYTE *)pInterface->MacAddr(), MAC_ADDR_LENGTH, szOldMac);
                        BinToStr(pIfList->pInterfaces[j].bMacAddr, MAC_ADDR_LENGTH, szNewMac);
                        PostEvent(EVENT_MAC_ADDR_CHANGED, m_dwId, "idsss",
                                  pInterface->Id(), pInterface->IfIndex(),
                                  pInterface->Name(), szOldMac, szNewMac);
                        pInterface->SetMacAddr(pIfList->pInterfaces[j].bMacAddr);
                     }
                     if (strcmp(pIfList->pInterfaces[j].szName, pInterface->Name()))
                     {
                        pInterface->SetName(pIfList->pInterfaces[j].szName);
                     }
                     bNewInterface = FALSE;
                     break;
                  }
               }
            }
            UnlockChildList();

            if (bNewInterface)
            {
               // New interface
               SendPollerMsg(dwRqId, POLLER_INFO _T("   Found new interface \"%s\"\r\n"), 
                             pIfList->pInterfaces[j].szName);
               CreateNewInterface(pIfList->pInterfaces[j].dwIpAddr, 
                                  pIfList->pInterfaces[j].dwIpNetMask,
                                  pIfList->pInterfaces[j].szName,
                                  pIfList->pInterfaces[j].dwIndex,
                                  pIfList->pInterfaces[j].dwType,
                                  pIfList->pInterfaces[j].bMacAddr);
               bHasChanges = TRUE;
            }
         }

         // Check if address we are using to communicate with node
         // is configured on one of node's interfaces
         for(i = 0; i < (DWORD)pIfList->iNumEntries; i++)
            if (pIfList->pInterfaces[i].dwIpAddr == m_dwIpAddr)
               break;

         if (i == (DWORD)pIfList->iNumEntries)
         {
            BOOL bCreate = TRUE;

            // Node is behind NAT
            m_dwFlags |= NF_BEHIND_NAT;

            // Check if we already have NAT interface
            LockChildList(FALSE);
            for(i = 0; i < m_dwChildCount; i++)
               if (m_pChildList[i]->Type() == OBJECT_INTERFACE)
               {
                  if (((Interface *)m_pChildList[i])->IfType() == IFTYPE_NETXMS_NAT_ADAPTER)
                  {
                     bCreate = FALSE;
                     break;
                  }
               }
            UnlockChildList();

            if (bCreate)
            {
               char szBuffer[MAX_OBJECT_NAME];

               // Create pseudo interface for NAT
               ConfigReadStr("NATAdapterName", szBuffer, MAX_OBJECT_NAME, "NetXMS NAT Adapter");
               CreateNewInterface(m_dwIpAddr, 0, szBuffer,
                                  0x7FFFFFFF, IFTYPE_NETXMS_NAT_ADAPTER);
               bHasChanges = TRUE;
            }
         }
         else
         {
            // Check if NF_BEHIND_NAT flag set incorrectly
            if (m_dwFlags & NF_BEHIND_NAT)
            {
               Interface *pIfNat;

               // Remove NAT interface
               LockChildList(FALSE);
               for(i = 0, pIfNat = NULL; i < m_dwChildCount; i++)
                  if (m_pChildList[i]->Type() == OBJECT_INTERFACE)
                  {
                     if (((Interface *)m_pChildList[i])->IfType() == IFTYPE_NETXMS_NAT_ADAPTER)
                     {
                        pIfNat = (Interface *)m_pChildList[i];
                        break;
                     }
                  }
               UnlockChildList();

               if (pIfNat != NULL)
                  DeleteInterface(pIfNat);

               m_dwFlags &= ~NF_BEHIND_NAT;
               bHasChanges = TRUE;
            }
         }

			CheckSubnetBinding(pIfList);

         DestroyInterfaceList(pIfList);
      }
      else     /* pIfList == NULL */
      {
         Interface *pInterface;
         DWORD dwCount;

         SendPollerMsg(dwRqId, POLLER_ERROR _T("Unable to get interface list from node\r\n"));

         // Delete all existing interfaces in case of forced capability recheck
         if (m_dwDynamicFlags & NDF_RECHECK_CAPABILITIES)
         {
            LockChildList(FALSE);
            ppDeleteList = (Interface **)malloc(sizeof(Interface *) * m_dwChildCount);
            for(i = 0, iDelCount = 0; i < m_dwChildCount; i++)
            {
               if (m_pChildList[i]->Type() == OBJECT_INTERFACE)
                  ppDeleteList[iDelCount++] = (Interface *)m_pChildList[i];
            }
            UnlockChildList();
            for(j = 0; j < iDelCount; j++)
            {
               SendPollerMsg(dwRqId, POLLER_WARNING _T("   Interface \"%s\" is no longer exist\r\n"), 
                             ppDeleteList[j]->Name());
               PostEvent(EVENT_INTERFACE_DELETED, m_dwId, "dsaa", ppDeleteList[j]->IfIndex(),
                         ppDeleteList[j]->Name(), ppDeleteList[j]->IpAddr(), ppDeleteList[j]->IpNetMask());
               DeleteInterface(ppDeleteList[j]);
            }
            safe_free(ppDeleteList);
         }

         // Check if we have pseudo-interface object
         dwCount = getInterfaceCount(&pInterface);
         if (dwCount == 1)
         {
            if (pInterface->IsFake())
            {
               // Check if primary IP is different from interface's IP
               if (pInterface->IpAddr() != m_dwIpAddr)
               {
                  DeleteInterface(pInterface);
						if (m_dwIpAddr != 0)
	                  CreateNewInterface(m_dwIpAddr, dwNetMask);
               }
            }
         }
         else if (dwCount == 0)
         {
            // No interfaces at all, create pseudo-interface
				if (m_dwIpAddr != 0)
            	CreateNewInterface(m_dwIpAddr, dwNetMask);
         }
      }

      m_tLastConfigurationPoll = time(NULL);
      SendPollerMsg(dwRqId, _T("Interface configuration check finished\r\n"));

		// Check node name
		SendPollerMsg(dwRqId, _T("Checking node name\r\n"));
		dwAddr = ntohl(_t_inet_addr(m_szName));
		if ((g_dwFlags & AF_RESOLVE_NODE_NAMES) &&
			 (dwAddr != INADDR_NONE) && 
			 (dwAddr != INADDR_ANY) &&
			 isMyIP(dwAddr))
		{
			SendPollerMsg(dwRqId, _T("Node name is an IP address and need to be resolved\r\n"));
	      SetPollerInfo(nPoller, "resolving name");
			if (ResolveName(FALSE))
			{
				SendPollerMsg(dwRqId, POLLER_INFO _T("Node name resolved to %s\r\n"), m_szName);
				bHasChanges = TRUE;
			}
			else
			{
				SendPollerMsg(dwRqId, POLLER_WARNING _T("Node name cannot be resolved\r\n"));
			}
		}
		else
		{
			if (g_dwFlags & AF_SYNC_NODE_NAMES_WITH_DNS)
			{
				SendPollerMsg(dwRqId, _T("Syncing node name with DNS\r\n"));
		      SetPollerInfo(nPoller, "resolving name");
				if (ResolveName(TRUE))
				{
					SendPollerMsg(dwRqId, POLLER_INFO _T("Node name resolved to %s\r\n"), m_szName);
					bHasChanges = TRUE;
				}
			}
			else
			{
				SendPollerMsg(dwRqId, _T("Node name is OK\r\n"));
			}
		}

		// Apply templates
		ApplySystemTemplates();
		ApplyUserTemplates();

		updateContainerMembership();

		SendPollerMsg(dwRqId, _T("Finished configuration poll for node %s\r\n"), m_szName);
		SendPollerMsg(dwRqId, _T("Node configuration was%schanged after poll\r\n"), bHasChanges ? _T(" ") : _T(" not "));

		m_dwDynamicFlags |= NDF_CONFIGURATION_POLL_PASSED;
   }

   // Finish configuration poll
   SetPollerInfo(nPoller, "cleanup");
   if (dwRqId == 0)
      m_dwDynamicFlags &= ~NDF_QUEUED_FOR_CONFIG_POLL;
   m_dwDynamicFlags &= ~NDF_RECHECK_CAPABILITIES;
   pollerUnlock();
   DbgPrintf(4, "Finished configuration poll for node %s (ID: %d)", m_szName, m_dwId);

   if (bHasChanges)
   {
      LockData();
      Modify();
      UnlockData();
   }
}


//
// Apply system templates
//

void Node::ApplySystemTemplates()
{
	Template *pTemplate;

	pTemplate = FindTemplateByName(_T("@System.Agent"));
	if (pTemplate != NULL)
	{
		if (isNativeAgent())
		{
			if (!pTemplate->IsChild(m_dwId))
			{
				DbgPrintf(4, _T("Node::ApplySystemTemplates(): applying template %d \"%s\" to node %d \"%s\""),
				          pTemplate->Id(), pTemplate->Name(), m_dwId, m_szName);
				pTemplate->ApplyToNode(this);
			}
		}
		else
		{
			if (pTemplate->IsChild(m_dwId))
			{
				DbgPrintf(4, _T("Node::ApplySystemTemplates(): removing template %d \"%s\" from node %d \"%s\""),
				          pTemplate->Id(), pTemplate->Name(), m_dwId, m_szName);
				pTemplate->DeleteChild(this);
				DeleteParent(pTemplate);
				pTemplate->queueRemoveFromNode(m_dwId, TRUE);
			}
		}
	}

	pTemplate = FindTemplateByName(_T("@System.SNMP"));
	if (pTemplate != NULL)
	{
		if (isSNMPSupported())
		{
			if (!pTemplate->IsChild(m_dwId))
			{
				DbgPrintf(4, _T("Node::ApplySystemTemplates(): applying template %d \"%s\" to node %d \"%s\""),
				          pTemplate->Id(), pTemplate->Name(), m_dwId, m_szName);
				pTemplate->ApplyToNode(this);
			}
		}
		else
		{
			if (pTemplate->IsChild(m_dwId))
			{
				DbgPrintf(4, _T("Node::ApplySystemTemplates(): removing template %d \"%s\" from node %d \"%s\""),
				          pTemplate->Id(), pTemplate->Name(), m_dwId, m_szName);
				pTemplate->DeleteChild(this);
				DeleteParent(pTemplate);
				pTemplate->queueRemoveFromNode(m_dwId, TRUE);
			}
		}
	}
}


//
// Apply user templates
//

void Node::ApplyUserTemplates()
{
   DWORD i;
   Template *pTemplate;

   if (g_pIndexById == NULL)
      return;

   RWLockReadLock(g_rwlockIdIndex, INFINITE);
   for(i = 0; i < g_dwIdIndexSize; i++)
   {
      if ((((NetObj *)g_pIndexById[i].pObject)->Type() == OBJECT_TEMPLATE) &&
			 (!((NetObj *)g_pIndexById[i].pObject)->IsDeleted()))
      {
         pTemplate = (Template *)g_pIndexById[i].pObject;
			if (pTemplate->isApplicable(this))
			{
				if (!pTemplate->IsChild(m_dwId))
				{
					DbgPrintf(4, _T("Node::ApplyUserTemplates(): applying template %d \"%s\" to node %d \"%s\""),
					          pTemplate->Id(), pTemplate->Name(), m_dwId, m_szName);
					pTemplate->ApplyToNode(this);
				}
			}
			else
			{
				if (pTemplate->isAutoApplyEnabled() && pTemplate->IsChild(m_dwId))
				{
					DbgPrintf(4, _T("Node::ApplyUserTemplates(): removing template %d \"%s\" from node %d \"%s\""),
					          pTemplate->Id(), pTemplate->Name(), m_dwId, m_szName);
					pTemplate->DeleteChild(this);
					DeleteParent(pTemplate);
					pTemplate->queueRemoveFromNode(m_dwId, TRUE);
				}
			}
      }
   }
   RWLockUnlock(g_rwlockIdIndex);
}


//
// Update container membership
//

void Node::updateContainerMembership()
{
   DWORD i;
   Container *pContainer;

   if (g_pIndexById == NULL)
      return;

   RWLockReadLock(g_rwlockIdIndex, INFINITE);
   for(i = 0; i < g_dwIdIndexSize; i++)
   {
      if ((((NetObj *)g_pIndexById[i].pObject)->Type() == OBJECT_CONTAINER) &&
			 (!((NetObj *)g_pIndexById[i].pObject)->IsDeleted()))
      {
         pContainer = (Container *)g_pIndexById[i].pObject;
			if (pContainer->IsSuitableForNode(this))
			{
				if (!pContainer->IsChild(m_dwId))
				{
					DbgPrintf(4, _T("Node::UpdateContainerMembership(): binding node %d \"%s\" to container %d \"%s\""),
					          m_dwId, m_szName, pContainer->Id(), pContainer->Name());
					pContainer->AddChild(this);
					AddParent(pContainer);
				}
			}
			else
			{
				if (pContainer->IsAutoBindEnabled() && pContainer->IsChild(m_dwId))
				{
					DbgPrintf(4, _T("Node::UpdateContainerMembership(): removing node %d \"%s\" from container %d \"%s\""),
					          m_dwId, m_szName, pContainer->Id(), pContainer->Name());
					pContainer->DeleteChild(this);
					DeleteParent(pContainer);
				}
			}
      }
   }
   RWLockUnlock(g_rwlockIdIndex);
}


//
// Connect to native agent
//

BOOL Node::connectToAgent(DWORD *error, DWORD *socketError)
{
   BOOL bRet;

   // Create new agent connection object if needed
   if (m_pAgentConnection == NULL)
      m_pAgentConnection = new AgentConnectionEx(htonl(m_dwIpAddr), m_wAgentPort, m_wAuthMethod, m_szSharedSecret);

   // Check if we already connected
   if (m_pAgentConnection->nop() == ERR_SUCCESS)
      return TRUE;

   // Close current connection or clean up after broken connection
   m_pAgentConnection->disconnect();
   m_pAgentConnection->setPort(m_wAgentPort);
   m_pAgentConnection->setAuthData(m_wAuthMethod, m_szSharedSecret);
   setAgentProxy(m_pAgentConnection);
   bRet = m_pAgentConnection->connect(g_pServerKey, FALSE, error, socketError);
   if (bRet)
	{
		m_pAgentConnection->setCommandTimeout(g_dwAgentCommandTimeout);
      m_pAgentConnection->enableTraps();
	}
   return bRet;
}


//
// Get item's value via SNMP
//

DWORD Node::GetItemFromSNMP(WORD port, const char *szParam, DWORD dwBufSize, char *szBuffer)
{
   DWORD dwResult;

   if ((((m_dwDynamicFlags & NDF_SNMP_UNREACHABLE) || !(m_dwFlags & NF_IS_SNMP)) && (port == 0)) ||
       (m_dwDynamicFlags & NDF_UNREACHABLE) ||
		 (m_dwFlags & NF_DISABLE_SNMP))
   {
      dwResult = SNMP_ERR_COMM;
   }
   else
   {
		SNMP_Transport *pTransport;

		pTransport = createSnmpTransport(port);
		if (pTransport != NULL)
		{
			dwResult = SnmpGet(m_snmpVersion, pTransport,
									 szParam, NULL, 0, szBuffer, dwBufSize, FALSE, TRUE);
			delete pTransport;
		}
		else
		{
			dwResult = SNMP_ERR_COMM;
		}
   }
   DbgPrintf(7, "Node(%s)->GetItemFromSNMP(%s): dwResult=%d", m_szName, szParam, dwResult);
   return (dwResult == SNMP_ERR_SUCCESS) ? DCE_SUCCESS : 
      ((dwResult == SNMP_ERR_NO_OBJECT) ? DCE_NOT_SUPPORTED : DCE_COMM_ERROR);
}


//
// Get item's value via SNMP from CheckPoint's agent
//

DWORD Node::GetItemFromCheckPointSNMP(const char *szParam, DWORD dwBufSize, char *szBuffer)
{
   DWORD dwResult;

   if ((m_dwDynamicFlags & NDF_CPSNMP_UNREACHABLE) ||
       (m_dwDynamicFlags & NDF_UNREACHABLE))
   {
      dwResult = SNMP_ERR_COMM;
   }
   else
   {
		SNMP_Transport *pTransport;

		pTransport = new SNMP_UDPTransport;
		((SNMP_UDPTransport *)pTransport)->createUDPTransport(NULL, htonl(m_dwIpAddr), CHECKPOINT_SNMP_PORT);
      dwResult = SnmpGet(SNMP_VERSION_1, pTransport,
                         szParam, NULL, 0, szBuffer,
                         dwBufSize, FALSE, TRUE);
		delete pTransport;
   }
   DbgPrintf(7, "Node(%s)->GetItemFromCheckPointSNMP(%s): dwResult=%d", m_szName, szParam, dwResult);
   return (dwResult == SNMP_ERR_SUCCESS) ? DCE_SUCCESS : 
      ((dwResult == SNMP_ERR_NO_OBJECT) ? DCE_NOT_SUPPORTED : DCE_COMM_ERROR);
}


//
// Get item's value via native agent
//

DWORD Node::GetItemFromAgent(const char *szParam, DWORD dwBufSize, char *szBuffer)
{
   DWORD dwError = ERR_NOT_CONNECTED, dwResult = DCE_COMM_ERROR;
   DWORD dwTries = 3;

   if ((m_dwDynamicFlags & NDF_AGENT_UNREACHABLE) ||
       (m_dwDynamicFlags & NDF_UNREACHABLE) ||
		 (m_dwFlags & NF_DISABLE_NXCP) ||
		 !(m_dwFlags & NF_IS_NATIVE_AGENT))
      return DCE_COMM_ERROR;

   agentLock();

   // Establish connection if needed
   if (m_pAgentConnection == NULL)
      if (!connectToAgent())
         goto end_loop;

   // Get parameter from agent
   while(dwTries-- > 0)
   {
      dwError = m_pAgentConnection->GetParameter((char *)szParam, dwBufSize, szBuffer);
      switch(dwError)
      {
         case ERR_SUCCESS:
            dwResult = DCE_SUCCESS;
            goto end_loop;
         case ERR_UNKNOWN_PARAMETER:
            dwResult = DCE_NOT_SUPPORTED;
            goto end_loop;
         case ERR_NOT_CONNECTED:
         case ERR_CONNECTION_BROKEN:
            if (!connectToAgent())
               goto end_loop;
            break;
         case ERR_REQUEST_TIMEOUT:
				// Reset connection to agent after timeout
				DbgPrintf(7, _T("Node(%s)->GetItemFromAgent(%s): timeout; resetting connection to agent..."), m_szName, szParam);
				delete_and_null(m_pAgentConnection);
            if (!connectToAgent())
               goto end_loop;
				DbgPrintf(7, _T("Node(%s)->GetItemFromAgent(%s): connection to agent restored successfully"), m_szName, szParam);
            break;
      }
   }

end_loop:
   agentUnlock();
   DbgPrintf(7, "Node(%s)->GetItemFromAgent(%s): dwError=%d dwResult=%d",
             m_szName, szParam, dwError, dwResult);
   return dwResult;
}


//
// Get value for server's internal parameter
//

DWORD Node::GetInternalItem(const char *szParam, DWORD dwBufSize, char *szBuffer)
{
   DWORD dwError = DCE_SUCCESS;

   if (!stricmp(szParam, "Status"))
   {
      sprintf(szBuffer, "%d", m_iStatus);
   }
   else if (!stricmp(szParam, "Dummy"))
   {
      _tcscpy(szBuffer, _T("0"));
   }
   else if (!stricmp(szParam, "AgentStatus"))
   {
      if (m_dwFlags & NF_IS_NATIVE_AGENT)
      {
         szBuffer[0] = (m_dwDynamicFlags & NDF_AGENT_UNREACHABLE) ? '1' : '0';
         szBuffer[1] = 0;
      }
      else
      {
         dwError = DCE_NOT_SUPPORTED;
      }
   }
   else if (MatchString("ChildStatus(*)", szParam, FALSE))
   {
      char *pEnd, szArg[256];
      DWORD i, dwId;
      NetObj *pObject = NULL;

      AgentGetParameterArg((char *)szParam, 1, szArg, 256);
      dwId = strtoul(szArg, &pEnd, 0);
      if (*pEnd != 0)
      {
         // Argument is object's name
         dwId = 0;
      }

      // Find child object with requested ID or name
      LockChildList(FALSE);
      for(i = 0; i < m_dwChildCount; i++)
      {
         if (((dwId == 0) && (!stricmp(m_pChildList[i]->Name(), szArg))) ||
             (dwId == m_pChildList[i]->Id()))
         {
            pObject = m_pChildList[i];
            break;
         }
      }
      UnlockChildList();

      if (pObject != NULL)
      {
         sprintf(szBuffer, "%d", pObject->Status());
      }
      else
      {
         dwError = DCE_NOT_SUPPORTED;
      }
   }
   else if (MatchString("ConditionStatus(*)", szParam, FALSE))
   {
      char *pEnd, szArg[256];
      DWORD dwId;
      NetObj *pObject = NULL;

      AgentGetParameterArg((char *)szParam, 1, szArg, 256);
      dwId = strtoul(szArg, &pEnd, 0);
      if (*pEnd == 0)
		{
			pObject = FindObjectById(dwId);
			if (pObject != NULL)
				if (pObject->Type() != OBJECT_CONDITION)
					pObject = NULL;
		}
		else
      {
         // Argument is object's name
			pObject = FindObjectByName(szArg, OBJECT_CONDITION);
      }

      if (pObject != NULL)
      {
			if (pObject->IsTrustedNode(m_dwId))
			{
				sprintf(szBuffer, "%d", pObject->Status());
			}
			else
			{
	         dwError = DCE_NOT_SUPPORTED;
			}
      }
      else
      {
         dwError = DCE_NOT_SUPPORTED;
      }
   }
   else if (m_dwFlags & NF_IS_LOCAL_MGMT)
   {
      if (!stricmp(szParam, "Server.AverageDCPollerQueueSize"))
      {
         sprintf(szBuffer, "%f", g_dAvgPollerQueueSize);
      }
      else if (!stricmp(szParam, "Server.AverageDBWriterQueueSize"))
      {
         sprintf(szBuffer, "%f", g_dAvgDBWriterQueueSize);
      }
      else if (!stricmp(szParam, "Server.AverageStatusPollerQueueSize"))
      {
         sprintf(szBuffer, "%f", g_dAvgStatusPollerQueueSize);
      }
      else if (!stricmp(szParam, "Server.AverageConfigurationPollerQueueSize"))
      {
         sprintf(szBuffer, "%f", g_dAvgConfigPollerQueueSize);
      }
      else if (!stricmp(szParam, "Server.AverageDCIQueuingTime"))
      {
         sprintf(szBuffer, "%u", g_dwAvgDCIQueuingTime);
      }
      else if (!stricmp(szParam, "Server.TotalEventsProcessed"))
      {
         sprintf(szBuffer, INT64_FMT, g_totalEventsProcessed);
      }
      else
      {
         dwError = DCE_NOT_SUPPORTED;
      }
   }
   else
   {
      dwError = DCE_NOT_SUPPORTED;
   }

   return dwError;
}


//
// Get item's value for client
//

DWORD Node::GetItemForClient(int iOrigin, const char *pszParam, char *pszBuffer, DWORD dwBufSize)
{
   DWORD dwResult = 0, dwRetCode;

   // Get data from node
   switch(iOrigin)
   {
      case DS_INTERNAL:
         dwRetCode = GetInternalItem(pszParam, dwBufSize, pszBuffer);
         break;
      case DS_NATIVE_AGENT:
         dwRetCode = GetItemFromAgent(pszParam, dwBufSize, pszBuffer);
         break;
      case DS_SNMP_AGENT:
         dwRetCode = GetItemFromSNMP(0, pszParam, dwBufSize, pszBuffer);
         break;
      case DS_CHECKPOINT_AGENT:
         dwRetCode = GetItemFromCheckPointSNMP(pszParam, dwBufSize, pszBuffer);
         break;
      default:
         dwResult = RCC_INVALID_ARGUMENT;
         break;
   }

   // Translate return code to RCC
   if (dwResult != RCC_INVALID_ARGUMENT)
   {
      switch(dwRetCode)
      {
         case DCE_SUCCESS:
            dwResult = RCC_SUCCESS;
            break;
         case DCE_COMM_ERROR:
            dwResult = RCC_COMM_FAILURE;
            break;
         case DCE_NOT_SUPPORTED:
            dwResult = RCC_DCI_NOT_SUPPORTED;
            break;
         default:
            dwResult = RCC_SYSTEM_FAILURE;
            break;
      }
   }

   return dwResult;
}


//
// Put items which requires polling into the queue
//

void Node::queueItemsForPolling(Queue *pPollerQueue)
{
   DWORD i;
   time_t currTime;

   if ((m_iStatus == STATUS_UNMANAGED) ||
	    (m_dwFlags & NF_DISABLE_DATA_COLLECT) ||
		 (m_bIsDeleted))
      return;  // Do not collect data for unmanaged nodes or if data collection is disabled

   currTime = time(NULL);

   lockDciAccess();
   for(i = 0; i < m_dwNumItems; i++)
   {
      if (m_ppItems[i]->isReadyForPolling(currTime))
      {
         m_ppItems[i]->setBusyFlag(TRUE);
         IncRefCount();   // Increment reference count for each queued DCI
         pPollerQueue->Put(m_ppItems[i]);
			DbgPrintf(8, _T("Node(%s)->QueueItemsForPolling(): item %d \"%s\" added to queue"), m_szName, m_ppItems[i]->getId(), m_ppItems[i]->getName());
      }
   }
   unlockDciAccess();
}


//
// Create CSCP message with object's data
//

void Node::CreateMessage(CSCPMessage *pMsg)
{
   Template::CreateMessage(pMsg);
   pMsg->SetVariable(VID_FLAGS, m_dwFlags);
   pMsg->SetVariable(VID_RUNTIME_FLAGS, m_dwDynamicFlags);
   pMsg->SetVariable(VID_AGENT_PORT, m_wAgentPort);
   pMsg->SetVariable(VID_AUTH_METHOD, m_wAuthMethod);
   pMsg->SetVariable(VID_SHARED_SECRET, m_szSharedSecret);
	pMsg->SetVariable(VID_SNMP_AUTH_OBJECT, m_snmpSecurity->getCommunity());
	pMsg->SetVariable(VID_SNMP_AUTH_PASSWORD, m_snmpSecurity->getAuthPassword());
	pMsg->SetVariable(VID_SNMP_PRIV_PASSWORD, m_snmpSecurity->getPrivPassword());
	pMsg->SetVariable(VID_SNMP_USM_METHODS, (WORD)((WORD)m_snmpSecurity->getAuthMethod() | ((WORD)m_snmpSecurity->getPrivMethod() << 8)));
   pMsg->SetVariable(VID_SNMP_OID, m_szObjectId);
   pMsg->SetVariable(VID_SNMP_PORT, m_wSNMPPort);
   pMsg->SetVariable(VID_NODE_TYPE, m_dwNodeType);
   pMsg->SetVariable(VID_SNMP_VERSION, (WORD)m_snmpVersion);
   pMsg->SetVariable(VID_AGENT_VERSION, m_szAgentVersion);
   pMsg->SetVariable(VID_PLATFORM_NAME, m_szPlatformName);
   pMsg->SetVariable(VID_POLLER_NODE_ID, m_dwPollerNode);
   pMsg->SetVariable(VID_ZONE_GUID, m_dwZoneGUID);
   pMsg->SetVariable(VID_PROXY_NODE, m_dwProxyNode);
   pMsg->SetVariable(VID_SNMP_PROXY, m_dwSNMPProxy);
	pMsg->SetVariable(VID_REQUIRED_POLLS, (WORD)m_iRequiredPollCount);
	pMsg->SetVariable(VID_SYS_DESCRIPTION, m_szSysDescription);
	pMsg->SetVariable(VID_USE_IFXTABLE, (WORD)m_nUseIfXTable);
}


//
// Modify object from message
//

DWORD Node::ModifyFromMessage(CSCPMessage *pRequest, BOOL bAlreadyLocked)
{
	TCHAR buffer[256];

   if (!bAlreadyLocked)
      LockData();

   // Change primary IP address
   if (pRequest->IsVariableExist(VID_IP_ADDRESS))
   {
      DWORD i, dwIpAddr;
      
      dwIpAddr = pRequest->GetVariableLong(VID_IP_ADDRESS);

      // Check if received IP address is one of node's interface addresses
      LockChildList(FALSE);
      for(i = 0; i < m_dwChildCount; i++)
         if ((m_pChildList[i]->Type() == OBJECT_INTERFACE) &&
             (m_pChildList[i]->IpAddr() == dwIpAddr))
            break;
      UnlockChildList();
      if (i == m_dwChildCount)
      {
         UnlockData();
         return RCC_INVALID_IP_ADDR;
      }

      UpdateNodeIndex(m_dwIpAddr, dwIpAddr, this);
      m_dwIpAddr = dwIpAddr;
   }

   // Poller node ID
   if (pRequest->IsVariableExist(VID_POLLER_NODE_ID))
   {
      DWORD dwNodeId;
      NetObj *pObject;
      
      dwNodeId = pRequest->GetVariableLong(VID_POLLER_NODE_ID);
      pObject = FindObjectById(dwNodeId);

      // Check if received id is a valid node id
      if (pObject == NULL)
      {
         UnlockData();
         return RCC_INVALID_OBJECT_ID;
      }
      if (pObject->Type() != OBJECT_NODE)
      {
         UnlockData();
         return RCC_INVALID_OBJECT_ID;
      }

      m_dwPollerNode = dwNodeId;
   }

   // Change listen port of native agent
   if (pRequest->IsVariableExist(VID_AGENT_PORT))
      m_wAgentPort = pRequest->GetVariableShort(VID_AGENT_PORT);

   // Change authentication method of native agent
   if (pRequest->IsVariableExist(VID_AUTH_METHOD))
      m_wAuthMethod = pRequest->GetVariableShort(VID_AUTH_METHOD);

   // Change shared secret of native agent
   if (pRequest->IsVariableExist(VID_SHARED_SECRET))
      pRequest->GetVariableStr(VID_SHARED_SECRET, m_szSharedSecret, MAX_SECRET_LENGTH);

   // Change SNMP protocol version
   if (pRequest->IsVariableExist(VID_SNMP_VERSION))
	{
      m_snmpVersion = pRequest->GetVariableShort(VID_SNMP_VERSION);
		m_snmpSecurity->setSecurityModel((m_snmpVersion == SNMP_VERSION_3) ? SNMP_SECURITY_MODEL_USM : SNMP_SECURITY_MODEL_V2C);
	}

   // Change SNMP port
   if (pRequest->IsVariableExist(VID_SNMP_PORT))
		m_wSNMPPort = pRequest->GetVariableShort(VID_SNMP_PORT);

   // Change SNMP authentication data
   if (pRequest->IsVariableExist(VID_SNMP_AUTH_OBJECT))
	{
      pRequest->GetVariableStr(VID_SNMP_AUTH_OBJECT, buffer, 256);
		m_snmpSecurity->setAuthName(buffer);

		pRequest->GetVariableStr(VID_SNMP_AUTH_PASSWORD, buffer, 256);
		m_snmpSecurity->setAuthPassword(buffer);

		pRequest->GetVariableStr(VID_SNMP_PRIV_PASSWORD, buffer, 256);
		m_snmpSecurity->setPrivPassword(buffer);

		WORD methods = pRequest->GetVariableShort(VID_SNMP_USM_METHODS);
		m_snmpSecurity->setAuthMethod((int)(methods & 0xFF));
		m_snmpSecurity->setPrivMethod((int)(methods >> 8));
	}

   // Change proxy node
   if (pRequest->IsVariableExist(VID_PROXY_NODE))
      m_dwProxyNode = pRequest->GetVariableLong(VID_PROXY_NODE);

   // Change SNMP proxy node
   if (pRequest->IsVariableExist(VID_SNMP_PROXY))
      m_dwSNMPProxy = pRequest->GetVariableLong(VID_SNMP_PROXY);

   // Number of required polls
   if (pRequest->IsVariableExist(VID_REQUIRED_POLLS))
      m_iRequiredPollCount = (int)pRequest->GetVariableShort(VID_REQUIRED_POLLS);

   // Enable/disable usage of ifXTable
   if (pRequest->IsVariableExist(VID_USE_IFXTABLE))
      m_nUseIfXTable = (BYTE)pRequest->GetVariableShort(VID_USE_IFXTABLE);

   // Change flags
   if (pRequest->IsVariableExist(VID_FLAGS))
   {
      m_dwFlags &= NF_SYSTEM_FLAGS;
      m_dwFlags |= pRequest->GetVariableLong(VID_FLAGS) & NF_USER_FLAGS;
   }

   return Template::ModifyFromMessage(pRequest, TRUE);
}


//
// Wakeup node using magic packet
//

DWORD Node::wakeUp()
{
   DWORD i, dwResult = RCC_NO_WOL_INTERFACES;

   LockChildList(FALSE);

   for(i = 0; i < m_dwChildCount; i++)
      if ((m_pChildList[i]->Type() == OBJECT_INTERFACE) &&
          (m_pChildList[i]->Status() != STATUS_UNMANAGED) &&
          (m_pChildList[i]->IpAddr() != 0))
      {
         dwResult = ((Interface *)m_pChildList[i])->wakeUp();
         break;
      }

   UnlockChildList();
   return dwResult;
}


//
// Get status of interface with given index from SNMP agent
//

int Node::getInterfaceStatusFromSNMP(SNMP_Transport *pTransport, DWORD dwIndex)
{
   return SnmpGetInterfaceStatus(m_snmpVersion, pTransport, dwIndex);
}


//
// Get status of interface with given index from native agent
//

int Node::getInterfaceStatusFromAgent(DWORD dwIndex)
{
   char szParam[128], szBuffer[32];
   DWORD dwAdminStatus, dwLinkState;
   int iStatus;

   // Get administrative status
   sprintf(szParam, "Net.Interface.AdminStatus(%u)", dwIndex);
   if (GetItemFromAgent(szParam, 32, szBuffer) == DCE_SUCCESS)
   {
      dwAdminStatus = strtoul(szBuffer, NULL, 0);

      switch(dwAdminStatus)
      {
         case 3:
            iStatus = STATUS_TESTING;
            break;
         case 2:
         case 0:     // Agents before 0.2.1 may return 0 instead of 2
            iStatus = STATUS_DISABLED;
            break;
         case 1:     // Interface administratively up, check link state
            sprintf(szParam, "Net.Interface.Link(%u)", dwIndex);
            if (GetItemFromAgent(szParam, 32, szBuffer) == DCE_SUCCESS)
            {
               dwLinkState = strtoul(szBuffer, NULL, 0);
               iStatus = (dwLinkState == 0) ? STATUS_CRITICAL : STATUS_NORMAL;
            }
            else
            {
               iStatus = STATUS_UNKNOWN;
            }
            break;
         default:
            iStatus = STATUS_UNKNOWN;
            break;
      }
   }
   else
   {
      iStatus = STATUS_UNKNOWN;
   }

   return iStatus;
}


//
// Put list of supported parameters into CSCP message
//

void Node::WriteParamListToMessage(CSCPMessage *pMsg)
{
   DWORD i, dwId;

   LockData();
   if (m_pParamList != NULL)
   {
      pMsg->SetVariable(VID_NUM_PARAMETERS, m_dwNumParams);
      for(i = 0, dwId = VID_PARAM_LIST_BASE; i < m_dwNumParams; i++)
      {
         pMsg->SetVariable(dwId++, m_pParamList[i].szName);
         pMsg->SetVariable(dwId++, m_pParamList[i].szDescription);
         pMsg->SetVariable(dwId++, (WORD)m_pParamList[i].iDataType);
      }
		DbgPrintf(6, _T("Node[%s]::WriteParamListToMessage(): sending %d parameters"), m_szName, m_dwNumParams);
   }
   else
   {
		DbgPrintf(6, _T("Node[%s]::WriteParamListToMessage(): m_pParamList == NULL"), m_szName);
      pMsg->SetVariable(VID_NUM_PARAMETERS, (DWORD)0);
   }
   UnlockData();
}


//
// Open list of supported parameters for reading
//

void Node::OpenParamList(DWORD *pdwNumParams, NXC_AGENT_PARAM **ppParamList)
{
   LockData();
   *pdwNumParams = m_dwNumParams;
   *ppParamList = m_pParamList;
}


//
// Check status of network service
//

DWORD Node::CheckNetworkService(DWORD *pdwStatus, DWORD dwIpAddr, int iServiceType,
                                WORD wPort, WORD wProto, TCHAR *pszRequest,
                                TCHAR *pszResponse)
{
   DWORD dwError = ERR_NOT_CONNECTED;

   if ((m_dwFlags & NF_IS_NATIVE_AGENT) &&
       (!(m_dwDynamicFlags & NDF_AGENT_UNREACHABLE)) &&
       (!(m_dwDynamicFlags & NDF_UNREACHABLE)))
   {
      AgentConnection *pConn;

      pConn = createAgentConnection();
      if (pConn != NULL)
      {
         dwError = pConn->CheckNetworkService(pdwStatus, dwIpAddr, iServiceType,
                                              wPort, wProto, pszRequest, pszResponse);
         pConn->disconnect();
         delete pConn;
      }
   }
   return dwError;
}


//
// Handler for object deletion
//

void Node::OnObjectDelete(DWORD dwObjectId)
{
	LockData();
   if (dwObjectId == m_dwPollerNode)
   {
      // If deleted object is our poller node, change it to default
      m_dwPollerNode = 0;
      Modify();
      DbgPrintf(3, _T("Node \"%s\": poller node %d deleted"), m_szName, dwObjectId);
   }
	UnlockData();
}


//
// Check node for OSPF support
//

void Node::CheckOSPFSupport(SNMP_Transport *pTransport)
{
   LONG nAdminStatus;

   if (SnmpGet(m_snmpVersion, pTransport,
               ".1.3.6.1.2.1.14.1.2.0", NULL, 0, &nAdminStatus, sizeof(LONG),
               FALSE, FALSE) == SNMP_ERR_SUCCESS)
   {
		LockData();
      if (nAdminStatus)
      {
         m_dwFlags |= NF_IS_OSPF;
      }
      else
      {
         m_dwFlags &= ~NF_IS_OSPF;
      }
		UnlockData();
   }
}


//
// Create ready to use agent connection
//

AgentConnectionEx *Node::createAgentConnection()
{
   AgentConnectionEx *conn;

   if ((!(m_dwFlags & NF_IS_NATIVE_AGENT)) ||
	    (m_dwFlags & NF_DISABLE_NXCP) ||
       (m_dwDynamicFlags & NDF_AGENT_UNREACHABLE) ||
       (m_dwDynamicFlags & NDF_UNREACHABLE))
      return NULL;

   conn = new AgentConnectionEx(htonl(m_dwIpAddr), m_wAgentPort,
                                m_wAuthMethod, m_szSharedSecret);
   setAgentProxy(conn);
   if (!conn->connect(g_pServerKey))
   {
      delete conn;
      conn = NULL;
   }
   return conn;
}


//
// Get last collected values of all DCIs
//

DWORD Node::getLastValues(CSCPMessage *pMsg)
{
   DWORD i, dwId, dwCount;

   lockDciAccess();

   for(i = 0, dwId = VID_DCI_VALUES_BASE, dwCount = 0; i < m_dwNumItems; i++)
	{
		if (_tcsnicmp(m_ppItems[i]->getDescription(), _T("@system."), 8))
		{
			m_ppItems[i]->getLastValue(pMsg, dwId);
			dwId += 10;
			dwCount++;
		}
	}
   pMsg->SetVariable(VID_NUM_ITEMS, dwCount);

   unlockDciAccess();
   return RCC_SUCCESS;
}


//
// Clean expired DCI data
//

void Node::cleanDCIData()
{
   DWORD i;

   lockDciAccess();
   for(i = 0; i < m_dwNumItems; i++)
      m_ppItems[i]->deleteExpiredData();
   unlockDciAccess();
}


//
// Apply DCI from template
// pItem passed to this method should be a template's DCI
//

BOOL Node::applyTemplateItem(DWORD dwTemplateId, DCItem *pItem)
{
   BOOL bResult = TRUE;
   DWORD i;
   DCItem *pNewItem;

   lockDciAccess();	// write lock

   DbgPrintf(5, "Applying item \"%s\" to node \"%s\"", pItem->getName(), m_szName);

   // Check if that template item exists
   for(i = 0; i < m_dwNumItems; i++)
      if ((m_ppItems[i]->getTemplateId() == dwTemplateId) &&
          (m_ppItems[i]->getTemplateItemId() == pItem->getId()))
         break;   // Item with specified id already exist

   if (i == m_dwNumItems)
   {
      // New item from template, just add it
      pNewItem = new DCItem(pItem);
      pNewItem->setTemplateId(dwTemplateId, pItem->getId());
      pNewItem->changeBinding(CreateUniqueId(IDG_ITEM), this, TRUE);
      bResult = addItem(pNewItem, true);
   }
   else
   {
      // Update existing item
      m_ppItems[i]->updateFromTemplate(pItem);
   }

   unlockDciAccess();

	if (bResult)
	{
		LockData();
		m_bIsModified = TRUE;
		UnlockData();
	}
   return bResult;
}


//
// Clean deleted template items from node's DCI list
// Arguments is template id and list of valid template item ids.
// all items related to given template and not presented in list should be deleted.
//

void Node::cleanDeletedTemplateItems(DWORD dwTemplateId, DWORD dwNumItems, DWORD *pdwItemList)
{
   DWORD i, j, dwNumDeleted, *pdwDeleteList;

   lockDciAccess();  // write lock

   pdwDeleteList = (DWORD *)malloc(sizeof(DWORD) * m_dwNumItems);
   dwNumDeleted = 0;

   for(i = 0; i < m_dwNumItems; i++)
      if (m_ppItems[i]->getTemplateId() == dwTemplateId)
      {
         for(j = 0; j < dwNumItems; j++)
            if (m_ppItems[i]->getTemplateItemId() == pdwItemList[j])
               break;

         // Delete DCI if it's not in list
         if (j == dwNumItems)
            pdwDeleteList[dwNumDeleted++] = m_ppItems[i]->getId();
      }

   for(i = 0; i < dwNumDeleted; i++)
      deleteItem(pdwDeleteList[i], false);

   unlockDciAccess();
   free(pdwDeleteList);
}


//
// Unbind node from template, i.e either remove DCI association with template
// or remove these DCIs at all
//

void Node::unbindFromTemplate(DWORD dwTemplateId, BOOL bRemoveDCI)
{
   DWORD i;

   if (bRemoveDCI)
   {
		DWORD *pdwDeleteList = (DWORD *)malloc(sizeof(DWORD) * m_dwNumItems);
		DWORD dwNumDeleted = 0;

      lockDciAccess();  // write lock

      for(i = 0; i < m_dwNumItems; i++)
         if (m_ppItems[i]->getTemplateId() == dwTemplateId)
         {
            pdwDeleteList[dwNumDeleted++] = m_ppItems[i]->getId();
         }

		for(i = 0; i < dwNumDeleted; i++)
			deleteItem(pdwDeleteList[i], false);

      unlockDciAccess();

		safe_free(pdwDeleteList);
   }
   else
   {
      lockDciAccess();

      for(i = 0; i < m_dwNumItems; i++)
         if (m_ppItems[i]->getTemplateId() == dwTemplateId)
         {
            m_ppItems[i]->setTemplateId(0, 0);
         }

      unlockDciAccess();
   }
}


//
// Change node's IP address
//

void Node::changeIPAddress(DWORD dwIpAddr)
{
   DWORD i;

   pollerLock();

   LockData();

   UpdateNodeIndex(m_dwIpAddr, dwIpAddr, this);
   m_dwIpAddr = dwIpAddr;
   m_dwDynamicFlags |= NDF_FORCE_CONFIGURATION_POLL | NDF_RECHECK_CAPABILITIES;

   // Change status of node and all it's childs to UNKNOWN
   m_iStatus = STATUS_UNKNOWN;
   LockChildList(FALSE);
   for(i = 0; i < m_dwChildCount; i++)
   {
      m_pChildList[i]->ResetStatus();
      if (m_pChildList[i]->Type() == OBJECT_INTERFACE)
      {
         if (((Interface *)m_pChildList[i])->IsFake())
         {
            ((Interface *)m_pChildList[i])->SetIpAddr(dwIpAddr);
         }
      }
   }
   UnlockChildList();

   Modify();
   UnlockData();

   agentLock();
   delete_and_null(m_pAgentConnection);
   agentUnlock();

   pollerUnlock();
}


//
// Get number of interface objects and pointer to the last one
//

DWORD Node::getInterfaceCount(Interface **ppInterface)
{
   DWORD i, dwCount;

   LockChildList(FALSE);
   for(i = 0, dwCount = 0; i < m_dwChildCount; i++)
      if (m_pChildList[i]->Type() == OBJECT_INTERFACE)
      {
         dwCount++;
         *ppInterface = (Interface *)m_pChildList[i];
      }
   UnlockChildList();
   return dwCount;
}


//
// Update cache for all DCI's
//

void Node::updateDciCache()
{
   DWORD i;

	lockDciAccess();
   for(i = 0; i < m_dwNumItems; i++)
      m_ppItems[i]->updateCacheSize();
	unlockDciAccess();
}


//
// Get routing table from node
//

ROUTING_TABLE *Node::getRoutingTable()
{
   ROUTING_TABLE *pRT = NULL;

   if ((m_dwFlags & NF_IS_NATIVE_AGENT) && (!(m_dwFlags & NF_DISABLE_NXCP)))
   {
      agentLock();
      if (connectToAgent())
      {
         pRT = m_pAgentConnection->GetRoutingTable();
      }
      agentUnlock();
   }
   if ((pRT == NULL) && (m_dwFlags & NF_IS_SNMP) && (!(m_dwFlags & NF_DISABLE_SNMP)))
   {
		SNMP_Transport *pTransport;

		pTransport = createSnmpTransport();
		if (pTransport != NULL)
		{
			pRT = SnmpGetRoutingTable(m_snmpVersion, pTransport);
			delete pTransport;
		}
   }

   if (pRT != NULL)
   {
      SortRoutingTable(pRT);
   }
   return pRT;
}


//
// Get next hop for given destination address
//

BOOL Node::getNextHop(DWORD dwSrcAddr, DWORD dwDestAddr, DWORD *pdwNextHop,
                      DWORD *pdwIfIndex, BOOL *pbIsVPN)
{
   DWORD i;
   BOOL bResult = FALSE;

   // Check VPN connectors
   LockChildList(FALSE);
   for(i = 0; i < m_dwChildCount; i++)
      if (m_pChildList[i]->Type() == OBJECT_VPNCONNECTOR)
      {
         if (((VPNConnector *)m_pChildList[i])->IsRemoteAddr(dwDestAddr) &&
             ((VPNConnector *)m_pChildList[i])->IsLocalAddr(dwSrcAddr))
         {
            *pdwNextHop = ((VPNConnector *)m_pChildList[i])->GetPeerGatewayAddr();
            *pdwIfIndex = m_pChildList[i]->Id();
            *pbIsVPN = TRUE;
            bResult = TRUE;
            break;
         }
      }
   UnlockChildList();

   // Check routing table
   if (!bResult)
   {
      routingTableLock();
      if (m_pRoutingTable != NULL)
      {
         for(i = 0; i < (DWORD)m_pRoutingTable->iNumEntries; i++)
            if ((dwDestAddr & m_pRoutingTable->pRoutes[i].dwDestMask) == m_pRoutingTable->pRoutes[i].dwDestAddr)
            {
               *pdwNextHop = m_pRoutingTable->pRoutes[i].dwNextHop;
               *pdwIfIndex = m_pRoutingTable->pRoutes[i].dwIfIndex;
               *pbIsVPN = FALSE;
               bResult = TRUE;
               break;
            }
      }
      routingTableUnlock();
   }

   return bResult;
}


//
// Update cached routing table
//

void Node::updateRoutingTable()
{
   ROUTING_TABLE *pRT;

   pRT = getRoutingTable();
   if (pRT != NULL)
   {
      routingTableLock();
      DestroyRoutingTable(m_pRoutingTable);
      m_pRoutingTable = pRT;
      routingTableUnlock();
   }
   m_tLastRTUpdate = time(NULL);
   m_dwDynamicFlags &= ~NDF_QUEUED_FOR_ROUTE_POLL;
}


//
// Call SNMP Enumerate with node's SNMP parameters
//

DWORD Node::CallSnmpEnumerate(const char *pszRootOid, 
                              DWORD (* pHandler)(DWORD, SNMP_Variable *, SNMP_Transport *, void *),
                              void *pArg)
{
   if ((m_dwFlags & NF_IS_SNMP) && 
       (!(m_dwDynamicFlags & NDF_SNMP_UNREACHABLE)) &&
       (!(m_dwDynamicFlags & NDF_UNREACHABLE)))
	{
		SNMP_Transport *pTransport;
		DWORD dwResult;

		pTransport = createSnmpTransport();
		if (pTransport != NULL)
		{
			dwResult = SnmpEnumerate(m_snmpVersion, pTransport,
											 pszRootOid, pHandler, pArg, FALSE);
			delete pTransport;
		}
		else
		{
	      dwResult = SNMP_ERR_COMM;
		}
		return dwResult;
	}
   else
	{
      return SNMP_ERR_COMM;
	}
}


//
// Set proxy information for agent's connection
//

void Node::setAgentProxy(AgentConnection *pConn)
{
   if (m_dwProxyNode != 0)
   {
      Node *pNode;

      pNode = (Node *)FindObjectById(m_dwProxyNode);
      if (pNode != NULL)
      {
         pConn->setProxy(htonl(pNode->m_dwIpAddr), pNode->m_wAgentPort,
                         pNode->m_wAuthMethod, pNode->m_szSharedSecret);
      }
   }
}


//
// Prepare node object for deletion
//

void Node::PrepareForDeletion(void)
{
   // Prevent node from being queued for polling
   LockData();
   m_dwDynamicFlags |= NDF_POLLING_DISABLED;
   UnlockData();

   // Wait for all pending polls
   while(1)
   {
      LockData();
      if ((m_dwDynamicFlags & 
            (NDF_QUEUED_FOR_STATUS_POLL | NDF_QUEUED_FOR_CONFIG_POLL |
             NDF_QUEUED_FOR_DISCOVERY_POLL | NDF_QUEUED_FOR_ROUTE_POLL)) == 0)
      {
         UnlockData();
         break;
      }
      UnlockData();
      ThreadSleepMs(100);
   }
	Template::PrepareForDeletion();
}


//
// Check if specified SNMP variable set to specified value.
// If variable doesn't exist at all, will return FALSE
//

BOOL Node::CheckSNMPIntegerValue(SNMP_Transport *pTransport, const char *pszOID, int nValue)
{
   DWORD dwTemp;

   if (SnmpGet(m_snmpVersion, pTransport, pszOID, NULL, 0, &dwTemp,
               sizeof(DWORD), FALSE, FALSE) == SNMP_ERR_SUCCESS)
      return (int)dwTemp == nValue;
   return FALSE;
}


//
// Check and update if needed interface names
//

void Node::CheckInterfaceNames(INTERFACE_LIST *pIfList)
{
   int i;
   TCHAR *ptr;

   if ((m_dwNodeType == NODE_TYPE_NORTEL_BAYSTACK) ||
		 (m_dwNodeType == NODE_TYPE_NORTEL_OPTERA))
   {
      // Translate interface names
      for(i = 0; i < pIfList->iNumEntries; i++)
      {
         if ((ptr = _tcsstr(pIfList->pInterfaces[i].szName, _T("- Port"))) != NULL)
			{
				ptr += 2;
            memmove(pIfList->pInterfaces[i].szName, ptr, _tcslen(ptr) + 1);
			}
         else if ((ptr = _tcsstr(pIfList->pInterfaces[i].szName, _T("- Unit"))) != NULL)
			{
				ptr += 2;
            memmove(pIfList->pInterfaces[i].szName, ptr, _tcslen(ptr) + 1);
			}
         else if ((_tcsstr(pIfList->pInterfaces[i].szName, _T("BayStack")) != NULL) ||
                  (_tcsstr(pIfList->pInterfaces[i].szName, _T("Nortel Ethernet Switch")) != NULL))
         {
            ptr = _tcsrchr(pIfList->pInterfaces[i].szName, _T('-'));
            if (ptr != NULL)
            {
               ptr++;
               while(*ptr == _T(' '))
                  ptr++;
               memmove(pIfList->pInterfaces[i].szName, ptr, _tcslen(ptr) + 1);
            }
         }
			StrStrip(pIfList->pInterfaces[i].szName);
      }
   }

   // Cut interface names to MAX_OBJECT_NAME and check for unnamed interfaces
   for(i = 0; i < pIfList->iNumEntries; i++)
   {
      pIfList->pInterfaces[i].szName[MAX_OBJECT_NAME - 1] = 0;
      if (pIfList->pInterfaces[i].szName[0] == 0)
         _stprintf(pIfList->pInterfaces[i].szName, _T("%d"), pIfList->pInterfaces[i].dwIndex);
   }
}


//
// Get cluster object this node belongs to, if any
//

Cluster *Node::getMyCluster()
{
	DWORD i;
	Cluster *pCluster = NULL;

	LockParentList(FALSE);
	for(i = 0; i < m_dwParentCount; i++)
		if (m_pParentList[i]->Type() == OBJECT_CLUSTER)
		{
			pCluster = (Cluster *)m_pParentList[i];
			break;
		}
	UnlockParentList();
	return pCluster;
}


//
// Create SNMP transport
//

SNMP_Transport *Node::createSnmpTransport(WORD port)
{
	SNMP_Transport *pTransport = NULL;

	if (m_dwSNMPProxy == 0)
	{
		pTransport = new SNMP_UDPTransport;
		((SNMP_UDPTransport *)pTransport)->createUDPTransport(NULL, htonl(m_dwIpAddr), (port != 0) ? port : m_wSNMPPort);
	}
	else
	{
		NetObj *pObject;

		pObject = FindObjectById(m_dwSNMPProxy);
		if (pObject != NULL)
		{
			if (pObject->Type() == OBJECT_NODE)
			{
				AgentConnection *pConn;

				pConn = ((Node *)pObject)->createAgentConnection();
				if (pConn != NULL)
				{
					pTransport = new SNMP_ProxyTransport(pConn, m_dwIpAddr, (port != 0) ? port : m_wSNMPPort);
				}
			}
		}
	}

	// Set security
	if (pTransport != NULL)
	{
		LockData();
		pTransport->setSecurityContext(new SNMP_SecurityContext(m_snmpSecurity));
		UnlockData();
	}
	return pTransport;
}


//
// Get SNMP security context
// ATTENTION: This method returns new copy of security context
// which must be destroyed by the caller
//

SNMP_SecurityContext *Node::getSnmpSecurityContext()
{
	LockData();
	SNMP_SecurityContext *ctx = new SNMP_SecurityContext(m_snmpSecurity);
	UnlockData();
	return ctx;
}


//
// Resolve node's name
//

BOOL Node::ResolveName(BOOL useOnlyDNS)
{
	BOOL bSuccess = FALSE;
	HOSTENT *hs;
	DWORD i, dwAddr;
	TCHAR szBuffer[256];

	DbgPrintf(4, _T("Resolving name for node %d [%s]..."), m_dwId, m_szName);

	// Try to resolve primary IP
	dwAddr = htonl(m_dwIpAddr);
	hs = gethostbyaddr((const char *)&dwAddr, 4, AF_INET);
	if (hs != NULL)
	{
		nx_strncpy(m_szName, hs->h_name, MAX_OBJECT_NAME);
		bSuccess = TRUE;
	}
	else
	{
		// Try to resolve each interface's IP address
		LockChildList(FALSE);
		for(i = 0; i < m_dwChildCount; i++)
		{
			if (m_pChildList[i]->Type() == OBJECT_INTERFACE)
			{
				dwAddr = htonl(m_pChildList[i]->IpAddr());
				if (dwAddr != 0)
				{
					hs = gethostbyaddr((const char *)&dwAddr, 4, AF_INET);
					if (hs != NULL)
					{
						nx_strncpy(m_szName, hs->h_name, MAX_OBJECT_NAME);
						bSuccess = TRUE;
						break;
					}
				}
			}
		}
		UnlockChildList();

		// Try to get hostname from agent if address resolution fails
		if (!(bSuccess || useOnlyDNS))
		{
			DbgPrintf(4, _T("Resolving name for node %d [%s] via agent..."), m_dwId, m_szName);
			if (GetItemFromAgent("System.Hostname", 256, szBuffer) == DCE_SUCCESS)
			{
				StrStrip(szBuffer);
				if (szBuffer[0] != 0)
				{
					nx_strncpy(m_szName, szBuffer, MAX_OBJECT_NAME);
					bSuccess = TRUE;
				}
			}
		}

		// Try to get hostname from SNMP if other methods fails
		if (!(bSuccess || useOnlyDNS))
		{
			DbgPrintf(4, _T("Resolving name for node %d [%s] via SNMP..."), m_dwId, m_szName);
			if (GetItemFromSNMP(0, ".1.3.6.1.2.1.1.5.0", 256, szBuffer) == DCE_SUCCESS)
			{
				StrStrip(szBuffer);
				if (szBuffer[0] != 0)
				{
					nx_strncpy(m_szName, szBuffer, MAX_OBJECT_NAME);
					bSuccess = TRUE;
				}
			}
		}
	}

	if (bSuccess)
		DbgPrintf(4, _T("Name for node %d was resolved to %s"), m_dwId, m_szName);
	else
		DbgPrintf(4, _T("Name for node %d was not resolved"), m_dwId, m_szName);
	return bSuccess;
}


//
// Send list of system DCIs
//

DWORD Node::getPerfTabDCIList(CSCPMessage *pMsg)
{
   DWORD i, dwId, dwCount;

   LockData();

   for(i = 0, dwId = VID_SYSDCI_LIST_BASE, dwCount = 0; i < m_dwNumItems; i++)
	{
		if (!_tcsnicmp(m_ppItems[i]->getDescription(), _T("@System."), 8) ||
			 (m_ppItems[i]->getPerfTabSettings() != NULL))
		{
			pMsg->SetVariable(dwId++, m_ppItems[i]->getId());
			pMsg->SetVariable(dwId++, (TCHAR *)m_ppItems[i]->getDescription());
			pMsg->SetVariable(dwId++, (WORD)m_ppItems[i]->getStatus());
			if (m_ppItems[i]->getPerfTabSettings() != NULL)
			{
				pMsg->SetVariable(dwId++, m_ppItems[i]->getPerfTabSettings());
				dwId += 6;
			}
			else
			{
				dwId += 7;
			}
			dwCount++;
		}
	}
   pMsg->SetVariable(VID_NUM_ITEMS, dwCount);

   UnlockData();
   return RCC_SUCCESS;
}


//
// Get current layer 2 topology (as dynamically created list which should be destroyed by caller)
// Will return NULL if there are no topology information or it is expired
//

nxmap_ObjList *Node::GetL2Topology(void)
{
	nxmap_ObjList *pResult;
	DWORD dwExpTime;

	dwExpTime = ConfigReadULong(_T("TopologyExpirationTime"), 900);
	MutexLock(m_mutexTopoAccess, INFINITE);
	if ((m_pTopology == NULL) || (m_tLastTopologyPoll + (time_t)dwExpTime < time(NULL)))
	{
		pResult = NULL;
	}
	else
	{
		pResult = new nxmap_ObjList(m_pTopology);
	}
	MutexUnlock(m_mutexTopoAccess);
	return pResult;
}


//
// Rebuild layer 2 topology and return it as dynamically reated list which should be destroyed by caller
//

nxmap_ObjList *Node::BuildL2Topology(DWORD *pdwStatus)
{
	nxmap_ObjList *pResult;
	int nDepth;

	nDepth = ConfigReadInt(_T("TopologyDiscoveryRadius"), 5);
	MutexLock(m_mutexTopoAccess, INFINITE);
	delete m_pTopology;
	if ((m_dwFlags & NF_IS_CDP) || (m_dwFlags & NF_IS_SONMP) || (m_dwFlags & NF_IS_LLDP))
	{
		m_pTopology = new nxmap_ObjList;
		if ((*pdwStatus = ::BuildL2Topology(*m_pTopology, this, NULL, nDepth, NULL)) == RCC_SUCCESS)
		{
			m_tLastTopologyPoll = time(NULL);
			pResult = new nxmap_ObjList(m_pTopology);
		}
		else
		{
			delete_and_null(m_pTopology);
			pResult = NULL;
		}
	}
	else
	{
		pResult = NULL;
		m_pTopology = NULL;
		*pdwStatus = RCC_NO_L2_TOPOLOGY_SUPPORT;
	}
	MutexUnlock(m_mutexTopoAccess);
	return pResult;
}


//
// Check subnet bindings
//

void Node::CheckSubnetBinding(INTERFACE_LIST *pIfList)
{
	Subnet *pSubnet;
	Interface *pInterface;
	Cluster *pCluster;
	NetObj **ppUnlinkList;
	int i, j, count;
	BOOL isSync;

	pCluster = getMyCluster();

	// Check if we have subnet bindings for all interfaces
	for(i = 0; i < pIfList->iNumEntries; i++)
	{
		if (pIfList->pInterfaces[i].dwIpAddr != 0)
		{
			pInterface = findInterface(pIfList->pInterfaces[i].dwIndex, pIfList->pInterfaces[i].dwIpAddr);
			if (pInterface == NULL)
			{
				nxlog_write(MSG_INTERNAL_ERROR, EVENTLOG_WARNING_TYPE, "s", _T("Cannot find interface object in Node::CheckSubnetBinding()"));
				break;	// Something goes really wrong
			}

			// Is cluster interconnect interface?
			isSync = (pCluster != NULL) ? pCluster->isSyncAddr(pInterface->IpAddr()) : FALSE;

			pSubnet = FindSubnetForNode(pIfList->pInterfaces[i].dwIpAddr);
			if (pSubnet != NULL)
			{
				if (isSync)
				{
					pSubnet = NULL;	// No further checks on this subnet
				}
				else
				{
					if (pSubnet->IsSyntheticMask())
					{
						DbgPrintf(4, _T("Setting correct netmask for subnet %s [%d] from node %s [%d]"),
									 pSubnet->Name(), pSubnet->Id(), m_szName, m_dwId);
						pSubnet->SetCorrectMask(pInterface->IpAddr() & pInterface->IpNetMask(), pInterface->IpNetMask());
					}
				}
			}
			else if (!isSync)
			{
				// Create subnet
				pSubnet = new Subnet(pIfList->pInterfaces[i].dwIpAddr & pIfList->pInterfaces[i].dwIpNetMask,
				                     pIfList->pInterfaces[i].dwIpNetMask, m_dwZoneGUID, FALSE);
				NetObjInsert(pSubnet, TRUE);
				g_pEntireNet->AddSubnet(pSubnet);
				pSubnet->AddNode(this);
				DbgPrintf(4, _T("Node::CheckSubnetBinding(): Creating new subnet %s [%d] for node %s [%d]"),
				          pSubnet->Name(), pSubnet->Id(), m_szName, m_dwId);
			}

			// Check if subnet mask is correct on interface
			if ((pSubnet != NULL) && (pSubnet->IpNetMask() != pIfList->pInterfaces[i].dwIpNetMask))
			{
				PostEvent(EVENT_INCORRECT_NETMASK, m_dwId, "idsaa", pInterface->Id(),
							 pInterface->IfIndex(), pInterface->Name(),
							 pInterface->IpNetMask(), pSubnet->IpNetMask());
			}
		}
	}
	
	// Check if we have incorrect subnets as parents
	LockParentList(FALSE);
	LockChildList(FALSE);
	ppUnlinkList = (NetObj **)malloc(sizeof(NetObj *) * m_dwParentCount);
	for(i = 0, count = 0; i < (int)m_dwParentCount; i++)
	{
		if (m_pParentList[i]->Type() == OBJECT_SUBNET)
		{
			pSubnet = (Subnet *)m_pParentList[i];
			for(j = 0; j < (int)m_dwChildCount; j++)
			{
				if (m_pChildList[j]->Type() == OBJECT_INTERFACE)
				{
					if (pSubnet->IpAddr() == (m_pChildList[j]->IpAddr() & pSubnet->IpNetMask()))
					{
						if (pCluster != NULL)
						{
							if (pCluster->isSyncAddr(m_pChildList[j]->IpAddr()))
							{
								j = (int)m_dwChildCount;	// Cause to unbind from this subnet
							}
						}
						break;
					}
				}
			}
			if (j == (int)m_dwChildCount)
			{
				DbgPrintf(4, _T("Node::CheckSubnetBinding(): Subnet %s [%d] is incorrect for node %s [%d]"),
							 pSubnet->Name(), pSubnet->Id(), m_szName, m_dwId);
				ppUnlinkList[count++] = pSubnet;	
			}
		}
	}
	UnlockChildList();
	UnlockParentList();
	
	// Unlink for incorrect subnet objects
	for(i = 0; i < count; i++)
	{
		ppUnlinkList[i]->DeleteChild(this);
		DeleteParent(ppUnlinkList[i]);
	}
	safe_free(ppUnlinkList);
}


//
// Update interface names
//

void Node::updateInterfaceNames(ClientSession *pSession, DWORD dwRqId)
{
	INTERFACE_LIST *pIfList;
	DWORD i;
	int j;
	
   pollerLock();
   m_pPollRequestor = pSession;
   SendPollerMsg(dwRqId, _T("Starting interface names poll for node %s\r\n"), m_szName);
   DbgPrintf(4, "Starting interface names poll for node %s (ID: %d)", m_szName, m_dwId);

   // Retrieve interface list
   pIfList = getInterfaceList();
   if (pIfList != NULL)
   {
      // Check names of existing interfaces
      for(j = 0; j < pIfList->iNumEntries; j++)
      {
         LockChildList(FALSE);
         for(i = 0; i < m_dwChildCount; i++)
         {
            if (m_pChildList[i]->Type() == OBJECT_INTERFACE)
            {
               Interface *pInterface = (Interface *)m_pChildList[i];

               if (pIfList->pInterfaces[j].dwIndex == pInterface->IfIndex())
               {
						SendPollerMsg(dwRqId, _T("   Checking interface %d (%s)\r\n"), pInterface->IfIndex(), pInterface->Name());
                  if (strcmp(pIfList->pInterfaces[j].szName, pInterface->Name()))
                  {
                     pInterface->SetName(pIfList->pInterfaces[j].szName);
							SendPollerMsg(dwRqId, POLLER_WARNING _T("   Name of interface %d changed to %s\r\n"), pInterface->IfIndex(), pIfList->pInterfaces[j].szName);
                  }
                  break;
               }
            }
         }
         UnlockChildList();
      }

      DestroyInterfaceList(pIfList);
   }
   else     /* pIfList == NULL */
   {
      SendPollerMsg(dwRqId, POLLER_ERROR _T("   Unable to get interface list from node\r\n"));
   }

   // Finish poll
	SendPollerMsg(dwRqId, _T("Finished interface names poll for node %s\r\n"), m_szName);
   pollerUnlock();
   DbgPrintf(4, "Finished interface names poll for node %s (ID: %d)", m_szName, m_dwId);
}


//
// Process new DCI value
//

void Node::processNewDciValue(DCItem *item, time_t currTime, const TCHAR *value)
{
	lockDciAccess();
	item->processNewValue(currTime, value);
	unlockDciAccess();
}


//
// Get list of parent objects for NXSL script
//

NXSL_Array *Node::getParentsForNXSL()
{
	NXSL_Array *parents = new NXSL_Array;
	int index = 0;

	LockParentList(FALSE);
	for(DWORD i = 0; i < m_dwParentCount; i++)
	{
		if (((m_pParentList[i]->Type() == OBJECT_CONTAINER) ||
		     (m_pParentList[i]->Type() == OBJECT_CLUSTER) ||
			  (m_pParentList[i]->Type() == OBJECT_SUBNET) ||
			  (m_pParentList[i]->Type() == OBJECT_SERVICEROOT)) &&
		    m_pParentList[i]->IsTrustedNode(m_dwId))
		{
			parents->set(index++, new NXSL_Value(new NXSL_Object(&g_nxslNetObjClass, m_pParentList[i])));
		}
	}
	UnlockParentList();

	return parents;
}
