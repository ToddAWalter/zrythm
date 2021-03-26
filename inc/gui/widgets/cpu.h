/*
 * Copyright (C) 2019 Alexandros Theodotou <alex at zrythm dot org>
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

/**
 * \file
 *
 * Cpu widget.
 */

#ifndef __GUI_WIDGETS_CPU_H__
#define __GUI_WIDGETS_CPU_H__

#include <gtk/gtk.h>

/**
 * @addtogroup widgets
 *
 * @{
 */

#define CPU_WIDGET_TYPE \
  (cpu_widget_get_type ())
G_DECLARE_FINAL_TYPE (CpuWidget,
                      cpu_widget,
                      Z,
                      CPU_WIDGET,
                      GtkDrawingArea)

typedef struct _CpuWidget
{
  GtkDrawingArea         parent_instance;

  /** CPU load (0-100). */
  int                    cpu;

  /** DSP load (0-100). */
  int                    dsp;

  /** Source func IDs. */
  guint                  cpu_source_id;
  guint                  dsp_source_id;
} CpuWidget;

/**
 * Starts drawing the CPU usage.
 */
void
cpu_widget_setup (CpuWidget * self);

/**
 * @}
 */

#endif
