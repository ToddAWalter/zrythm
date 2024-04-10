.. SPDX-FileCopyrightText: © 2019-2020 Alexandros Theodotou <alex@zrythm.org>
   SPDX-License-Identifier: GFDL-1.3-invariants-or-later
   This is part of the Zrythm Manual.
   See the file index.rst for copying conditions.

System Requirements
===================

Platform Support
----------------

Zrythm aims to support all platforms, however due to lack of resources we
focus on the most used ones. See the matrix below for support status of each
platform.

○: Supported
△: Untested
×: Not supported

.. list-table:: Platform support matrix
   :header-rows: 1
   :stub-columns: 1

   * - Platform
     - x86_64/AMD64/x64
     - AArch64/ARM64/ARMv8
     - ARMv7
     - PowerPC64
     - i386
     - i686
   * - GNU/Linux
     - ○
     - ○
     - △
     - △
     - △
     - △
   * - FreeBSD
     - ○
     - △
     - △
     - △
     - △
     - △
   * - Windows
     - ○
     - ×
     - ×
     - ×
     - ×
     - ×
   * - MacOS
     - ○
     - ○
     - ×
     - ×
     - ×
     - ×

Backend Support
---------------

○: Supported
△: Has known issues
×: Broken

.. list-table:: Audio backends
   :header-rows: 1

   * - Backend
     - Status
   * - JACK
     - ○
   * - JACK (via PipeWire)
     - ○
   * - PulseAudio (RtAudio)
     - ○
   * - PulseAudio
     - △
   * - SDL2
     - △
   * - ALSA (RtAudio)
     - ○
   * - ALSA
     - ×
   * - WASAPI (RtAudio)
     - ○
   * - CoreAudio (RtAudio)
     - ○

.. list-table:: MIDI backends
   :header-rows: 1

   * - Backend
     - Status
   * - JACK MIDI
     - ○
   * - JACK MIDI (via PipeWire)
     - ○
   * - WindowsMME
     - △
   * - ALSA Sequencer (RtMidi)
     - ○
   * - WindowsMME (RtMidi)
     - ○
   * - CoreMIDI (RtMidi)
     - ○

Recommendations
---------------

CPU
  For smooth operation, we recommend using a CPU with at least 2GHz clock
  speed and at least 4 cores in total.
Monitor
  It is recommended to have at least 16 inches of monitor space to work
  efficiently with Zrythm.
Audio interface
  An audio interface offers low latency and better quality than integrated
  sound cards (especially if recording audio).
MIDI keyboard
  A MIDI keyboard is useful for quickly trying out melodies, recording, and controlling knobs and buttons.

JACK
----
When using the :term:`JACK` backend, JACK needs to
be set up
and configured before running Zrythm. You will find
lots of information online about how to configure
JACK, such as
`Demystifying JACK - A Beginners Guide to Getting Started with JACK <https://linuxaudio.github.io/libremusicproduction/html/articles/demystifying-jack-%E2%80%93-beginners-guide-getting-started-jack.html>`_,
so we will skip this part.

Memory Locking
--------------
Zrythm requires memory locking privileges for reliable, dropout-free operation. In short, if data is not locked into memory, it can be swapped by the kernel, causing xruns when attempting to access the data.

Realtime Scheduling
-------------------
Zrythm requires realtime scheduling privileges for reliable, dropout-free operation. Realtime (RT) scheduling is a feature that enables applications to meet timing deadlines more reliably.

GNU/Linux
+++++++++
To set up these privileges for your user, see
`How do I configure my linux system to allow JACK to use realtime scheduling? <https://jackaudio.org/faq/linux_rt_config.html#systems-using-pam>`_.

FreeBSD
+++++++
To set up these privileges for your user, change
the user class' ``memorylocked`` value in
:file:`/etc/login.conf`.

Open File Limit
---------------
On startup, Zrythm will attempt to increase the
maximum limit of files it can open. You should give
your user enough permissions to allow this.
