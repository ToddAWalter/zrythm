// SPDX-FileCopyrightText: © 2021-2022,2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#ifndef __AUDIO_PORT_CONNECTION_H__
#define __AUDIO_PORT_CONNECTION_H__

#include "zrythm-config.h"

#include "dsp/port_identifier.h"
#include "utils/math.h"

#include <QtQmlIntegration>

using namespace zrythm;

/**
 * A connection between two ports.
 */
class PortConnection final
    : public QObject,
      public zrythm::utils::serialization::ISerializable<PortConnection>,
      public ICloneable<PortConnection>
{
  Q_OBJECT
  QML_ELEMENT

public:
  using PortUuid = dsp::PortIdentifier::PortUuid;

  PortConnection (QObject * parent = nullptr);

  PortConnection (
    const PortUuid &src,
    const PortUuid &dest,
    float           multiplier,
    bool            locked,
    bool            enabled,
    QObject *       parent = nullptr);

  void
  init_after_cloning (const PortConnection &other, ObjectCloneType clone_type)
    override;

  void update (float multiplier, bool locked, bool enabled)
  {
    multiplier_ = multiplier;
    locked_ = locked;
    enabled_ = enabled;
  }

  DECLARE_DEFINE_FIELDS_METHOD ();

public:
  PortUuid src_id_;
  PortUuid dest_id_;

  /**
   * Multiplier to apply, where applicable.
   *
   * Range: 0 to 1.
   * Default: 1.
   */
  float multiplier_ = 1.0f;

  /**
   * Whether the connection can be removed or the multiplier edited by the user.
   *
   * Ignored when connecting things internally and only used to deter the user
   * from breaking necessary connections.
   */
  bool locked_ = false;

  /**
   * Whether the connection is enabled.
   *
   * @note The user can disable port connections only if they are not locked.
   */
  bool enabled_ = true;

  /** Used for CV -> control port connections. */
  float base_value_ = 0.0f;
};

bool
operator== (const PortConnection &lhs, const PortConnection &rhs);

DEFINE_OBJECT_FORMATTER (
  PortConnection,
  port_connection,
  [] (const PortConnection &conn) {
    return fmt::format (
      "PortConnection{{src: {}, dest: {}, mult: {:.2f}, locked: {}, enabled: {}}}",
      conn.src_id_, conn.dest_id_, conn.multiplier_, conn.locked_,
      conn.enabled_);
  })

#endif /* __AUDIO_PORT_CONNECTION_H__ */
