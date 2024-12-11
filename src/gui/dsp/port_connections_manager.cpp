// SPDX-FileCopyrightText: © 2021-2022, 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "dsp/port_identifier.h"
#include "gui/backend/backend/project.h"
#include "gui/backend/backend/zrythm.h"
#include "gui/dsp/channel_send.h"
#include "gui/dsp/port_connections_manager.h"
#include "utils/rt_thread_id.h"

#include <fmt/format.h>

PortConnectionsManager::PortConnectionsManager (QObject * parent)
    : QObject (parent)
{
}

void
PortConnectionsManager::init_after_cloning (const PortConnectionsManager &other)
{
  connections_.reserve (other.connections_.size ());
  for (const auto &conn : other.connections_)
    {
      connections_.push_back (conn->clone_raw_ptr ());
      connections_.back ()->setParent (this);
    }
  regenerate_hashtables ();
}

void
PortConnectionsManager::disconnect_port_collection (
  std::vector<Port *> &ports,
  bool                 deleting)
{
  /* can only be called from the gtk thread */
  z_return_if_fail (ZRYTHM_IS_QT_THREAD);

  /* go through each port */
  for (auto * port : ports)
    {
      ensure_disconnect_all (*port->id_);
      port->srcs_.clear ();
      port->dests_.clear ();
      port->deleting_ = deleting;
    }
}

void
PortConnectionsManager::add_or_replace_connection (
  ConnectionHashTable  &ht,
  const PortIdentifier &id,
  const PortConnection &conn)
{
  auto   it = ht.find (id.get_hash ());
  auto * clone_conn = conn.clone_raw_ptr ();
  clone_conn->setParent (this);
  if (it != ht.end ())
    {
      z_return_if_fail (it->first == id.get_hash ());
      it->second.push_back (clone_conn);
    }
  else
    {
      ht.emplace (id.get_hash (), ConnectionsVector{ clone_conn });
    }
}

bool
PortConnectionsManager::replace_connection (
  const PortConnection &before,
  const PortConnection &after)
{
  auto it = std::find (connections_.begin (), connections_.end (), before);
  if (it == connections_.end ())
    {
      z_return_val_if_reached (false);
    }

  (*it)->setParent (nullptr);
  (*it)->deleteLater ();
  *it = after.clone_raw_ptr ();
  (*it)->setParent (this);
  regenerate_hashtables ();

  return true;
}

void
PortConnectionsManager::regenerate_hashtables ()
{
  src_ht_.clear ();
  dest_ht_.clear ();

  for (auto &conn : connections_)
    {
      add_or_replace_connection (src_ht_, *conn->src_id_, *conn);
      add_or_replace_connection (dest_ht_, *conn->dest_id_, *conn);
    }

#if 0
  unsigned int srcs_size = src_ht_.size ();
  unsigned int dests_size = dest_ht_.size ();
  z_debug (
    "Sources hashtable: {} elements | "
    "Destinations hashtable: {} elements",
    srcs_size, dests_size);
#endif
}

int
PortConnectionsManager::get_sources_or_dests (
  ConnectionsVector *   arr,
  const PortIdentifier &id,
  bool                  sources) const
{
  /* note: we look at the opposite hashtable */
  const ConnectionHashTable &ht = sources ? dest_ht_ : src_ht_;
  auto                       it = ht.find (id.get_hash ());

  if (it == ht.end ())
    {
      return 0;
    }

  /* append to the given array */
  if (arr != nullptr)
    {
      for (const auto &conn : it->second)
        {
          arr->push_back (conn);
        }
    }

  /* return number of connections found */
  return (int) it->second.size ();
}

int
PortConnectionsManager::get_unlocked_sources_or_dests (
  ConnectionsVector *   arr,
  const PortIdentifier &id,
  bool                  sources) const
{
  const ConnectionHashTable &ht = sources ? dest_ht_ : src_ht_;
  auto                       it = ht.find (id.get_hash ());

  if (it == ht.end ())
    return 0;

  int ret = 0;
  for (const auto &conn : it->second)
    {
      if (!conn->locked_)
        ret++;

      /* append to the given array */
      if (arr != nullptr)
        {
          arr->push_back (conn);
        }
    }

  /* return number of connections found */
  return (int) ret;
}

