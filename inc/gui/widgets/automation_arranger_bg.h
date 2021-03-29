/*
 * Copyright (C) 2019 Alexandros Theodotou <alex at zrythm dot org>
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#if 0

/**
 * \file
 *
 * The background (main overlay child) of the
 * AutomationArrangerWidget.
 */

#ifndef __GUI_WIDGETS_AUTOMATION_ARRANGER_BG_H__
#define __GUI_WIDGETS_AUTOMATION_ARRANGER_BG_H__

#include "gui/widgets/arranger_bg.h"

#include <gtk/gtk.h>

#define AUTOMATION_ARRANGER_BG_WIDGET_TYPE \
  (automation_arranger_bg_widget_get_type ())
G_DECLARE_FINAL_TYPE (
  AutomationArrangerBgWidget,
  automation_arranger_bg_widget,
  Z, AUTOMATION_ARRANGER_BG_WIDGET,
  ArrangerBgWidget)

/**
 * @addtogroup widgets
 *
 * @{
 */

#define AUTOMATION_ARRANGER_BG \
  Z_AUTOMATION_ARRANGER_BG_WIDGET ( \
    arranger_widget_get_private ( \
      Z_ARRANGER_WIDGET (MW_AUTOMATION_ARRANGER))->bg)

typedef struct _AutomationArrangerBgWidget
{
  ArrangerBgWidget         parent_instance;
} AutomationArrangerBgWidget;

/**
 * Creates a AutomationArrangerBgWidget for the given
 * arranger and ruler.
 */
AutomationArrangerBgWidget *
automation_arranger_bg_widget_new (
  RulerWidget *    ruler,
  ArrangerWidget * arranger);

/**
 * @}
 */

#endif
#endif
