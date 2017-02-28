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

import java.util.UUID;
import org.json.JSONObject;

/**
 * Template reference
 */
public class TemplateReference extends RepositoryElement
{
   /**
    * Create template reference from JSON object
    * 
    * @param guid
    * @param json
    */
   public TemplateReference(UUID guid, JSONObject json)
   {
      super(guid, json);
   }

   /* (non-Javadoc)
    * @see org.netxms.ui.eclipse.market.objects.MarketObject#add(org.netxms.ui.eclipse.market.objects.MarketObject)
    */
   @Override
   public void add(MarketObject object)
   {
   }

   /* (non-Javadoc)
    * @see org.netxms.ui.eclipse.market.objects.MarketObject#setParent(org.netxms.ui.eclipse.market.objects.MarketObject)
    */
   @Override
   public void setParent(MarketObject parent)
   {
   }   
}
