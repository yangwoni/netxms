/**
 * Java-Bridge NetXMS subagent
 * Copyright (C) 2013 TEMPEST a.s.
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
package org.netxms.agent;

import org.netxms.agent.ParameterType;

/**
 * Single-values parameter
 */
public interface Parameter extends AgentContributionItem
{
   /**
    * Get parameter's type
    * 
    * @return parameter's type
    */
   public ParameterType getType();

   /**
    * Get value
    * 
    * @param param parameter name
    * @return value for requested parameter
    * @throws Exception can throw exception to indicate data collection error
    */
   public String getValue(final String param) throws Exception;
}
