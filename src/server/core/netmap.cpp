/* 
** NetXMS - Network Management System
** Copyright (C) 2003-2012 Victor Kirhenshtein
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
** File: netmap.cpp
**
**/

#include "nxcore.h"


//
// Redefined status calculation for network maps group
//

void NetworkMapGroup::calculateCompoundStatus(BOOL bForcedRecalc)
{
   m_iStatus = STATUS_NORMAL;
}


//
// Network map object default constructor
//

NetworkMap::NetworkMap() : NetObj()
{
	m_mapType = NETMAP_USER_DEFINED;
	m_seedObject = 0;
	m_flags = MF_SHOW_STATUS_ICON;
	m_layout = MAP_LAYOUT_MANUAL;
	uuid_clear(m_background);
	m_iStatus = STATUS_NORMAL;
	m_backgroundLatitude = 0;
	m_backgroundLongitude = 0;
	m_backgroundZoom = 1;
	m_backgroundColor = 0xFFFFFF;
	m_defaultLinkColor = -1;
	m_elements = new ObjectArray<NetworkMapElement>(0, 32, true);
	m_links = new ObjectArray<NetworkMapLink>(0, 32, true);
}


//
// Create network map object from user session
//

NetworkMap::NetworkMap(int type, DWORD seed) : NetObj()
{
	m_mapType = type;
	m_seedObject = seed;
	m_flags = MF_SHOW_STATUS_ICON;
	m_layout = MAP_LAYOUT_RADIAL;
	uuid_clear(m_background);
	m_iStatus = STATUS_NORMAL;
	m_backgroundLatitude = 0;
	m_backgroundLongitude = 0;
	m_backgroundZoom = 1;
	m_backgroundColor = 0xFFFFFF;
	m_defaultLinkColor = -1;
	m_elements = new ObjectArray<NetworkMapElement>(0, 32, true);
	m_links = new ObjectArray<NetworkMapLink>(0, 32, true);
	m_bIsHidden = TRUE;
}


//
// Network map object destructor
//

NetworkMap::~NetworkMap()
{
	delete m_elements;
	delete m_links;
}

/**
 * Redefined status calculation for network maps
 */
void NetworkMap::calculateCompoundStatus(BOOL bForcedRecalc)
{
   m_iStatus = STATUS_NORMAL;
}

/**
 * Save to database
 */
