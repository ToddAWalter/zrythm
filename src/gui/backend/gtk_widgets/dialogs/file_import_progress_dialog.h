// SPDX-FileCopyrightText: © 2023 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

/**
 * @file
 *
 * File import progress dialog.
 */

#ifndef __GUI_WIDGETS_DIALOGS_FILE_IMPORT_PROGRESS_PROGRESS_DIALOG_H__
#define __GUI_WIDGETS_DIALOGS_FILE_IMPORT_PROGRESS_PROGRESS_DIALOG_H__

#include "common/dsp/track.h"
#include "common/utils/types.h"
#include "gui/backend/gtk_widgets/libadwaita_wrapper.h"

#define FILE_IMPORT_PROGRESS_PROGRESS_DIALOG_TYPE \
  (file_import_progress_dialog_get_type ())
G_DECLARE_FINAL_TYPE (
  FileImportProgressDialog,
  file_import_progress_dialog,
  Z,
  FILE_IMPORT_PROGRESS_DIALOG,
  AdwMessageDialog);

struct FileImportInfo;
TYPEDEF_STRUCT_UNDERSCORED (FileImport);

/**
 * @addtogroup widgets
 *
 * @{
 */

/**
 * A progress dialog for file import.
 */
using FileImportProgressDialogWidget = struct _FileImportProgressDialog
{
  AdwMessageDialog parent_instance;

  char **          filepaths;
  int              num_files_total;
  FileImportInfo * import_info;
  int              num_files_remaining;
  GCancellable *   cancellable;

  /** Pointer array of FileImport instances. */
  GPtrArray * file_imports;

  /** Returned arrays of regions. */
  std::vector<std::vector<std::shared_ptr<Region>>> region_arrays;

  TracksReadyCallback tracks_ready_cb;
};

/**
 * Creates an instance of FileImportProgressDialog for the
 * given array of filepaths (NULL-delimited).
 */
FileImportProgressDialog *
file_import_progress_dialog_new (
  const char **       filepaths,
  FileImportInfo *    import_info,
  TracksReadyCallback tracks_ready_cb,
  GtkWidget *         parent);

/**
 * Runs the dialog and imports each file asynchronously while
 * presenting progress info.
 */
void
file_import_progress_dialog_run (FileImportProgressDialog * self);

/**
 * @}
 */

#endif
