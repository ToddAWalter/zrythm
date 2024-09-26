// SPDX-FileCopyrightText: © 2018-2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#ifndef __SETTINGS_SETTINGS_MANAGER_H__
#define __SETTINGS_SETTINGS_MANAGER_H__

#include "zrythm-config.h"

#include <QSettings>

#define DEFINE_SETTING_PROPERTY_WITHOUT_SETTER(ptype, name, default_value) \
\
  Q_PROPERTY ( \
    ptype name READ get_##name WRITE set_##name NOTIFY name##_changed); \
\
public: \
  [[nodiscard]] ptype get_default_##name () const \
  { \
    return default_value; \
  } \
  [[nodiscard]] ptype get_##name () const \
  { \
    return settings_.value (QStringLiteral (#name), default_value) \
      .value<ptype> (); \
  } \
  Q_SIGNAL void name##_changed ();

#define DEFINE_SETTING_PROPERTY(ptype, name, default_value) \
\
  DEFINE_SETTING_PROPERTY_WITHOUT_SETTER (ptype, name, default_value); \
  void set_##name (ptype value) \
  { \
    if ( \
      settings_.value (QStringLiteral (#name), default_value).value<ptype> () \
      != value) \
      { \
        settings_.setValue (QStringLiteral (#name), value); \
        Q_EMIT name##_changed (); \
        settings_.sync (); \
      } \
  }

#define DEFINE_SETTING_PROPERTY_DOUBLE(ptype, name, default_value) \
\
  DEFINE_SETTING_PROPERTY_WITHOUT_SETTER (ptype, name, default_value); \
  void set_##name (ptype value) \
  { \
    if ( \
      !math_doubles_equal ( \
        settings_.value (QStringLiteral (#name), default_value).value<ptype> (), \
        value)) \
      { \
        settings_.setValue (QStringLiteral (#name), value); \
        Q_EMIT name##_changed (); \
        settings_.sync (); \
      } \
  }

class SettingsManager final : public QObject
{
  Q_OBJECT

  DEFINE_SETTING_PROPERTY (
    QString,
    zrythm_user_path,
    QStandardPaths::writableLocation (QStandardPaths::AppDataLocation))
  DEFINE_SETTING_PROPERTY (bool, first_run, true)

  // note: in amplitude (0 to 2)
  DEFINE_SETTING_PROPERTY_DOUBLE (double, metronome_volume, 1.0)

public:
  ~SettingsManager () override;

private:
  SettingsManager () = default;

private:
  QSettings settings_;

public:
  JUCE_DECLARE_SINGLETON_SINGLETHREADED (SettingsManager, true)
};

#undef DEFINE_SETTING_PROPERTY

#endif // __SETTINGS_SETTINGS_MANAGER_H__