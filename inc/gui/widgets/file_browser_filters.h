// SPDX-FileCopyrightText: © 2021 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

/**
 * \file
 *
 * File auditioner controls.
 */

#ifndef __GUI_WIDGETS_FILE_BROWSER_FILTERS_H__
#define __GUI_WIDGETS_FILE_BROWSER_FILTERS_H__

#include "zrythm-config.h"

#include "dsp/supported_file.h"
#include "utils/types.h"

#include <gtk/gtk.h>

#define FILE_BROWSER_FILTERS_WIDGET_TYPE \
  (file_browser_filters_widget_get_type ())
G_DECLARE_FINAL_TYPE (
  FileBrowserFiltersWidget,
  file_browser_filters_widget,
  Z,
  FILE_BROWSER_FILTERS_WIDGET,
  GtkBox)

/**
 * @addtogroup widgets
 *
 * @{
 */

enum class FileBrowserFilterType
{
  FILE_BROWSER_FILTER_NONE,
  FILE_BROWSER_FILTER_AUDIO,
  FILE_BROWSER_FILTER_MIDI,
  FILE_BROWSER_FILTER_PRESET,
};

/**
 * File auditioner controls used in file browsers.
 */
typedef struct _FileBrowserFiltersWidget
{
  GtkBox parent_instance;

  GtkToggleButton * toggle_audio;
  GtkToggleButton * toggle_midi;
  GtkToggleButton * toggle_presets;

  /** Callbacks. */
  GtkWidget *     owner;
  GenericCallback refilter_files;
} FileBrowserFiltersWidget;

/**
 * Sets up a FileBrowserFiltersWidget.
 */
void
file_browser_filters_widget_setup (
  FileBrowserFiltersWidget * self,
  GtkWidget *                owner,
  GenericCallback            refilter_files_cb);

/**
 * @}
 */

#endif