BOOL NetworkMap::SaveToDB(DB_HANDLE hdb)
{
	TCHAR query[1024], temp[64];

	LockData();

	if (!saveCommonProperties(hdb))
		goto fail;

	DB_STATEMENT hStmt;
	if (IsDatabaseRecordExist(hdb, _T("network_maps"), _T("id"), m_dwId))
	{
		hStmt = DBPrepare(hdb, _T("UPDATE network_maps SET map_type=?,layout=?,seed=?,background=?,bg_latitude=?,bg_longitude=?,bg_zoom=?,flags=?,link_color=?,link_routing=?,bg_color=? WHERE id=?"));
	}
	else
	{
		hStmt = DBPrepare(hdb, _T("INSERT INTO network_maps (map_type,layout,seed,background,bg_latitude,bg_longitude,bg_zoom,flags,link_color,link_routing,bg_color,id) VALUES (?,?,?,?,?,?,?,?,?,?,?,?)"));
	}
	if (hStmt == NULL)
		goto fail;

	DBBind(hStmt, 1, DB_SQLTYPE_INTEGER, (LONG)m_mapType);
	DBBind(hStmt, 2, DB_SQLTYPE_INTEGER, (LONG)m_layout);
	DBBind(hStmt, 3, DB_SQLTYPE_INTEGER, m_seedObject);
	DBBind(hStmt, 4, DB_SQLTYPE_VARCHAR, uuid_to_string(m_background, temp), DB_BIND_STATIC);
	DBBind(hStmt, 5, DB_SQLTYPE_DOUBLE, m_backgroundLatitude);
	DBBind(hStmt, 6, DB_SQLTYPE_DOUBLE, m_backgroundLongitude);
	DBBind(hStmt, 7, DB_SQLTYPE_INTEGER, (LONG)m_backgroundZoom);
	DBBind(hStmt, 8, DB_SQLTYPE_INTEGER, m_flags);
	DBBind(hStmt, 9, DB_SQLTYPE_INTEGER, (LONG)m_defaultLinkColor);
	DBBind(hStmt, 10, DB_SQLTYPE_INTEGER, (LONG)m_defaultLinkRouting);
	DBBind(hStmt, 11, DB_SQLTYPE_INTEGER, (LONG)m_backgroundColor);
	DBBind(hStmt, 12, DB_SQLTYPE_INTEGER, m_dwId);

	if (!DBExecute(hStmt))
	{
		DBFreeStatement(hStmt);
		goto fail;
	}
	DBFreeStatement(hStmt);

	if (!saveACLToDB(hdb))
		goto fail;

   // Save elements
   _sntprintf(query, 256, _T("DELETE FROM network_map_elements WHERE map_id=%d"), m_dwId);
   if (!DBQuery(hdb, query))
		goto fail;
	for(int i = 0; i < m_elements->size(); i++)
   {
		NetworkMapElement *e = m_elements->get(i);
		Config *config = new Config();
		config->setTopLevelTag(_T("element"));
		e->updateConfig(config);
		String data = DBPrepareString(hdb, config->createXml());
		delete config;
		int len = data.getSize() + 256;
		TCHAR *eq = (TCHAR *)malloc(len * sizeof(TCHAR));
      _sntprintf(eq, len, _T("INSERT INTO network_map_elements (map_id,element_id,element_type,element_data) VALUES (%d,%d,%d,%s)"),
		           m_dwId, e->getId(), e->getType(), (const TCHAR *)data);
      DBQuery(hdb, eq);
		free(eq);
   }

	// Save links
   _sntprintf(query, 256, _T("DELETE FROM network_map_links WHERE map_id=%d"), m_dwId);
   if (!DBQuery(hdb, query))
		goto fail;
	hStmt = DBPrepare(hdb, _T("INSERT INTO network_map_links (map_id,element1,element2,link_type,link_name,connector_name1,connector_name2,color,status_object,routing,bend_points) VALUES (?,?,?,?,?,?,?,?,?,?,?)"));
	if (hStmt == NULL)
		goto fail;
	for(int i = 0; i < m_links->size(); i++)
   {
		NetworkMapLink *l = m_links->get(i);
		DBBind(hStmt, 1, DB_SQLTYPE_INTEGER, m_dwId);
		DBBind(hStmt, 2, DB_SQLTYPE_INTEGER, l->getElement1());
		DBBind(hStmt, 3, DB_SQLTYPE_INTEGER, l->getElement2());
		DBBind(hStmt, 4, DB_SQLTYPE_INTEGER, (LONG)l->getType());
		DBBind(hStmt, 5, DB_SQLTYPE_VARCHAR, l->getName(), DB_BIND_STATIC);
		DBBind(hStmt, 6, DB_SQLTYPE_VARCHAR, l->getConnector1Name(), DB_BIND_STATIC);
		DBBind(hStmt, 7, DB_SQLTYPE_VARCHAR, l->getConnector2Name(), DB_BIND_STATIC);
		DBBind(hStmt, 8, DB_SQLTYPE_INTEGER, l->getColor());
		DBBind(hStmt, 9, DB_SQLTYPE_INTEGER, l->getStatusObject());
		DBBind(hStmt, 10, DB_SQLTYPE_INTEGER, (LONG)l->getRouting());
		DBBind(hStmt, 11, DB_SQLTYPE_VARCHAR, l->getBendPoints(query), DB_BIND_STATIC);
		DBExecute(hStmt);
   }
	DBFreeStatement(hStmt);

	UnlockData();
	return TRUE;

fail:
	UnlockData();
	return FALSE;
}

