/*
 * Copyright (C) 2019-2021 Alexandros Theodotou <alex at zrythm dot org>
 *
 * This file is part of Zrythm
 *
 * Zrythm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version approved by
 * Alexandros Theodotou in a public statement of acceptance.
 *
 * Zrythm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Zrythm.  If not, see <https://www.gnu.org/licenses/>.
 */

/**
 * \file
 *
 * Stack implementation.
 */

#ifndef __UTILS_STACK_H__
#define __UTILS_STACK_H__

#include <stdlib.h>

#include "utils/yaml.h"

#include <gtk/gtk.h>

/**
 * @addtogroup utils
 *
 * @{
 */

#define STACK_PUSH(s, element) \
  stack_push (s, (void *) element)

/**
 * Stack implementation.
 *
 */
typedef struct Stack
{
  void **           elements;

  /**
   * Max stack size, or -1 for unlimited.
   *
   * If the stack has a fixed length, it will be
   * real-time safe. If it has unlimited length,
   * it will not be real-time safe.
   */
  int               max_length;

  /**
   * Index of the top of the stack.
   *
   * This is an index and not a count.
   * Eg., if there is 1 element, this will be 0.
   */
  volatile gint     top;
} Stack;

static const cyaml_schema_field_t
  stack_fields_schema[] =
{
  YAML_FIELD_INT (
    Stack, max_length),

  CYAML_FIELD_END
};

static const cyaml_schema_value_t
  stack_schema =
{
  YAML_VALUE_PTR (
    Stack, stack_fields_schema),
};

/**
 * Creates a new stack of the given size.
 *
 * @param length Stack size. If -1, the stack will
 *   have unlimited size.
 */
Stack *
stack_new (int length);

int
stack_size (Stack * s);

int
stack_is_empty (Stack * s);

int
stack_is_full (Stack * s);

void *
stack_peek (Stack * s);

void *
stack_peek_last (Stack * s);

void
stack_push (Stack *    s,
            void *     element);

void *
stack_pop (Stack * s);

/**
 * Pops the last element and moves everything back.
 */
void *
stack_pop_last (Stack * s);

void
stack_free_members (
  Stack * s);

void
stack_free (
  Stack * s);

/**
 * @}
 */

#endif