PortConnection *
PortConnectionsManager::get_source_or_dest (
  const PortIdentifier &id,
  bool                  sources) const
{
  ConnectionsVector conns;
  int               num_conns = get_sources_or_dests (&conns, id, sources);
  if (num_conns != 1)
    {
      auto buf = id.print_to_str ();
      z_error (
        "expected 1 {}, found {} "
        "connections for\n{}",
        sources ? "source" : "destination", num_conns, buf);
      return nullptr;
    }

  return conns.front ();
}

PortConnection *
PortConnectionsManager::find_connection (
  const PortIdentifier &src,
  const PortIdentifier &dest) const
{
  auto it = std::find_if (
    connections_.begin (), connections_.end (), [&] (const auto &conn) {
      return *conn->src_id_ == src && *conn->dest_id_ == dest;
    });
  return it != connections_.end () ? (*it) : nullptr;
}

const PortConnection *
PortConnectionsManager::ensure_connect (
  const PortIdentifier &src,
  const PortIdentifier &dest,
  float                 multiplier,
  bool                  locked,
  bool                  enabled)
{
  z_warn_if_fail (ZRYTHM_IS_QT_THREAD);

  for (auto &conn : connections_)
    {
      if (*conn->src_id_ == src && *conn->dest_id_ == dest)
        {
          conn->update (multiplier, locked, enabled);
          regenerate_hashtables ();
          return conn;
        }
    }

  connections_.push_back (
    new PortConnection (src, dest, multiplier, locked, enabled, this));
  const auto &conn = connections_.back ();

  if (this == get_active_instance ())
    {
      z_debug (
        "New connection: <{}>; have {} connections", conn->print_to_str (),
        connections_.size ());
    }

  regenerate_hashtables ();

  return conn;
}

void
PortConnectionsManager::remove_connection (const size_t idx)
{
  const auto conn = connections_[idx];

  connections_.erase (connections_.begin () + idx);

  if (this == get_active_instance ())
    {
      z_debug (
        "Disconnected <{}>; have {} connections", conn->print_to_str (),
        connections_.size ());
    }

  regenerate_hashtables ();
}

bool
PortConnectionsManager::ensure_disconnect (
  const PortIdentifier &src,
  const PortIdentifier &dest)
{
  z_return_val_if_fail (ZRYTHM_IS_QT_THREAD, false);

  for (size_t i = 0; i < connections_.size (); i++)
    {
      const auto &conn = connections_[i];
      if (*conn->src_id_ == src && *conn->dest_id_ == dest)
        {
          remove_connection (i);
          return true;
        }
    }

  return false;
}

void
PortConnectionsManager::ensure_disconnect_all (const PortIdentifier &pi)
{
  z_return_if_fail (ZRYTHM_IS_QT_THREAD);

  for (size_t i = 0; i < connections_.size (); i++)
    {
      const auto &conn = connections_[i];
      if (*conn->src_id_ == pi || *conn->dest_id_ == pi)
        {
          remove_connection (i);
          i--;
        }
    }
}
bool
PortConnectionsManager::contains_connection (const PortConnection &conn) const
{
  return std::find (connections_.begin (), connections_.end (), conn)
         != connections_.end ();
}

void
PortConnectionsManager::reset_connections (const PortConnectionsManager * other)
{
  clear_connections ();

  if (other)
    {
      connections_ = other->connections_;
      regenerate_hashtables ();
    }
}

void
PortConnectionsManager::print_ht (const ConnectionHashTable &ht)
{
  std::string str;
  z_trace ("ht size: {}", ht.size ());
  for (const auto &[key, value] : ht)
    {
      for (const auto &conn : value)
        {
          const auto &id = &ht == &src_ht_ ? conn->dest_id_ : conn->src_id_;
          str +=
            fmt::format ("{}\n  {}\n", id->get_label (), conn->print_to_str ());
        }
    }
  z_info ("{}", str.c_str ());
}

void
PortConnectionsManager::print () const
{
  std::string str =
    fmt::format ("Port connections manager ({}):\n", (void *) this);
  for (size_t i = 0; i < connections_.size (); i++)
    {
      str += fmt::format ("[{}] {}\n", i, connections_[i]->print_to_str ());
    }
  z_info ("{}", str.c_str ());
}

PortConnectionsManager *
PortConnectionsManager::get_active_instance ()
{
  auto prj = Project::get_active_instance ();
  if (prj)
    {

      return prj->port_connections_manager_;
    }
  return nullptr;
}
