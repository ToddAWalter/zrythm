/*
 * Copyright (C) 2018-2019 Alexandros Theodotou <alex@zrythm.org>
 *
 * This file is part of Zrythm
 *
 * Zrythm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Zrythm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Zrythm.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef __UTILS_ARRAYS_H__
#define __UTILS_ARRAYS_H__

#define array_index_of(array, size, element) \
  _array_index_of ((void **) array, \
                   size, \
                   (void *) element)

/**
 * Appends element to the end of array array and increases the size.
 */
#define array_append(array, size, element) \
  array[size++] = element;

/**
 * Inserts element in array at pos and increases the size.
 */
#define array_insert(array,size,pos,element) \
  for (int ii = pos; ii < size; ii++) \
    { \
      array[ii + 1] = array[ii]; \
    } \
  array[pos] = element; \
  size++;

/**
 * Deletes element from array and rearranges other elements
 * accordingly.
 */
#define array_delete(array,size,element) \
  for (int ii = 0; ii < size; ii++) \
    { \
      if (array[ii] == element) \
        { \
          --size; \
          for (int jj = ii; jj < size; jj++) \
            { \
              array[jj] = array[jj + 1]; \
            } \
          break; \
        } \
    }

/**
 * Same as array_delete but sets the deleted element
 * index to pos.
 */
#define array_delete_return_pos(array,size,element,pos) \
  for (int ii = 0; ii < size; ii++) \
    { \
      if (array[ii] == element) \
        { \
          pos = ii; \
          --size; \
          for (int jj = ii; jj < size; jj++) \
            { \
              array[jj] = array[jj + 1]; \
            } \
          break; \
        } \
    }

/**
 * Deletes from two arrays concurrently.
 *
 * Assumes that the element should be deleted at the
 * same index in both arrays.
 */
#define array_double_delete( \
  array1,array2,size,element1,element2) \
  for (int ii = 0; ii < size; ii++) \
    { \
      if (array1[ii] == element1) \
        { \
          --size; \
          for (int jj = ii; jj < size; jj++) \
            { \
              array1[jj] = array1[jj + 1]; \
              array2[jj] = array2[jj + 1]; \
            } \
          break; \
        } \
    }



/**
 * Macro so that no casting to void ** and void * is
 * necessary.
 */
#define array_contains(array,size,element) \
  _array_contains ((void **) array, \
                   size, \
                   (void *) element)

/**
 * Returns 1 if element exists in array, 0 if not.
 */
int
_array_contains (void ** array,
                int size,
                void * element);

/**
 * Returns the index ofthe element exists in array,
 * -1 if not.
 */
int
_array_index_of (void ** array,
                int size, void * element);

void
array_sort_alphabetically (char ** array,
                           int     size,
                           int     case_sensitive);

#endif /* __UTILS_ARRAYS_H__ */
