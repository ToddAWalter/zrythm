/*
 * Copyright (C) 2019-2020 Alexandros Theodotou <alex at zrythm dot org>
 *
 * This file is part of Zrythm
 *
 * Zrythm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * Zrythm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Zrythm.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdlib.h>

#include "audio/chord_descriptor.h"
#include "gui/backend/chord_editor.h"
#include "project.h"

void
chord_editor_init (
  ChordEditor * self)
{
  self->schema_version =
    CHORD_EDITOR_SCHEMA_VERSION;

  self->num_chords = 0;
  for (int i = 0; i < 12; i++)
    {
      self->chords[i] =
        chord_descriptor_new (
          NOTE_C + i, 1, NOTE_C + i, CHORD_TYPE_MAJ,
          CHORD_ACC_NONE, 0);
      self->num_chords++;
    }

  editor_settings_init (&self->editor_settings);
}
