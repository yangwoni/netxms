/*
** NetXMS - Network Management System
** Copyright (C) 2003-2016 Victor Kirhenshtein
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
** File: chassis.cpp
**
**/

#include "nxcore.h"

/**
 * Default constructor
 */
Chassis::Chassis() : DataCollectionTarget()
{
   m_controllerId = 0;
   m_rackId = 0;
   m_rackPosition = 0;
   m_rackHeight = 1;
}

/**
 * Create new chassis object
 */
Chassis::Chassis(const TCHAR *name, UINT32 controllerId) : DataCollectionTarget(name)
{
   m_controllerId = controllerId;
   m_rackId = 0;
   m_rackPosition = 0;
   m_rackHeight = 1;
}

/**
 * Destructor
 */
Chassis::~Chassis()
{
}

/**
 * Called by client session handler to check if threshold summary should be shown for this object.
 */
bool Chassis::showThresholdSummary()
{
   return true;
}

/**
 * Update rack binding
 */
void Chassis::updateRackBinding()
{
   bool rackFound = false;
   ObjectArray<NetObj> deleteList(16, 16, false);

   lockParentList(true);
   for(int i = 0; i < m_parentList->size(); i++)
   {
      NetObj *object = m_parentList->get(i);
      if (object->getObjectClass() != OBJECT_RACK)
         continue;
      if (object->getId() == m_rackId)
      {
         rackFound = true;
         continue;
      }
      object->incRefCount();
      deleteList.add(object);
   }
   unlockParentList();

   for(int n = 0; n < deleteList.size(); n++)
   {
      NetObj *rack = deleteList.get(n);
      DbgPrintf(5, _T("Chassis::updateRackBinding(%s [%d]): delete incorrect rack binding %s [%d]"), m_name, m_id, rack->getName(), rack->getId());
      rack->deleteChild(this);
      deleteParent(rack);
      rack->decRefCount();
   }

   if (!rackFound && (m_rackId != 0))
   {
      Rack *rack = (Rack *)FindObjectById(m_rackId, OBJECT_RACK);
      if (rack != NULL)
      {
         DbgPrintf(5, _T("Chassis::updateRackBinding(%s [%d]): add rack binding %s [%d]"), m_name, m_id, rack->getName(), rack->getId());
         rack->addChild(this);
         addParent(rack);
      }
      else
      {
         DbgPrintf(5, _T("Chassis::updateRackBinding(%s [%d]): rack object [%d] not found"), m_name, m_id, m_rackId);
      }
   }
}

/**
 * Create NXCP message with object's data
 */
void Chassis::fillMessageInternal(NXCPMessage *msg)
{
   DataCollectionTarget::fillMessageInternal(msg);
   msg->setField(VID_CONTROLLER_ID, m_controllerId);
   msg->setField(VID_RACK_ID, m_rackId);
   msg->setField(VID_RACK_IMAGE, m_rackImage);
   msg->setField(VID_RACK_POSITION, m_rackPosition);
   msg->setField(VID_RACK_HEIGHT, m_rackHeight);
}

/**
 * Modify object from NXCP message
 */
UINT32 Chassis::modifyFromMessageInternal(NXCPMessage *request)
{
   if (request->isFieldExist(VID_CONTROLLER_ID))
      m_controllerId = request->getFieldAsUInt32(VID_CONTROLLER_ID);
   if (request->isFieldExist(VID_RACK_ID))
   {
      m_rackId = request->getFieldAsUInt32(VID_RACK_ID);
      updateRackBinding();
   }
   if (request->isFieldExist(VID_RACK_IMAGE))
      m_rackImage = request->getFieldAsGUID(VID_RACK_IMAGE);
   if (request->isFieldExist(VID_RACK_POSITION))
      m_rackPosition = request->getFieldAsInt16(VID_RACK_POSITION);
   if (request->isFieldExist(VID_RACK_HEIGHT))
      m_rackHeight = request->getFieldAsInt16(VID_RACK_HEIGHT);

   return DataCollectionTarget::modifyFromMessageInternal(request);
}

