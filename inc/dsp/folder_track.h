// SPDX-FileCopyrightText: © 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#ifndef __AUDIO_FOLDER_TRACK_H__
#define __AUDIO_FOLDER_TRACK_H__

#include "dsp/channel_track.h"
#include "dsp/foldable_track.h"

/**
 * @brief A track that can contain other tracks.
 */
class FolderTrack final
    : public FoldableTrack,
      public ChannelTrack,
      public ICloneable<FolderTrack>,
      public ISerializable<FolderTrack>,
      public InitializableObjectFactory<FolderTrack>
{
  friend class InitializableObjectFactory<FolderTrack>;

public:
  bool get_listened () const override
  {
    return is_status (MixerStatus::Listened);
  }

  bool get_muted () const override { return is_status (MixerStatus::Muted); }

  bool get_implied_soloed () const override
  {
    return is_status (MixerStatus::ImpliedSoloed);
  }

  bool get_soloed () const override { return is_status (MixerStatus::Soloed); }
  void init_loaded () override { }

  void init_after_cloning (const FolderTrack &other) override
  {
    FoldableTrack::copy_members_from (other);
    Track::copy_members_from (other);
  }

  DECLARE_DEFINE_FIELDS_METHOD ();

private:
  FolderTrack () = default;
  FolderTrack (const std::string &name, int pos);

  bool initialize () override;
};

#endif /* __AUDIO_FOLDER_TRACK_H__ */