/* 
** Project X - Network Management System
** Copyright (C) 2003 Victor Kirhenshtein
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
** $module: status.cpp
**
**/

#include "nms_core.h"


//
// Status poll thread
//

void StatusPoller(void *arg)
{
   Node *pNode;
   DWORD dwWatchdogId;

   dwWatchdogId = WatchdogAddThread("Status Poller");

   while(!ShutdownInProgress())
   {
      if (SleepAndCheckForShutdown(5))
         break;      // Shutdown has arrived
      WatchdogNotify(dwWatchdogId);

      // Walk through nodes and do status poll
      for(DWORD i = 0; i < g_dwNodeAddrIndexSize; i++)
      {
         pNode = (Node *)g_pNodeIndexByAddr[i].pObject;
         if (pNode->ReadyForStatusPoll())
         {
            pNode->StatusPoll();
            WatchdogNotify(dwWatchdogId);
         }
      }
   }
}


//
// Configuration poll thread
//

void ConfigurationPoller(void *arg)
{
   Node *pNode;
   DWORD dwWatchdogId;

   dwWatchdogId = WatchdogAddThread("Configuration Poller");

   while(!ShutdownInProgress())
   {
      if (SleepAndCheckForShutdown(30))
         break;      // Shutdown has arrived
      WatchdogNotify(dwWatchdogId);

      // Walk through nodes and do configuration poll
      for(DWORD i = 0; i < g_dwNodeAddrIndexSize; i++)
      {
         pNode = (Node *)g_pNodeIndexByAddr[i].pObject;
         if (pNode->ReadyForConfigurationPoll())
         {
            pNode->ConfigurationPoll();
            WatchdogNotify(dwWatchdogId);
         }
      }
   }
}
