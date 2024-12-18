// SPDX-FileCopyrightText: © 2021-2022, 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "gui/dsp/control_port.h"
#include "gui/dsp/cv_port.h"
#include "gui/dsp/modulator_macro_processor.h"
#include "gui/dsp/modulator_track.h"
#include "gui/dsp/port.h"

#include "utils/debug.h"
#include "utils/dsp.h"
#include <fmt/format.h>
#include <fmt/printf.h>

using namespace zrythm;

bool
ModulatorMacroProcessor::is_in_active_project () const
{
  return track_ && track_->is_in_active_project ();
}

void
ModulatorMacroProcessor::init_loaded (ModulatorTrack &track)
{
  track_ = &track;

  macro_->init_loaded (*this);
  cv_in_->init_loaded (*this);
  cv_out_->init_loaded (*this);
}

void
ModulatorMacroProcessor::process_block (const EngineProcessTimeInfo time_nfo)
{
  z_return_if_fail_cmp (
    time_nfo.local_offset_ + time_nfo.nframes_, <=, cv_out_->last_buf_sz_);

  /* if there are inputs, multiply by the knob value */
  if (!cv_in_->srcs_.empty ())
    {
      utils::float_ranges::copy (
        &cv_out_->buf_[time_nfo.local_offset_],
        &cv_in_->buf_[time_nfo.local_offset_], time_nfo.nframes_);
      utils::float_ranges::mul_k2 (
        &cv_out_->buf_[time_nfo.local_offset_], macro_->get_val (),
        time_nfo.nframes_);
    }
  /* else if there are no inputs, set the knob value as the output */
  else
    {
      utils::float_ranges::fill (
        &cv_out_->buf_[time_nfo.local_offset_],
        macro_->get_val () * (cv_out_->range_.maxf_ - cv_out_->range_.minf_)
          + cv_out_->range_.minf_,
        time_nfo.nframes_);
    }
}

ModulatorMacroProcessor::ModulatorMacroProcessor (ModulatorTrack * track, int idx)
    : track_ (track)
{

  name_ = format_str (QObject::tr ("Macro {}").toStdString (), idx + 1);
  macro_ = std::make_unique<ControlPort> (name_);
  macro_->set_owner (*this);
  macro_->id_->sym_ = fmt::format ("macro_{}", idx + 1);
  macro_->range_ = { 0.f, 1.f };
  macro_->deff_ = 0.f;
  macro_->set_control_value (0.75f, false, false);
  macro_->id_->flags_ |= dsp::PortIdentifier::Flags::Automatable;
  macro_->id_->flags_ |= dsp::PortIdentifier::Flags::ModulatorMacro;
  macro_->id_->port_index_ = idx;

  cv_in_ = std::make_unique<CVPort> (
    format_str (QObject::tr ("Macro CV In {}").toStdString (), idx + 1),
    dsp::PortFlow::Input);
  cv_in_->set_owner (*this);
  cv_in_->id_->sym_ = fmt::format ("macro_cv_in_{}", idx + 1);
  cv_in_->id_->flags_ |= dsp::PortIdentifier::Flags::ModulatorMacro;
  cv_in_->id_->port_index_ = idx;

  cv_out_ = std::make_unique<CVPort> (
    format_str (QObject::tr ("Macro CV Out {}").toStdString (), idx + 1),
    dsp::PortFlow::Output);
  cv_out_->set_owner (*this);
  cv_out_->id_->sym_ = fmt::format ("macro_cv_out_{}", idx + 1);
  cv_out_->id_->flags_ |= dsp::PortIdentifier::Flags::ModulatorMacro;
  cv_out_->id_->port_index_ = idx;
}

void
ModulatorMacroProcessor::set_port_metadata_from_owner (
  dsp::PortIdentifier &id,
  PortRange           &range) const
{
  id.owner_type_ = dsp::PortIdentifier::OwnerType::ModulatorMacroProcessor;
  z_return_if_fail (get_track ());
  id.track_name_hash_ = get_track ()->get_name_hash ();
}

std::string
ModulatorMacroProcessor::get_full_designation_for_port (
  const dsp::PortIdentifier &id) const
{
  return fmt::format ("Modulator Macro Processor/{}", id.label_);
}

void
ModulatorMacroProcessor::init_after_cloning (
  const ModulatorMacroProcessor &other)
{
  name_ = other.name_;
  cv_in_ = other.cv_in_->clone_unique ();
  cv_out_ = other.cv_out_->clone_unique ();
  macro_ = other.macro_->clone_unique ();
}
