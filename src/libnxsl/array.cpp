/* 
** NetXMS - Network Management System
** NetXMS Scripting Language Interpreter
** Copyright (C) 2003-2018 Victor Kirhenshtein
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU Lesser General Public License as published by
** the Free Software Foundation; either version 3 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU Lesser General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** File: array.cpp
**
**/

#include "libnxsl.h"

/**
 * Create empty array
 */
NXSL_Array::NXSL_Array(NXSL_ValueManager *vm) : NXSL_HandleCountObject(vm)
{
	m_size = 0;
	m_allocated = 0;
	m_data = NULL;
}

/**
 *  Create copy of given array
 */
NXSL_Array::NXSL_Array(const NXSL_Array *src) : NXSL_HandleCountObject(src->m_vm)
{
	m_size = src->m_size;
	m_allocated = src->m_size;
	if (m_size > 0)
	{
		m_data = (NXSL_ArrayElement *)malloc(sizeof(NXSL_ArrayElement) * m_size);
		for(int i = 0; i < m_size; i++)
		{
			m_data[i].index = src->m_data[i].index;
			m_data[i].value = m_vm->createValue(src->m_data[i].value);
		}
	}
	else
	{
		m_data = NULL;
	}
}

/**
 * Create array populated with values from string list
 */
NXSL_Array::NXSL_Array(NXSL_ValueManager *vm, const StringList *values) : NXSL_HandleCountObject(vm)
{
	m_size = values->size();
	m_allocated = m_size;
	if (m_size > 0)
	{
		m_data = (NXSL_ArrayElement *)malloc(sizeof(NXSL_ArrayElement) * m_size);
		for(int i = 0; i < m_size; i++)
		{
			m_data[i].index = i;
			m_data[i].value = m_vm->createValue(values->get(i));
		}
	}
	else
	{
		m_data = NULL;
	}
}

/**
 * Destructor
 */
NXSL_Array::~NXSL_Array()
{
	int i;

	for(i = 0; i < m_size; i++)
		m_vm->destroyValue(m_data[i].value);
	free(m_data);
}

/**
 * Compare two indexes
 */
static int CompareElements(const void *p1, const void *p2)
{
	return COMPARE_NUMBERS(((NXSL_ArrayElement *)p1)->index, ((NXSL_ArrayElement *)p2)->index);
}

/**
 * Get element by index
 */
NXSL_Value *NXSL_Array::get(int index) const
{
	NXSL_ArrayElement *element, key;

	key.index = index;
	element = (NXSL_ArrayElement *)bsearch(&key, m_data, m_size, sizeof(NXSL_ArrayElement), CompareElements);
	return (element != NULL) ? element->value : NULL;
}

/**
 * Get element by internal position (used by iterator)
 */
NXSL_Value *NXSL_Array::getByPosition(int position) const
{
	if ((position < 0) || (position >= m_size))
		return NULL;
	return m_data[position].value;
}

/**
 * Get all elements as string list
 */
StringList *NXSL_Array::toStringList() const
{
   StringList *list = new StringList();
   for(int i = 0; i < m_size; i++)
   {
      list->add(m_data[i].value->getValueAsCString());
   }
   return list;
}

/**
 * Set element
 */
void NXSL_Array::set(int index, NXSL_Value *value)
{
	NXSL_ArrayElement *element, key;

	key.index = index;
	element = (NXSL_ArrayElement *)bsearch(&key, m_data, m_size, sizeof(NXSL_ArrayElement), CompareElements);
	if (element != NULL)
	{
		m_vm->destroyValue(element->value);
		element->value = value;
	}
	else
	{
		if (m_size == m_allocated)
		{
			m_allocated += 64;
			m_data = (NXSL_ArrayElement *)realloc(m_data, sizeof(NXSL_ArrayElement) * m_allocated);
		}
		m_data[m_size].index = index;
		m_data[m_size].value = value;
		m_size++;
		if ((m_size > 1) && (m_data[m_size - 2].index > index))
		{
		   // do not sort if adding at the end
		   qsort(m_data, m_size, sizeof(NXSL_ArrayElement), CompareElements);
		}
	}
}

/**
 * Insert value into array
 */
void NXSL_Array::insert(int index, NXSL_Value *value)
{
   int i = m_size - 1;
   while((i >= 0) && (m_data[i].index >= index))
   {
      m_data[i].index++;
      i--;
   }

   if (m_size == m_allocated)
   {
      m_allocated += 32;
      m_data = (NXSL_ArrayElement *)realloc(m_data, sizeof(NXSL_ArrayElement) * m_allocated);
   }

   i++;
   m_size++;
   memmove(&m_data[i + 1], &m_data[i], sizeof(NXSL_ArrayElement) * (m_size - i));
   m_data[i].index = index;
   m_data[i].value = value;
}

/**
 * Remove value from array
 */
void NXSL_Array::remove(int index)
{
   if (m_size == 0)
      return;

   int i = m_size - 1;
   while((i >= 0) && (m_data[i].index > index))
   {
      m_data[i].index--;
      i--;
   }
   if ((i >= 0) && (m_data[i].index == index))
   {
      m_vm->destroyValue(m_data[i].value);
      m_size--;
      memmove(&m_data[i], &m_data[i + 1], sizeof(NXSL_ArrayElement) * (m_size - i));
   }
}

/**
 * Call method on array
 */
int NXSL_Array::callMethod(const NXSL_Identifier& name, int argc, NXSL_Value **argv, NXSL_Value **result, NXSL_VM *vm)
{
   if (!strcmp(name.value, "append") || !strcmp(name.value, "push"))
   {
      if (argc != 1)
         return NXSL_ERR_INVALID_ARGUMENT_COUNT;
      append(m_vm->createValue(argv[0]));
      *result = m_vm->createValue(getMaxIndex());
   }
   else if (!strcmp(name.value, "insert"))
   {
      if (argc != 2)
         return NXSL_ERR_INVALID_ARGUMENT_COUNT;

      if (!argv[0]->isInteger())
         return NXSL_ERR_NOT_INTEGER;

      insert(argv[0]->getValueAsInt32(), m_vm->createValue(argv[1]));
      *result = m_vm->createValue();
   }
   else if (!strcmp(name.value, "pop"))
   {
      if (argc != 0)
         return NXSL_ERR_INVALID_ARGUMENT_COUNT;

      *result = (m_size > 0) ? m_data[--m_size].value : m_vm->createValue();
   }
   else if (!strcmp(name.value, "remove"))
   {
      if (argc != 1)
         return NXSL_ERR_INVALID_ARGUMENT_COUNT;

      if (!argv[0]->isInteger())
         return NXSL_ERR_NOT_INTEGER;

      remove(argv[0]->getValueAsInt32());
      *result = m_vm->createValue();
   }
   else
   {
      return NXSL_ERR_NO_SUCH_METHOD;
   }
   return 0;
}
