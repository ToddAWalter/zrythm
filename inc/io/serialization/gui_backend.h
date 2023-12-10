// SPDX-FileCopyrightText: © 2023 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

/**
 * \file
 *
 * Track serialization.
 */

#ifndef __IO_GUI_BACKEND_H__
#define __IO_GUI_BACKEND_H__

#include "utils/types.h"

#include <glib.h>

#include <yyjson.h>

TYPEDEF_STRUCT (ClipEditor);
TYPEDEF_STRUCT (Timeline);
TYPEDEF_STRUCT (SnapGrid);
TYPEDEF_STRUCT (QuantizeOptions);

bool
clip_editor_serialize_to_json (
  yyjson_mut_doc *   doc,
  yyjson_mut_val *   ce_obj,
  const ClipEditor * ce,
  GError **          error);

bool
timeline_serialize_to_json (
  yyjson_mut_doc * doc,
  yyjson_mut_val * t_obj,
  const Timeline * t,
  GError **        error);

bool
snap_grid_serialize_to_json (
  yyjson_mut_doc * doc,
  yyjson_mut_val * sg_obj,
  const SnapGrid * sg,
  GError **        error);

bool
quantize_options_serialize_to_json (
  yyjson_mut_doc *        doc,
  yyjson_mut_val *        qo_obj,
  const QuantizeOptions * qo,
  GError **               error);

#endif // __IO_GUI_BACKEND_H__