/**
 * Delete from database
 */
BOOL NetworkMap::DeleteFromDB()
{
	TCHAR query[256];

	_sntprintf(query, 256, _T("DELETE FROM network_maps WHERE id=%d"), m_dwId);
	QueueSQLRequest(query);
	_sntprintf(query, 256, _T("DELETE FROM network_map_elements WHERE map_id=%d"), m_dwId);
	QueueSQLRequest(query);
	_sntprintf(query, 256, _T("DELETE FROM network_map_links WHERE map_id=%d"), m_dwId);
	QueueSQLRequest(query);
	return TRUE;
}

/**
 * Load from database
 */
BOOL NetworkMap::CreateFromDB(DWORD dwId)
{
	m_dwId = dwId;

	if (!loadCommonProperties())
   {
      DbgPrintf(2, _T("Cannot load common properties for network map object %d"), dwId);
      return FALSE;
   }

   if (!m_bIsDeleted)
   {
		TCHAR query[256];

	   loadACLFromDB();

		_sntprintf(query, 256, _T("SELECT map_type,layout,seed,background,bg_latitude,bg_longitude,bg_zoom,flags,link_color,link_routing,bg_color FROM network_maps WHERE id=%d"), dwId);
		DB_RESULT hResult = DBSelect(g_hCoreDB, query);
		if (hResult == NULL)
			return FALSE;

		m_mapType = DBGetFieldLong(hResult, 0, 0);
		m_layout = DBGetFieldLong(hResult, 0, 1);
		m_seedObject = DBGetFieldULong(hResult, 0, 2);
		DBGetFieldGUID(hResult, 0, 3, m_background);
		m_backgroundLatitude = DBGetFieldDouble(hResult, 0, 4);
		m_backgroundLongitude = DBGetFieldDouble(hResult, 0, 5);
		m_backgroundZoom = (int)DBGetFieldLong(hResult, 0, 6);
		m_flags = DBGetFieldULong(hResult, 0, 7);
		m_defaultLinkColor = DBGetFieldLong(hResult, 0, 8);
		m_defaultLinkRouting = DBGetFieldLong(hResult, 0, 9);
		m_backgroundColor = DBGetFieldLong(hResult, 0, 10);
		DBFreeResult(hResult);

	   // Load elements
      _sntprintf(query, 256, _T("SELECT element_id,element_type,element_data FROM network_map_elements WHERE map_id=%d"), m_dwId);
      hResult = DBSelect(g_hCoreDB, query);
      if (hResult != NULL)
      {
         int count = DBGetNumRows(hResult);
			for(int i = 0; i < count; i++)
			{
				NetworkMapElement *e;
				DWORD id = DBGetFieldULong(hResult, i, 0);
				Config *config = new Config();
				TCHAR *data = DBGetField(hResult, i, 2, NULL, 0);
				if (data != NULL)
				{
#ifdef UNICODE
					char *utf8data = UTF8StringFromWideString(data);
					config->loadXmlConfigFromMemory(utf8data, (int)strlen(utf8data), _T("<database>"), "element");
					free(utf8data);
#else
					config->loadXmlConfigFromMemory(data, (int)strlen(data), _T("<database>"), "element");
#endif
					free(data);
					switch(DBGetFieldLong(hResult, i, 1))
					{
						case MAP_ELEMENT_OBJECT:
							e = new NetworkMapObject(id, config);
							break;
						case MAP_ELEMENT_DECORATION:
							e = new NetworkMapDecoration(id, config);
							break;
						default:		// Unknown type, create generic element
							e = new NetworkMapElement(id, config);
							break;
					}
				}
				else
				{
					e = new NetworkMapElement(id);
				}
				delete config;
				m_elements->add(e);
			}
         DBFreeResult(hResult);
      }

		// Load links
      _sntprintf(query, 256, _T("SELECT element1,element2,link_type,link_name,connector_name1,connector_name2,color,status_object,routing,bend_points FROM network_map_links WHERE map_id=%d"), m_dwId);
      hResult = DBSelect(g_hCoreDB, query);
      if (hResult != NULL)
      {
         int count = DBGetNumRows(hResult);
			for(int i = 0; i < count; i++)
			{
				TCHAR buffer[1024];

				NetworkMapLink *l = new NetworkMapLink(DBGetFieldLong(hResult, i, 0), DBGetFieldLong(hResult, i, 1), DBGetFieldLong(hResult, i, 2));
				l->setName(DBGetField(hResult, i, 3, buffer, 256));
				l->setConnector1Name(DBGetField(hResult, i, 4, buffer, 256));
				l->setConnector2Name(DBGetField(hResult, i, 5, buffer, 256));
				l->setColor(DBGetFieldULong(hResult, i, 6));
				l->setStatusObject(DBGetFieldULong(hResult, i, 7));
				l->setRouting(DBGetFieldULong(hResult, i, 8));
				l->parseBendPoints(DBGetField(hResult, i, 9, buffer, 1024));
				m_links->add(l);
			}
         DBFreeResult(hResult);
      }
	}

	m_iStatus = STATUS_NORMAL;

	return TRUE;
}

