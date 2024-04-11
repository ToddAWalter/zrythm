.. SPDX-FileCopyrightText: © 2019, 2022 Alexandros Theodotou <alex@zrythm.org>
   SPDX-License-Identifier: GFDL-1.3-invariants-or-later
.. This is part of the Zrythm Manual.
   See the file index.rst for copying conditions.

.. _plugin-browser:

Plugin Browser
==============

The plugin browser makes it easy to browse and
filter plugins installed on your computer.

.. figure:: /_static/img/plugin-browser.png
   :align: center

   Plugin browser

Filter Tabs
-----------
Plugins can be filtered based on various conditions
using the filter tabs at the top. The following
tabs are available:

Collection
  This tab contains your collections. You can create
  collections such as `MySynths` and filter by the
  selected collections.
Author
  Filter by plugin manufacturer/author.

  .. hint:: Built-in plugins can be found under
     `Zrythm DAW` here.

Category
  Filter by category (`Delay`, `Distortion`, etc.).
Protocol
  Filter by protocol (`LV2`, `SFZ`, etc.).

.. tip:: You can mix filters from various tabs for
   a custom search.

.. tip:: Where applicable, you can
   :kbd:`Control`-click to select multiple rows
   or deselect a row.

Filter Buttons
--------------
Additionally to the above, plugins can be filtered
based on their type.

.. figure:: /_static/img/plugin-filter-buttons.png
   :align: center

   Filtering by modulator plugins

See :ref:`plugin-types` for more information about
the various types of plugins there are.

Keyword Search
--------------

Plugins can also be searched by keywords using the
provided search bar.

Collections
-----------

Plugins can be saved inside custom collections. Collections can be created and modified
in the :guilabel:`Collection` tab by using the provided toolbar buttons.

.. figure:: /_static/img/plugin-collections.png
   :align: center

   Plugin collections

Plugins can be added to a collection by right-clicking them and selecting
a collection to add them too. A collection must be created beforehand.

.. figure:: /_static/img/plugin-browser-right-click.png
   :align: center

   Option to add the plugin to *My collection*

.. _instantiating-plugins:

Instantiating Plugins
---------------------
There are a couple of ways to instantiate plugins:

Drag-n-Drop
~~~~~~~~~~~

You can drag and drop the selected plugin into empty space in the
Tracklist or into empty space in the Mixer to
create a new track using that plugin.

.. figure:: /_static/img/drop-plugin-to-tracklist.png
   :align: center

   Dropping a plugin into the tracklist

.. figure:: /_static/img/drop-plugin-to-mixer.png
   :align: center

   Dropping a plugin into the mixer

Alternatively, you can drag the plugin on a mixer slot
to add it there or replace the previous plugin.

.. figure:: /_static/img/drop-plugin-to-slot.png
   :align: center

   Dropping a plugin into a slot

.. tip:: Dragging also works in plugin slots in the inspector.

If the plugin is a
modulator, you can drop it into the Modulators tab.

.. figure:: /_static/img/drop-plugin-to-modulators-tab.png
   :align: center

   Dropping a modulator plugin into the project modulators section

Double Click/Enter
~~~~~~~~~~~~~~~~~~

Double click on the plugin or select it and press the
return key on your keyboard to create a new track
using that plugin.
