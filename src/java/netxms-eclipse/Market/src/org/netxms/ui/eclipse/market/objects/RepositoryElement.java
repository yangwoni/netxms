/**
 * NetXMS - open source network management system
 * Copyright (C) 2003-2016 Victor Kirhenshtein
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
package org.netxms.ui.eclipse.market.objects;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.util.UUID;
import org.json.JSONArray;
import org.json.JSONObject;
import org.netxms.ui.eclipse.market.objects.helpers.RepositoryObjectInstance;

/**
 * Common interface for repository elements (templates, events, etc.)
 */
public abstract class RepositoryElement implements MarketObject, RepositoryObject
{
   private UUID guid;
   private String name;
   private MarketObject parent;
   private List<RepositoryObjectInstance> instances;
   private boolean marked;
   
   /**
    * Create element from JSON object
    * 
    * @param guid
    * @param json
    */
   public RepositoryElement(UUID guid, JSONObject json)
   {
      this.guid = guid;
      name = json.getString("name");
      parent = null;
      marked = false;
      
      JSONArray a = json.getJSONArray("instances");
      if (a != null)
      {
         instances = new ArrayList<RepositoryObjectInstance>(a.length());
         for(int i = 0; i < a.length(); i++)
         {
            instances.add(new RepositoryObjectInstance(a.getJSONObject(i)));
         }
         Collections.sort(instances, new Comparator<RepositoryObjectInstance>() {
            @Override
            public int compare(RepositoryObjectInstance o1, RepositoryObjectInstance o2)
            {
               return o2.getVersion() - o1.getVersion();
            }
         });
      }
      else
      {
         instances = new ArrayList<RepositoryObjectInstance>(0);
      }
   }   
   
   /* (non-Javadoc)
    * @see org.netxms.ui.eclipse.market.objects.MarketObject#getName()
    */
   @Override
   public String getName()
   {
      return marked ? name + " *" : name;
   }

   /* (non-Javadoc)
    * @see org.netxms.ui.eclipse.market.objects.MarketObject#getGuid()
    */
   @Override
   public UUID getGuid()
   {
      return guid;
   }

   /* (non-Javadoc)
    * @see org.netxms.ui.eclipse.market.objects.MarketObject#getParent()
    */
   @Override
   public MarketObject getParent()
   {
      return parent;
   }

   /* (non-Javadoc)
    * @see org.netxms.ui.eclipse.market.objects.MarketObject#getChildren()
    */
   @Override
   public MarketObject[] getChildren()
   {
      return new MarketObject[0];
   }

   /* (non-Javadoc)
    * @see org.netxms.ui.eclipse.market.objects.MarketObject#hasChildren()
    */
   @Override
   public boolean hasChildren()
   {
      return false;
   }

   /* (non-Javadoc)
    * @see org.netxms.ui.eclipse.market.objects.RepositoryObject#setParent(org.netxms.ui.eclipse.market.objects.Category)
    */
   public void setParent(Category parent)
   {
      this.parent = parent;
   }

   /* (non-Javadoc)
    * @see org.netxms.ui.eclipse.market.objects.RepositoryObject#isMarked()
    */
   public boolean isMarked()
   {
      return marked;
   }

   /* (non-Javadoc)
    * @see org.netxms.ui.eclipse.market.objects.RepositoryObject#setMarked(boolean)
    */
   public void setMarked(boolean marked)
   {
      this.marked = marked;
   }

   /* (non-Javadoc)
    * @see org.netxms.ui.eclipse.market.objects.RepositoryObject#getInstances()
    */
   public List<RepositoryObjectInstance> getInstances()
   {
      return instances;
   }

   /* (non-Javadoc)
    * @see org.netxms.ui.eclipse.market.objects.RepositoryObject#getActualInstance()
    */
   public RepositoryObjectInstance getActualInstance()
   {
      return instances.isEmpty() ? null : instances.get(0);
   }

   /* (non-Javadoc)
    * @see org.netxms.ui.eclipse.market.objects.RepositoryObject#getActualVersion()
    */
   public int getActualVersion()
   {
      return instances.isEmpty() ? 0 : instances.get(0).getVersion();
   }
}