/**
 * Fill NXCP message with object's data
 */
void NetworkMap::CreateMessage(CSCPMessage *msg)
{
	NetObj::CreateMessage(msg);

	msg->SetVariable(VID_MAP_TYPE, (WORD)m_mapType);
	msg->SetVariable(VID_LAYOUT, (WORD)m_layout);
	msg->SetVariable(VID_FLAGS, m_flags);
	msg->SetVariable(VID_SEED_OBJECT, m_seedObject);
	msg->SetVariable(VID_BACKGROUND, m_background, UUID_LENGTH);
	msg->SetVariable(VID_BACKGROUND_LATITUDE, m_backgroundLatitude);
	msg->SetVariable(VID_BACKGROUND_LONGITUDE, m_backgroundLongitude);
	msg->SetVariable(VID_BACKGROUND_ZOOM, (WORD)m_backgroundZoom);
	msg->SetVariable(VID_LINK_COLOR, (DWORD)m_defaultLinkColor);
	msg->SetVariable(VID_LINK_ROUTING, (WORD)m_defaultLinkRouting);
	msg->SetVariable(VID_BACKGROUND_COLOR, (DWORD)m_backgroundColor);

	msg->SetVariable(VID_NUM_ELEMENTS, (DWORD)m_elements->size());
	DWORD varId = VID_ELEMENT_LIST_BASE;
	for(int i = 0; i < m_elements->size(); i++)
	{
		m_elements->get(i)->fillMessage(msg, varId);
		varId += 100;
	}

	msg->SetVariable(VID_NUM_LINKS, (DWORD)m_links->size());
	varId = VID_LINK_LIST_BASE;
	for(int i = 0; i < m_links->size(); i++)
	{
		m_links->get(i)->fillMessage(msg, varId);
		varId += 10;
	}
}

/**
 * Update network map object from NXCP message
 */
