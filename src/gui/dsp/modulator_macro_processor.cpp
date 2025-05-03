// SPDX-FileCopyrightText: © 2021-2022, 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "gui/dsp/control_port.h"
#include "gui/dsp/cv_port.h"
#include "gui/dsp/modulator_macro_processor.h"
#include "gui/dsp/modulator_track.h"
#include "gui/dsp/port.h"
#include "utils/debug.h"

#include <fmt/format.h>
#include <fmt/printf.h>

using namespace zrythm;

ModulatorMacroProcessor::ModulatorMacroProcessor (
  const DeserializationDependencyHolder &dh)
    : ModulatorMacroProcessor (
        dh.get<std::reference_wrapper<PortRegistry>> ().get (),
        std::addressof (dh.get<std::reference_wrapper<ModulatorTrack>> ().get ()),
        std::nullopt,
        false)
{
}

ModulatorMacroProcessor::ModulatorMacroProcessor (
  PortRegistry      &port_registry,
  ModulatorTrack *   track,
  std::optional<int> idx,
  bool               new_identity)
    : track_ (track)
{
  if (new_identity)
    {
      assert (idx.has_value ());
      name_ = format_str (
        utils::qstring_to_std_string (QObject::tr ("Macro {}")), *idx + 1);
      {
        macro_id_ = port_registry.create_object<ControlPort> (name_);
        auto &macro = get_macro_port ();
        macro.set_owner (*this);
        macro.id_->sym_ = fmt::format ("macro_{}", *idx + 1);
        macro.range_ = { 0.f, 1.f };
        macro.deff_ = 0.f;
        macro.set_control_value (0.75f, false, false);
        macro.id_->flags_ |= dsp::PortIdentifier::Flags::Automatable;
        macro.id_->flags_ |= dsp::PortIdentifier::Flags::ModulatorMacro;
        macro.id_->port_index_ = *idx;
      }

      {
        cv_in_id_ = port_registry.create_object<CVPort> (
          format_str (
            utils::qstring_to_std_string (QObject::tr ("Macro CV In {}")),
            *idx + 1),
          dsp::PortFlow::Input);
        auto &cv_in = get_cv_in_port ();
        cv_in.set_owner (*this);
        cv_in.id_->sym_ = fmt::format ("macro_cv_in_{}", *idx + 1);
        cv_in.id_->flags_ |= dsp::PortIdentifier::Flags::ModulatorMacro;
        cv_in.id_->port_index_ = *idx;
      }

      {
        cv_out_id_ = port_registry.create_object<CVPort> (
          format_str (
            utils::qstring_to_std_string (QObject::tr ("Macro CV Out {}")),
            *idx + 1),
          dsp::PortFlow::Output);
        auto &cv_out = get_cv_out_port ();
        cv_out.set_owner (*this);
        cv_out.id_->sym_ = fmt::format ("macro_cv_out_{}", *idx + 1);
        cv_out.id_->flags_ |= dsp::PortIdentifier::Flags::ModulatorMacro;
        cv_out.id_->port_index_ = *idx;
      }
    }
}

void
ModulatorMacroProcessor::define_fields (const utils::serialization::Context &ctx)
{
  serialize_fields (
    ctx, make_field ("name", name_), make_field ("cvIn", cv_in_id_),
    make_field ("cvOut", cv_out_id_), make_field ("macro", macro_id_));
}

bool
ModulatorMacroProcessor::is_in_active_project () const
{
  return track_ && track_->is_in_active_project ();
}

void
ModulatorMacroProcessor::init_loaded (ModulatorTrack &track)
{
  track_ = &track;

  get_macro_port ().init_loaded (*this);
  get_cv_in_port ().init_loaded (*this);
  get_cv_out_port ().init_loaded (*this);
}

void
ModulatorMacroProcessor::process_block (const EngineProcessTimeInfo time_nfo)
{
  auto &macro = get_macro_port ();
  auto &cv_in = get_cv_in_port ();
  auto &cv_out = get_cv_out_port ();

  z_return_if_fail_cmp (
    time_nfo.local_offset_ + time_nfo.nframes_, <=,
    get_cv_out_port ().last_buf_sz_);

  /* if there are inputs, multiply by the knob value */
  if (!cv_in.srcs_.empty ())
    {
      utils::float_ranges::copy (
        &cv_out.buf_[time_nfo.local_offset_],
        &cv_in.buf_[time_nfo.local_offset_], time_nfo.nframes_);
      utils::float_ranges::mul_k2 (
        &cv_out.buf_[time_nfo.local_offset_], macro.get_val (),
        time_nfo.nframes_);
    }
  /* else if there are no inputs, set the knob value as the output */
  else
    {
      utils::float_ranges::fill (
        &cv_out.buf_[time_nfo.local_offset_],
        macro.get_val () * (cv_out.range_.maxf_ - cv_out.range_.minf_)
          + cv_out.range_.minf_,
        time_nfo.nframes_);
    }
}

void
ModulatorMacroProcessor::set_port_metadata_from_owner (
  dsp::PortIdentifier &id,
  PortRange           &range) const
{
  id.owner_type_ = dsp::PortIdentifier::OwnerType::ModulatorMacroProcessor;
  z_return_if_fail (get_track ());
  id.set_track_id (get_track ()->get_uuid ());
}

std::string
ModulatorMacroProcessor::get_full_designation_for_port (
  const dsp::PortIdentifier &id) const
{
  return fmt::format ("Modulator Macro Processor/{}", id.label_);
}

void
ModulatorMacroProcessor::init_after_cloning (
  const ModulatorMacroProcessor &other,
  ObjectCloneType                clone_type)
{
  name_ = other.name_;
  cv_in_id_ = other.cv_in_id_;
  cv_out_id_ = other.cv_out_id_;
  macro_id_ = other.macro_id_;
}
