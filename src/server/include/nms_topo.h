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
** File: nms_topo.h
**
**/

#ifndef _nms_topo_h_
#define _nms_topo_h_

#include <netxms_maps.h>


//
// Hop information structure
//

struct HOP_INFO
{
   DWORD dwNextHop;     // Next hop address
   NetObj *pObject;     // Current hop object
   DWORD dwIfIndex;     // Interface index or VPN connector object ID
   BOOL bIsVPN;         // TRUE if next hop is behind VPN tunnel
};


//
// Network path trace
//

struct NETWORK_PATH_TRACE
{
   int iNumHops;
   HOP_INFO *pHopList;
};


//
// Switch forwarding database
//

struct FDB_ENTRY
{
	DWORD ifIndex;       // Interface index
	BYTE macAddr[6];     // MAC address
	DWORD nodeObject;    // ID of node object or 0 if not found
};

class ForwardingDatabase
{
private:
	int m_count;
	int m_allocated;
	FDB_ENTRY *m_fdb;	
	time_t m_timestamp;
	int m_refCount;

public:
	ForwardingDatabase();
	~ForwardingDatabase();

	void addEntry(FDB_ENTRY *entry);

	void incRefCount() { m_refCount++; }
	void decRefCount();

	time_t getTimeStamp() { return m_timestamp; }
	int getAge() { return (int)(time(NULL) - m_timestamp); }
};


//
// link layer neighbors
//

#define LL_PROTO_FDB    0		/* obtained from switch forwarding database */
#define LL_PROTO_CDP    1		/* Cisco Discovery Protocol */
#define LL_PROTO_LLDP   2		/* Link Layer Discovery Protocol */
#define LL_PROTO_NDP    3		/* Nortel Discovery Protocol */
#define LL_PROTO_EDP    4		/* Extreme Discovery Protocol */

struct LL_NEIGHBOR_INFO
{
	DWORD ifLocal;			// Local interface index
	DWORD ifRemote;		// Remote interface index
	DWORD objectId;		// ID of connected object
	bool isPtToPt;			// true if this is point-to-point link
	int protocol;			// Protocol used to obtain information
};

class LinkLayerNeighbors
{
private:
	int m_count;
	int m_allocated;
	LL_NEIGHBOR_INFO *m_connections;

	bool isDuplicate(LL_NEIGHBOR_INFO *info);

public:
	LinkLayerNeighbors();
	~LinkLayerNeighbors();

	void addConnection(LL_NEIGHBOR_INFO *info);
};


//
// Topology functions
//

NETWORK_PATH_TRACE *TraceRoute(Node *pSrc, Node *pDest);
void DestroyTraceData(NETWORK_PATH_TRACE *pTrace);
DWORD BuildL2Topology(nxmap_ObjList &topology, Node *pRoot, Node *pParent,
							 int nDepth, TCHAR *pszParentIfName);
ForwardingDatabase *GetSwitchForwardingDatabase(Node *node);


#endif   /* _nms_topo_h_ */
