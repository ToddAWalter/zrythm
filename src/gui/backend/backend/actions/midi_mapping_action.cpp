// SPDX-FileCopyrightText: © 2020-2022, 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "common/dsp/channel.h"
#include "common/dsp/midi_mapping.h"
#include "common/dsp/port_identifier.h"
#include "common/dsp/router.h"
#include "gui/backend/backend/actions/midi_mapping_action.h"
#include "gui/backend/backend/project.h"
#include "gui/backend/backend/zrythm.h"

MidiMappingAction::MidiMappingAction (QObject * parent)
    : QObject (parent), UndoableAction (UndoableAction::Type::MidiMapping)
{
}

void
MidiMappingAction::init_after_cloning (const MidiMappingAction &other)
{
  UndoableAction::copy_members_from (other);
  idx_ = other.idx_;
  type_ = other.type_;
  if (other.dest_port_id_ != nullptr)
    {
      dest_port_id_ = other.dest_port_id_->clone_raw_ptr ();
      dest_port_id_->setParent (this);
    }
  dev_port_ =
    other.dev_port_ ? std::make_unique<ExtPort> (*other.dev_port_) : nullptr;
  buf_ = other.buf_;
}

MidiMappingAction::MidiMappingAction (int idx_to_enable_or_disable, bool enable)
    : UndoableAction (UndoableAction::Type::MidiMapping),
      idx_ (idx_to_enable_or_disable),
      type_ (enable ? Type::Enable : Type::Disable)
{
}

MidiMappingAction::MidiMappingAction (
  const std::array<midi_byte_t, 3> buf,
  const ExtPort *                  device_port,
  const Port                      &dest_port)
    : UndoableAction (UndoableAction::Type::MidiMapping), type_ (Type::Bind),
      dest_port_id_ (dest_port.id_->clone_raw_ptr ()),
      dev_port_ (
        (device_port != nullptr)
          ? std::make_unique<ExtPort> (*device_port)
          : nullptr),
      buf_ (buf)
{
  dest_port_id_->setParent (this);
}

MidiMappingAction::MidiMappingAction (int idx_to_unbind)
    : UndoableAction (UndoableAction::Type::MidiMapping), idx_ (idx_to_unbind),
      type_ (Type::Unbind)
{
}

void
MidiMappingAction::bind_or_unbind (bool bind)
{
  if (bind)
    {
      auto port = Port::find_from_identifier (*dest_port_id_);
      idx_ = MIDI_MAPPINGS->mappings_.size ();
      MIDI_MAPPINGS->bind_device (buf_, dev_port_.get (), *port, false);
    }
  else
    {
      auto &mapping = MIDI_MAPPINGS->mappings_[idx_];
      buf_ = mapping->key_;
      dev_port_ =
        mapping->device_port_
          ? std::make_unique<ExtPort> (*mapping->device_port_)
          : nullptr;
      dest_port_id_ = mapping->dest_id_->clone_raw_ptr ();
      dest_port_id_->setParent (this);
      MIDI_MAPPINGS->unbind (idx_, false);
    }
}

void
MidiMappingAction::perform_impl ()
{
  switch (type_)
    {
    case Type::Enable:
      MIDI_MAPPINGS->mappings_[idx_]->enabled_ = true;
      break;
    case Type::Disable:
      MIDI_MAPPINGS->mappings_[idx_]->enabled_ = false;
      break;
    case Type::Bind:
      bind_or_unbind (true);
      break;
    case Type::Unbind:
      bind_or_unbind (false);
      break;
    }

  /* EVENTS_PUSH (EventType::ET_MIDI_BINDINGS_CHANGED, nullptr); */
}

void
MidiMappingAction::undo_impl ()
{
  switch (type_)
    {
    case Type::Enable:
      MIDI_MAPPINGS->mappings_[idx_]->enabled_ = false;
      break;
    case Type::Disable:
      MIDI_MAPPINGS->mappings_[idx_]->enabled_ = true;
      break;
    case Type::Bind:
      bind_or_unbind (false);
      break;
    case Type::Unbind:
      bind_or_unbind (true);
      break;
    }

  /* EVENTS_PUSH (EventType::ET_MIDI_BINDINGS_CHANGED, nullptr); */
}

QString
MidiMappingAction::to_string () const
{
  switch (type_)
    {
    case Type::Enable:
      return QObject::tr ("MIDI mapping enable");
    case Type::Disable:
      return QObject::tr ("MIDI mapping disable");
    case Type::Bind:
      return QObject::tr ("MIDI mapping bind");
    case Type::Unbind:
      return QObject::tr ("MIDI mapping unbind");
    default:
      z_return_val_if_reached ({});
    }
}