// SPDX-FileCopyrightText: © 2024-2025 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "structure/arrangement/colored_object.h"
#include "structure/tracks/track_all.h"

namespace zrythm::structure::arrangement
{
void
init_from (
  ColoredObject         &obj,
  const ColoredObject   &other,
  utils::ObjectCloneType clone_type)
{
  obj.color_ = other.color_;
  obj.use_color_ = other.use_color_;
}

QColor
ColoredObject::get_effective_color () const
{
  if (use_color_)
    {
      return color_.to_qcolor ();
    }

  auto track_var = get_track ();
  return std::visit (
    [&] (auto &&track) { return track->getColor (); }, track_var);
}
}
