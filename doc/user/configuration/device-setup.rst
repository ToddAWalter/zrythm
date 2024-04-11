.. SPDX-FileCopyrightText: © 2019-2022, 2024 Alexandros Theodotou <alex@zrythm.org>
   SPDX-License-Identifier: GFDL-1.3-invariants-or-later
.. This is part of the Zrythm Manual.
   See the file index.rst for copying conditions.

Devices
=======

Device Setup
++++++++++++

Connecting MIDI and Audio Devices
---------------------------------

Zrythm will auto-connect to the devices specified in
the
:ref:`Preferences <configuration/preferences:Preferences>`
and make these devices available in
:ref:`track inputs <tracks/inspector-page:Inputs>`
and other places.

JACK/PipeWire
-------------

Zrythm exposes ports to JACK (or PipeWire) when
using the JACK audio and MIDI backend. These ports
can be viewed using a tool like
`Qjackctl <https://qjackctl.sourceforge.io/>`_.

.. figure:: /_static/img/zrythm-in-qjackctl.png
   :align: center

   Device connections in Qjackctl

Zrythm will manage hardware connections on its own,
so users are not expected to have to use such a
patchbay to route MIDI/audio devices, however such
a tool can be used to route signals from other
apps into Zrythm to record their output, or to
route the output of Zrythm into other JACK apps.

.. warning:: Any connections made externally will
   not be remembered by Zrythm.

MIDI Bindings
+++++++++++++

Creating Bindings
-----------------

MIDI device controls can be mapped to controls
inside Zrythm (on controls that support it). After
enabling a device in the
:ref:`Preferences <configuration/preferences:Preferences>`,
a mapping can be created by right-clicking on
eligible controls and selecting
:guilabel:`MIDI learn`.

.. figure:: /_static/img/midi-learn-sends.png
   :align: center

   MIDI learn on channel sends

A window will show up asking you to press a key or
move a knob on your MIDI device.

.. figure:: /_static/img/midi-learn-popup.png
   :align: center

   MIDI learn window

When the mapping is confirmed, the selected control
can be controlled using the MIDI device.

Managing Bindings
-----------------

Device
mappings (bindings) can be found under the
:guilabel:`Bindings` tab in the
:ref:`main panel <zrythm-interface/main-panel:Main Panel>`.

.. figure:: /_static/img/midi-bindings-delete.png
   :align: center

   MIDI CC bindings

Bindings can be enabled/disabled by clicking the checkbox on each row, or
deleted by selecting one or more rows and pressing the :guilabel:`Delete`
button in the toolbar at the bottom.