/**
 * Save to database
 */
BOOL Chassis::saveToDatabase(DB_HANDLE hdb)
{
   lockProperties();
   bool success = saveCommonProperties(hdb);
   if (success)
   {
      DB_STATEMENT hStmt;
      if (IsDatabaseRecordExist(hdb, _T("chassis"), _T("id"), m_id))
         hStmt = DBPrepare(hdb, _T("UPDATE chassis SET controller_id=?,rack_id=?,rack_image=?,rack_position=?,rack_height=? WHERE id=?"));
      else
         hStmt = DBPrepare(hdb, _T("INSERT INTO chassis (controller_id,rack_id,rack_image,rack_position,rack_height,id) VALUES (?,?,?,?,?,?)"));
      if (hStmt != NULL)
      {
         DBBind(hStmt, 1, DB_SQLTYPE_INTEGER, m_controllerId);
         DBBind(hStmt, 2, DB_SQLTYPE_INTEGER, m_rackId);
         DBBind(hStmt, 3, DB_SQLTYPE_VARCHAR, m_rackImage);
         DBBind(hStmt, 4, DB_SQLTYPE_INTEGER, m_rackPosition);
         DBBind(hStmt, 5, DB_SQLTYPE_INTEGER, m_rackHeight);
         DBBind(hStmt, 6, DB_SQLTYPE_INTEGER, m_id);
         success = DBExecute(hStmt);
         DBFreeStatement(hStmt);
      }
      else
      {
         success = false;
      }
   }
   unlockProperties();
   return success;
}

/**
 * Delete from database
 */
bool Chassis::deleteFromDatabase(DB_HANDLE hdb)
{
   bool success = DataCollectionTarget::deleteFromDatabase(hdb);
   if (success)
   {
      success = executeQueryOnObject(hdb, _T("DELETE FROM chassis WHERE id=?"));
   }
   return success;
}

/**
 * Load from database
 */
bool Chassis::loadFromDatabase(DB_HANDLE hdb, UINT32 id)
{
   m_id = id;
   if (!loadCommonProperties(hdb))
   {
      nxlog_debug(2, _T("Cannot load common properties for chassis object %d"), id);
      return false;
   }

   DB_STATEMENT hStmt = DBPrepare(hdb, _T("SELECT controller_id,rack_id,rack_image,rack_position,rack_height FROM chassis WHERE id=?"));
   if (hStmt == NULL)
      return false;

   DBBind(hStmt, 1, DB_SQLTYPE_INTEGER, id);
   DB_RESULT hResult = DBSelectPrepared(hStmt);
   if (hResult == NULL)
   {
      DBFreeStatement(hStmt);
      return false;
   }

   m_controllerId = DBGetFieldULong(hResult, 0, 0);
   m_rackId = DBGetFieldULong(hResult, 0, 1);
   m_rackImage = DBGetFieldGUID(hResult, 0, 2);
   m_rackPosition = DBGetFieldULong(hResult, 0, 3);
   m_rackHeight = DBGetFieldULong(hResult, 0, 4);

   DBFreeResult(hResult);
   DBFreeStatement(hStmt);

   updateRackBinding();
   return true;
}

/**
 * Unbind cluster from template
 */
void Chassis::unbindFromTemplate(UINT32 dwTemplateId, bool removeDCI)
{
   DataCollectionTarget::unbindFromTemplate(dwTemplateId, removeDCI);
   queueUpdate();
}

/**
 * Called when data collection configuration changed
 */
void Chassis::onDataCollectionChange()
{
   queueUpdate();
}

/**
 * Create NXSL object for this object
 */
NXSL_Value *Chassis::createNXSLObject()
{
   return new NXSL_Value(new NXSL_Object(&g_nxslChassisClass, this));
}