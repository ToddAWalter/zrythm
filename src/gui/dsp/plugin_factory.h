// SPDX-FileCopyrightText: © 2025 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#pragma once

#include <utility>

#include "gui/backend/backend/settings_manager.h"
#include "gui/dsp/plugin_all.h"
#include "structure/tracks/track_all.h"

/**
 * @brief Factory for plugins.
 *
 * @note API that starts with `add` adds the object to the project and should be
 * used in most cases. API that starts with `create` only creates and registers
 * the object but does not add it to the project (this should only be used
 * internally).
 */
class PluginFactory : public QObject
{
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE ("")

public:
  using PluginConfiguration = zrythm::plugins::PluginConfiguration;

  PluginFactory () = delete;
  PluginFactory (
    PluginRegistry                               &registry,
    dsp::ProcessorBase::ProcessorBaseDependencies dependencies,
    gui::SettingsManager                         &settings_mgr,
    QObject *                                     parent = nullptr)
      : QObject (parent), plugin_registry_ (registry),
        dependencies_ (dependencies), settings_manager_ (settings_mgr)
  {
  }

  static PluginFactory * get_instance ();

  template <typename PluginT> class Builder
  {
    friend class PluginFactory;

  private:
    explicit Builder (
      PluginRegistry                               &registry,
      dsp::ProcessorBase::ProcessorBaseDependencies dependencies)
        : registry_ (registry), dependencies_ (dependencies)
    {
    }

    Builder &with_settings_manager (gui::SettingsManager &settings_manager)
    {
      settings_manager_ = settings_manager;
      return *this;
    }

    Builder &with_setting (const PluginConfiguration &setting)
    {
      setting_ = setting;
      return *this;
    }

  public:
    auto build ()
    {
      auto obj_ref = [&] () {
        auto ref = registry_.create_object<PluginT> (dependencies_);
        ref.template get_object_as<PluginT> ()->set_setting (*setting_);
        return std::move (ref);
      }();

      // auto * obj = std::get<PluginT *> (obj_ref.get_object ());

      return obj_ref;
    }

  private:
    PluginRegistry                               &registry_;
    dsp::ProcessorBase::ProcessorBaseDependencies dependencies_;
    OptionalRef<gui::SettingsManager>             settings_manager_;
    OptionalRef<const PluginConfiguration>        setting_;
  };

  template <typename PluginT> auto get_builder () const
  {
    auto builder =
      Builder<PluginT> (plugin_registry_, dependencies_)
        .with_settings_manager (settings_manager_);
    return builder;
  }

public:
  PluginUuidReference
  create_plugin_from_setting (const PluginConfiguration &setting) const
  {
    auto obj_ref =
      get_builder<CarlaNativePlugin> ().with_setting (setting).build ();
    return obj_ref;
  }

  template <typename PluginT>
  auto clone_new_object_identity (const PluginT &other) const
  {
    return plugin_registry_.clone_object (other, plugin_registry_);
  }

  template <typename PluginT>
  auto clone_object_snapshot (const PluginT &other, QObject &owner) const
  {
    PluginT * new_obj{};

    new_obj = utils::clone_qobject (
      other, &owner, utils::ObjectCloneType::Snapshot, plugin_registry_);
    return new_obj;
  }

private:
  PluginRegistry                               &plugin_registry_;
  dsp::ProcessorBase::ProcessorBaseDependencies dependencies_;
  gui::SettingsManager                         &settings_manager_;
};