DWORD NetworkMap::ModifyFromMessage(CSCPMessage *request, BOOL bAlreadyLocked)
{
	if (!bAlreadyLocked)
		LockData();

	if (request->IsVariableExist(VID_MAP_TYPE))
		m_mapType = (int)request->GetVariableShort(VID_MAP_TYPE);

	if (request->IsVariableExist(VID_LAYOUT))
		m_layout = (int)request->GetVariableShort(VID_LAYOUT);

	if (request->IsVariableExist(VID_FLAGS))
		m_flags = request->GetVariableLong(VID_FLAGS);

	if (request->IsVariableExist(VID_SEED_OBJECT))
		m_seedObject = request->GetVariableLong(VID_SEED_OBJECT);

	if (request->IsVariableExist(VID_LINK_COLOR))
		m_defaultLinkColor = (int)request->GetVariableLong(VID_LINK_COLOR);

	if (request->IsVariableExist(VID_LINK_ROUTING))
		m_defaultLinkRouting = (int)request->GetVariableShort(VID_LINK_ROUTING);

	if (request->IsVariableExist(VID_BACKGROUND_COLOR))
		m_backgroundColor = (int)request->GetVariableLong(VID_BACKGROUND_COLOR);

	if (request->IsVariableExist(VID_BACKGROUND))
	{
		request->GetVariableBinary(VID_BACKGROUND, m_background, UUID_LENGTH);
		m_backgroundLatitude = request->GetVariableDouble(VID_BACKGROUND_LATITUDE);
		m_backgroundLongitude = request->GetVariableDouble(VID_BACKGROUND_LONGITUDE);
		m_backgroundZoom = (int)request->GetVariableShort(VID_BACKGROUND_ZOOM);
	}

	if (request->IsVariableExist(VID_NUM_ELEMENTS))
	{
		m_elements->clear();

		int numElements = (int)request->GetVariableLong(VID_NUM_ELEMENTS);
		if (numElements > 0)
		{
			DWORD varId = VID_ELEMENT_LIST_BASE;
			for(int i = 0; i < numElements; i++)
			{
				NetworkMapElement *e;
				int type = (int)request->GetVariableShort(varId + 1);
				switch(type)
				{
					case MAP_ELEMENT_OBJECT:
						e = new NetworkMapObject(request, varId);
						break;
					case MAP_ELEMENT_DECORATION:
						e = new NetworkMapDecoration(request, varId);
						break;
					default:		// Unknown type, create generic element
						e = new NetworkMapElement(request, varId);
						break;
				}
				m_elements->add(e);
				varId += 100;
			}
		}

		m_links->clear();
		int numLinks = request->GetVariableLong(VID_NUM_LINKS);
		if (numLinks > 0)
		{
			DWORD varId = VID_LINK_LIST_BASE;
			for(int i = 0; i < numLinks; i++)
			{
				m_links->add(new NetworkMapLink(request, varId));
				varId += 10;
			}
		}
	}

	return NetObj::ModifyFromMessage(request, TRUE);
}

/**
 * Update map content for seeded map types
 */
void NetworkMap::updateContent()
{
	Node *seed;
	switch(m_mapType)
	{
		case MAP_TYPE_LAYER2_TOPOLOGY:
			seed = (Node *)FindObjectById(m_seedObject, OBJECT_NODE);
			if (seed != NULL)
			{
				DWORD status;
				nxmap_ObjList *objects = seed->BuildL2Topology(&status);
				if (objects != NULL)
				{
					updateObjects(objects);
					delete objects;
				}
				else
				{
					DbgPrintf(3, _T("NetworkMap::updateContent(%s [%d]): call to buildL2Topology on object %d failed"), m_szName, m_dwId, m_seedObject);
				}
			}
			else
			{
				DbgPrintf(3, _T("NetworkMap::updateContent(%s [%d]): seed object %d cannot be found"), m_szName, m_dwId, m_seedObject);
			}
			break;
		default:
			break;
	}
}

/**
 * Update objects from given list
 */
void NetworkMap::updateObjects(nxmap_ObjList *objects)
{
	LockData();

	// remove non-existing links
	for(int i = 0; i < m_links->size(); i++)
	{
	}

	// remove non-existing objects
	for(int i = 0; i < m_elements->size(); i++)
	{
		for(int j = 0; j < objects->GetNumObjects(); j++)
		{
		}
	}

	for(int i = 0; i < objects->GetNumObjects(); i++)
	{
	}

	UnlockData();
}
