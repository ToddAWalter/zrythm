// SPDX-FileCopyrightText: © 2025 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#pragma once

#include "structure/scenes/scene.h"

#include <QtQmlIntegration>

namespace zrythm::structure::scenes
{
class ClipLauncher : public QObject
{
  Q_OBJECT
  Q_PROPERTY (zrythm::structure::scenes::SceneList * scenes READ scenes CONSTANT)
  QML_ELEMENT
  QML_UNCREATABLE ("")

public:
  ClipLauncher (
    arrangement::ArrangerObjectRegistry &object_registry,
    const tracks::TrackCollection       &track_collection,
    QObject *                            parent = nullptr);

  SceneList * scenes () const { return scene_list_.get (); }

  // Scene management
  Q_INVOKABLE Scene * addScene ();

private:
  utils::QObjectUniquePtr<SceneList> scene_list_;

  arrangement::ArrangerObjectRegistry &object_registry_;
  const tracks::TrackCollection       &track_collection_;
};

} // namespace zrythm::structure::scenes
