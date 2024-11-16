// SPDX-FileCopyrightText: © 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "common/utils/datetime.h"
#include "common/utils/io.h"
#include "common/utils/logger.h"
#include "gui/backend/backend/project_info.h"

using namespace zrythm;

ProjectInfo::ProjectInfo (const std::string &name, const std::string &filename)
    : name_ (name)
{
  if (filename.empty ())
    {
      filename_ = "-";
      modified_ = 0;
      modified_str_ = "-";
    }
  else
    {
      filename_ = filename;
      modified_ = utils::io::file_get_last_modified_datetime (filename);
      if (modified_ == -1)
        {
          modified_str_ = PROJECT_INFO_FILE_NOT_FOUND_STR.toStdString ();
          modified_ = std::numeric_limits<int64_t>::max ();
        }
      else
        {
          modified_str_ = datetime_epoch_to_str (modified_);
        }
      z_return_if_fail (!modified_str_.empty ());
    }
}

void
ProjectInfo::destroy_func (void * data)
{
  delete static_cast<ProjectInfo *> (data);
}