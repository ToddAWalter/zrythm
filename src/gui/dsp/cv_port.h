// SPDX-FileCopyrightText: © 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#ifndef __AUDIO_CV_PORT_H__
#define __AUDIO_CV_PORT_H__

#include "gui/dsp/port.h"
#include "utils/icloneable.h"

/**
 * @addtogroup dsp
 *
 * @{
 */

/**
 * @brief CV port specifics.
 */
class CVPort final : public QObject, public Port, public AudioAndCVPortMixin
{
  Q_OBJECT
  QML_ELEMENT
public:
  CVPort ();
  CVPort (utils::Utf8String label, PortFlow flow);

  bool has_sound () const override;

  [[gnu::hot]] void process_block (EngineProcessTimeInfo time_nfo) override;

  void allocate_audio_bufs (nframes_t max_samples) override;

  void clear_buffer (std::size_t block_length) override;

  friend void
  init_from (CVPort &obj, const CVPort &other, utils::ObjectCloneType clone_type);

private:
  static constexpr std::string_view kRangeKey = "range";
  friend void                       to_json (nlohmann::json &j, const CVPort &p)
  {
    to_json (j, static_cast<const Port &> (p));
    j[kRangeKey] = p.range_;
  }
  friend void from_json (const nlohmann::json &j, CVPort &p)
  {
    from_json (j, static_cast<Port &> (p));
    j.at (kRangeKey).get_to (p.range_);
  }
};

/**
 * @}
 */

#endif
