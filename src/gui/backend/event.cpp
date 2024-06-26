// SPDX-FileCopyrightText: © 2019-2020 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include <cstdlib>
#include <cstring>

#include "gui/backend/event.h"
#include "utils/objects.h"

#include "gtk_wrapper.h"

ZEvent *
event_new (void)
{
  ZEvent * self = object_new_unresizable (ZEvent);

  return self;
}

void
event_free (ZEvent * self)
{
  g_free_and_null (self->backtrace);
  object_zero_and_free_unresizable (ZEvent, self);
}
