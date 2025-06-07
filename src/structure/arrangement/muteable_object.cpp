// SPDX-FileCopyrightText: © 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "gui/backend/backend/project.h"
#include "gui/backend/backend/zrythm.h"
#include "structure/arrangement/muteable_object.h"
#include "utils/rt_thread_id.h"

namespace zrythm::structure::arrangement
{
void
init_from (
  MuteableObject        &obj,
  const MuteableObject  &other,
  utils::ObjectCloneType clone_type)
{
  obj.muted_ = other.muted_;
}

void
MuteableObject::set_muted (bool muted, bool fire_events)
{
  muted_ = muted;

  if (fire_events)
    {
      // EVENTS_PUSH (EventType::ET_ARRANGER_OBJECT_CHANGED, this);
    }
}
}
