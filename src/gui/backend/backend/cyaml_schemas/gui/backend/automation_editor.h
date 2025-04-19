// SPDX-FileCopyrightText: © 2019-2022 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

/**
 * @file
 *
 * Automation editor schema.
 */

#ifndef __SCHEMAS_GUI_BACKEND_AUTOMATION_EDITOR_H__
#define __SCHEMAS_GUI_BACKEND_AUTOMATION_EDITOR_H__

#include "gui/backend/backend/cyaml_schemas/gui/backend/editor_settings.h"
#include "utils/yaml.h"

typedef struct AutomationEditor_v1
{
  int               schema_version;
  EditorSettings_v1 editor_settings;
} AutomationEditor_v1;

static const cyaml_schema_field_t automation_editor_fields_schema_v1[] = {
  YAML_FIELD_INT (AutomationEditor_v1, schema_version),
  YAML_FIELD_MAPPING_EMBEDDED (
    AutomationEditor_v1,
    editor_settings,
    editor_settings_fields_schema_v1),

  CYAML_FIELD_END
};

static const cyaml_schema_value_t automation_editor_schema_v1 = {
  YAML_VALUE_PTR (AutomationEditor_v1, automation_editor_fields_schema_v1),
};

#endif
