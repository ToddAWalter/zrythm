// SPDX-FileCopyrightText: © 2022-2023 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "dsp/port.h"
#include "utils/objects.h"

#include "schemas/dsp/port.h"

Port *
port_create_from_v1 (Port_v1 * old)
{
  if (!old)
    return NULL;

  Port * self = object_new (Port);
  self->schema_version = PORT_SCHEMA_VERSION;
  PortIdentifier * id = port_identifier_create_from_v1 (&old->id);
  self->id = *id;
  self->exposed_to_backend = old->exposed_to_backend;
  self->control = old->control;
  self->minf = old->minf;
  self->maxf = old->maxf;
  self->zerof = old->zerof;
  self->deff = old->deff;
  self->carla_param_id = old->carla_param_id;

  return self;
}

StereoPorts *
stereo_ports_create_from_v1 (StereoPorts_v1 * old)
{
  if (!old)
    return NULL;

  StereoPorts * self = object_new (StereoPorts);
  self->schema_version = STEREO_PORTS_SCHEMA_VERSION;
  self->l = port_create_from_v1 (old->l);
  self->r = port_create_from_v1 (old->r);

  return self;
}
