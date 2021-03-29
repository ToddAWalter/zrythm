/*
 * Copyright (C) 2018-2021 Alexandros Theodotou <alex at zrythm dot org>
 *
 * This file is part of Zrythm
 *
 * Zrythm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version approved by
 * Alexandros Theodotou in a public statement of acceptance.
 *
 * Zrythm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Zrythm.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "gui/accel.h"
#include "utils/gtk.h"
#include "utils/io.h"
#include "utils/resources.h"
#include "utils/string.h"
#include "utils/ui.h"
#include "zrythm.h"

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif

int
z_gtk_widget_destroy_idle (
  GtkWidget * widget)
{
  gtk_widget_destroy (widget);

  return G_SOURCE_REMOVE;
}

int
z_gtk_get_primary_monitor_scale_factor (void)
{
  GdkDisplay * display;
  GdkMonitor * monitor;
  int scale_factor;

  if (ZRYTHM_TESTING || !ZRYTHM_HAVE_UI)
    {
      return 1;
    }

  display =
    gdk_display_get_default ();
  if (!display)
    {
      g_warning ("no display");
      goto return_default_scale_factor;
    }
  monitor =
    gdk_display_get_primary_monitor (display);
  if (!monitor)
    {
      g_warning ("no primary monitor");
      goto return_default_scale_factor;
    }
  scale_factor =
    gdk_monitor_get_scale_factor (monitor);
  if (scale_factor < 1)
    {
      g_warning (
        "invalid scale factor: %d", scale_factor);
      goto return_default_scale_factor;
    }

  return scale_factor;

return_default_scale_factor:
  g_warning (
    "failed to get refresh rate from device, "
    "returning default");
  return 1;
}

/**
 * Returns the refresh rate in Hz.
 */
int
z_gtk_get_primary_monitor_refresh_rate (void)
{
  GdkDisplay * display;
  GdkMonitor * monitor;
  int refresh_rate;

  if (ZRYTHM_TESTING || !ZRYTHM_HAVE_UI)
    {
      return 30;
    }

  display =
    gdk_display_get_default ();
  if (!display)
    {
      g_warning ("no display");
      goto return_default_refresh_rate;
    }
  monitor =
    gdk_display_get_primary_monitor (display);
  if (!monitor)
    {
      g_warning ("no primary monitor");
      goto return_default_refresh_rate;
    }
  refresh_rate =
    /* divide by 1000 because gdk returns the
     * value in milli-Hz */
    gdk_monitor_get_refresh_rate (monitor) / 1000;
  if (refresh_rate == 0)
    {
      g_warning (
        "invalid refresh rate: %d", refresh_rate);
      goto return_default_refresh_rate;
    }

  return refresh_rate;

return_default_refresh_rate:
  g_warning (
    "failed to get refresh rate from device, "
    "returning default");
  return 30;
}

bool
z_gtk_is_wayland (void)
{
  if (ZRYTHM_TESTING || !ZRYTHM_HAVE_UI)
    {
      return false;
    }

  GdkDisplay * display =
    gdk_display_get_default ();
  g_return_val_if_fail (display, false);

#ifdef GDK_WINDOWING_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (display))
    {
      /*g_debug ("wayland");*/
      return true;
    }
  else
    {
      /*g_debug ("not wayland");*/
    }
#endif

  return false;
}

/**
 * @note Bumps reference, must be decremented after
 * calling.
 */
void
z_gtk_container_remove_all_children (
  GtkContainer * container)
{
  GList *children, *iter;

  children =
    gtk_container_get_children (
      GTK_CONTAINER (container));
  for (iter = children;
       iter != NULL;
       iter = g_list_next (iter))
    {
      /*g_object_ref (GTK_WIDGET (iter->data));*/
      gtk_container_remove (
        container,
        GTK_WIDGET (iter->data));
    }
  g_list_free (children);
}

/**
 * Returns the primary or secondary label of the
 * given GtkMessageDialog.
 *
 * @param 0 for primary, 1 for secondary.
 */
GtkLabel *
z_gtk_message_dialog_get_label (
  GtkMessageDialog * self,
  const int          secondary)
{
  GList *children, *iter;

  GtkWidget * box =
    gtk_message_dialog_get_message_area (self);

  children =
    gtk_container_get_children (
      GTK_CONTAINER (box));
  GtkLabel * label = NULL;
  for (iter = children;
       iter != NULL;
       iter = g_list_next (iter))
    {
      label =
        GTK_LABEL (iter->data);
      const char * name =
      gtk_widget_class_get_css_name (
        GTK_WIDGET_GET_CLASS (label));
      if (string_is_equal (name, "label") &&
          !secondary)
        {
          break;
        }
      else if (
        string_is_equal (name, "secondary_label") &&
        secondary)
        {
          break;
        }
    }
  g_list_free (children);

  return label;
}

void
z_gtk_overlay_add_if_not_exists (
  GtkOverlay * overlay,
  GtkWidget *  widget)
{
  GList *children, *iter;

  children =
    gtk_container_get_children (
      GTK_CONTAINER (overlay));
  for (iter = children;
       iter != NULL;
       iter = g_list_next (iter))
    {
      if (iter->data == widget)
        {
          g_message ("exists");
          g_list_free (children);
          return;
        }
    }
  g_list_free (children);

  g_message ("not exists, adding");
  gtk_overlay_add_overlay (overlay, widget);
}

void
z_gtk_container_destroy_all_children (
  GtkContainer * container)
{
  GList *children, *iter;

  children =
    gtk_container_get_children (GTK_CONTAINER (container));
  for (iter = children;
       iter != NULL;
       iter = g_list_next (iter))
    gtk_widget_destroy (GTK_WIDGET (iter->data));
  g_list_free (children);
}

void
z_gtk_container_remove_children_of_type (
  GtkContainer * container,
  GType          type)
{
  GList *children, *iter;

  children =
    gtk_container_get_children (GTK_CONTAINER (container));
  for (iter = children;
       iter != NULL;
       iter = g_list_next (iter))
    {
      GtkWidget * widget = iter->data;
      if (G_TYPE_CHECK_INSTANCE_TYPE (
            widget, type))
        {
          /*g_object_ref (widget);*/
          gtk_container_remove (
            container,
            widget);
        }
    }
  g_list_free (children);
}

void
z_gtk_tree_view_remove_all_columns (
  GtkTreeView * treeview)
{
  g_return_if_fail (
    treeview && GTK_IS_TREE_VIEW (treeview));

  GtkTreeViewColumn *column;
  GList * list, * iter;
  list =
    gtk_tree_view_get_columns (treeview);
  for (iter = list; iter != NULL;
       iter = g_list_next (iter))
    {
      column =
        GTK_TREE_VIEW_COLUMN (iter->data);
      gtk_tree_view_remove_column (
        treeview, column);
    }
  g_list_free (list);
}

/**
 * Configures a simple value-text combo box using
 * the given model.
 */
void
z_gtk_configure_simple_combo_box (
  GtkComboBox * cb,
  GtkTreeModel * model)
{
  enum
  {
    VALUE_COL,
    TEXT_COL,
    ID_COL,
  };

  GtkCellRenderer *renderer;
  gtk_combo_box_set_model (
    cb, model);
  gtk_combo_box_set_id_column (
    cb, ID_COL);
  gtk_cell_layout_clear (
    GTK_CELL_LAYOUT (cb));
  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (
    GTK_CELL_LAYOUT (cb),
    renderer, TRUE);
  gtk_cell_layout_set_attributes (
    GTK_CELL_LAYOUT (cb), renderer,
    "text", TEXT_COL,
    NULL);
}

void
z_gtk_button_set_icon_name (
  GtkButton * btn,
  const char * name)
{
  GtkWidget * img =
    gtk_image_new_from_icon_name (
      name,
      GTK_ICON_SIZE_BUTTON);
  gtk_widget_set_visible (img, 1);
  z_gtk_container_remove_all_children (
    GTK_CONTAINER (btn));
  gtk_container_add (GTK_CONTAINER (btn), img);
}

/**
 * Sets the given emblem to the button, or unsets
 * the emblem if \ref emblem_icon is NULL.
 */
void
z_gtk_button_set_emblem (
  GtkButton *  btn,
  const char * emblem_icon_name)
{
  GtkWidget * btn_child =
    gtk_bin_get_child (GTK_BIN (btn));
  GtkImage * prev_img = NULL;
  if (GTK_IS_BIN (btn_child))
    {
      GtkWidget * inner_child =
        gtk_bin_get_child (
          GTK_BIN (btn_child));
      if (GTK_IS_CONTAINER (inner_child))
        {
          prev_img =
            GTK_IMAGE (
              z_gtk_container_get_single_child (
                GTK_CONTAINER (inner_child)));
        }
      else if (GTK_IS_IMAGE (inner_child))
        {
          prev_img = GTK_IMAGE (inner_child);
        }
      else
        {
          g_critical (
            "unknown type %s",
            G_OBJECT_TYPE_NAME (inner_child));
        }
    }
  else if (GTK_IS_IMAGE (btn_child))
    {
      prev_img = GTK_IMAGE (btn_child);
    }
  else if (GTK_IS_CONTAINER (btn_child))
    {
      prev_img =
        GTK_IMAGE (
          z_gtk_container_get_single_child (
            GTK_CONTAINER (btn_child)));
    }
  else
    {
      g_return_if_reached ();
    }

  GtkIconSize icon_size;
  const char * icon_name = NULL;
  GtkImageType image_type =
    gtk_image_get_storage_type (prev_img);
  if (image_type == GTK_IMAGE_ICON_NAME)
    {
      gtk_image_get_icon_name (
        prev_img, &icon_name, &icon_size);
    }
  else if (image_type == GTK_IMAGE_GICON)
    {
      GIcon * emblemed_icon = NULL;
      gtk_image_get_gicon (
        prev_img, &emblemed_icon, &icon_size);
      g_return_if_fail (emblemed_icon);
      GIcon * prev_icon = NULL;
      if (G_IS_EMBLEMED_ICON (emblemed_icon))
        {
          prev_icon =
            g_emblemed_icon_get_icon (
              G_EMBLEMED_ICON (emblemed_icon));
        }
      else if (G_IS_THEMED_ICON (emblemed_icon))
        {
          prev_icon = emblemed_icon;
        }
      else
        {
          g_return_if_reached ();
        }

      const char * const * icon_names =
        g_themed_icon_get_names (
          G_THEMED_ICON (prev_icon));
      icon_name =  icon_names[0];
    }
  else
    {
      g_return_if_reached ();
    }

  GIcon * icon = g_themed_icon_new (icon_name);

  if (emblem_icon_name)
    {
      GIcon * dot_icon =
        g_themed_icon_new ("media-record");
      GEmblem * emblem =
        g_emblem_new (dot_icon);
      icon =
        g_emblemed_icon_new (icon, emblem);
    }

  /* set new icon */
  GtkWidget * img =
    gtk_image_new_from_gicon (
      icon, icon_size);
  gtk_widget_set_visible (img, true);
  gtk_button_set_image (btn, img);
}

/**
 * Creates a button with the given icon name.
 */
GtkButton *
z_gtk_button_new_with_icon (const char * name)
{
  GtkButton * btn = GTK_BUTTON (gtk_button_new ());
  z_gtk_button_set_icon_name (btn,
                              name);
  gtk_widget_set_visible (GTK_WIDGET (btn),
                          1);
  return btn;
}

/**
 * Creates a toggle button with the given icon name.
 */
GtkToggleButton *
z_gtk_toggle_button_new_with_icon (const char * name)
{
  GtkToggleButton * btn =
    GTK_TOGGLE_BUTTON (gtk_toggle_button_new ());
  z_gtk_button_set_icon_name (GTK_BUTTON (btn),
                              name);
  gtk_widget_set_visible (GTK_WIDGET (btn),
                          1);
  return btn;
}

/**
 * Creates a button with the given resource name as icon.
 */
GtkButton *
z_gtk_button_new_with_resource (IconType  icon_type,
                                const char * name)
{
  GtkButton * btn = GTK_BUTTON (gtk_button_new ());
  resources_add_icon_to_button (btn,
                                icon_type,
                                name);
  gtk_widget_set_visible (GTK_WIDGET (btn),
                          1);
  return btn;
}

/**
 * Creates a toggle button with the given resource name as
 * icon.
 */
GtkToggleButton *
z_gtk_toggle_button_new_with_resource (
  IconType  icon_type,
  const char * name)
{
  GtkToggleButton * btn =
    GTK_TOGGLE_BUTTON (gtk_toggle_button_new ());
  resources_add_icon_to_button (GTK_BUTTON (btn),
                                icon_type,
                                name);
  gtk_widget_set_visible (GTK_WIDGET (btn),
                          1);
  return btn;
}

/**
 * Creates a menu item.
 */
GtkMenuItem *
z_gtk_create_menu_item_full (
  const gchar *   label_name,
  const gchar *   icon_name,
  IconType        resource_icon_type,
  const gchar *   resource,
  bool            is_toggle,
  const char *    action_name)
{
  GtkWidget *box =
    gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  GtkWidget *icon = NULL;
  if (icon_name)
    {
      icon =
        gtk_image_new_from_icon_name (
          icon_name, GTK_ICON_SIZE_MENU);
    }
  else if (resource)
    {
      icon =
        resources_get_icon (
          resource_icon_type, resource);
    }
  GtkWidget * label =
    gtk_accel_label_new (label_name);
  GtkWidget * menu_item;
  if (is_toggle)
    menu_item = gtk_check_menu_item_new ();
  else
    menu_item = gtk_menu_item_new ();

  if (icon)
    gtk_container_add (GTK_CONTAINER (box), icon);

  gtk_label_set_use_underline (
    GTK_LABEL (label), TRUE);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);

  if (action_name)
    {
      gtk_actionable_set_action_name (
        GTK_ACTIONABLE (menu_item),
        action_name);
      accel_set_accel_label_from_action (
        GTK_ACCEL_LABEL (label),
        action_name);
    }

  gtk_box_pack_end (
    GTK_BOX (box), label, TRUE, TRUE, 0);

  gtk_container_add (GTK_CONTAINER (menu_item), box);

  gtk_widget_show_all (menu_item);

  return GTK_MENU_ITEM (menu_item);
}

/**
 * Returns a pointer stored at the given selection.
 */
void *
z_gtk_get_single_selection_pointer (
  GtkTreeView * tv,
  int           column)
{
  GtkTreeSelection * ts =
    gtk_tree_view_get_selection (tv);
  GtkTreeModel * model =
    gtk_tree_view_get_model (tv);
  GList * selected_rows =
    gtk_tree_selection_get_selected_rows (
      ts, NULL);
  GtkTreePath * tp =
    (GtkTreePath *)
    g_list_first (selected_rows)->data;
  GtkTreeIter iter;
  gtk_tree_model_get_iter (
    model, &iter, tp);
  GValue value = G_VALUE_INIT;
  gtk_tree_model_get_value (
    model, &iter, column, &value);
  return g_value_get_pointer (&value);
}

/**
 * Returns the label from a given GtkMenuItem.
 *
 * The menu item must have a box with an optional
 * icon and a label inside.
 */
GtkLabel *
z_gtk_get_label_from_menu_item (
  GtkMenuItem * mi)
{
  GList *children, *iter;

  /* get box */
  GtkWidget * box = NULL;
  children =
    gtk_container_get_children (
      GTK_CONTAINER (mi));
  for (iter = children;
       iter != NULL;
       iter = g_list_next (iter))
    {
      if (GTK_IS_BOX (iter->data))
        {
          box = iter->data;
          g_list_free (children);
          break;
        }
    }
  g_warn_if_fail (box);

  children =
    gtk_container_get_children (
      GTK_CONTAINER (box));
  for (iter = children;
       iter != NULL;
       iter = g_list_next (iter))
    {
      if (GTK_IS_LABEL (iter->data))
        {
          GtkLabel * label = GTK_LABEL (iter->data);
          g_list_free (children);
          return label;
        }
    }

  g_warn_if_reached ();
  g_list_free (children);

  return NULL;
}

/**
 * Sets the tooltip and finds the accel keys and
 * appends them to the tooltip in small text.
 */
void
z_gtk_set_tooltip_for_actionable (
  GtkActionable * actionable,
  const char *    tooltip)
{
  char * accel, * tt;
  accel =
    accel_get_primary_accel_for_action (
      gtk_actionable_get_action_name (
        actionable));
  if (accel)
    tt =
      g_strdup_printf (
        "%s <span size=\"x-small\" foreground=\"#F79616\">%s</span>",
        tooltip, accel);
  else
    tt = g_strdup (tooltip);
  gtk_widget_set_tooltip_markup (
    GTK_WIDGET (actionable),
    tt);
  g_free (accel);
  g_free (tt);
}

/**
 * Changes the size of the icon inside tool buttons.
 */
void
z_gtk_tool_button_set_icon_size (
  GtkToolButton * toolbutton,
  GtkIconSize     icon_size)
{
  GtkImage * img =
    GTK_IMAGE (
      z_gtk_container_get_single_child (
        GTK_CONTAINER (
          z_gtk_container_get_single_child (
            GTK_CONTAINER (
              z_gtk_container_get_single_child (
                GTK_CONTAINER (toolbutton)))))));
  GtkImageType type =
    gtk_image_get_storage_type (GTK_IMAGE (img));
  g_return_if_fail (type == GTK_IMAGE_ICON_NAME);
  const char * _icon_name;
  gtk_image_get_icon_name (
    GTK_IMAGE (img), &_icon_name, NULL);
  char * icon_name = g_strdup (_icon_name);
  gtk_image_set_from_icon_name (
    GTK_IMAGE (img), icon_name,
    GTK_ICON_SIZE_SMALL_TOOLBAR);
  g_free (icon_name);
}

/**
 * Sets the ellipsize mode of each text cell
 * renderer in the combo box.
 */
void
z_gtk_combo_box_set_ellipsize_mode (
  GtkComboBox * self,
  PangoEllipsizeMode ellipsize)
{
  GList *children, *iter;
  children =
    gtk_cell_layout_get_cells  (
      GTK_CELL_LAYOUT (self));
  for (iter = children;
       iter != NULL;
       iter = g_list_next (iter))
    {
      if (!GTK_IS_CELL_RENDERER_TEXT (iter->data))
        continue;

      g_object_set (
        iter->data, "ellipsize", ellipsize,
        NULL);
    }
  g_list_free (children);
}

/**
 * Adds the given style class to the widget.
 */
void
z_gtk_widget_add_style_class (
  GtkWidget   *widget,
  const gchar *class_name)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (class_name != NULL);

  gtk_style_context_add_class (
    gtk_widget_get_style_context (widget),
    class_name);
}

/**
 * Removes the given style class from the widget.
 */
void
z_gtk_widget_remove_style_class (
  GtkWidget   *widget,
  const gchar *class_name)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (class_name != NULL);

  gtk_style_context_remove_class (
    gtk_widget_get_style_context (widget),
    class_name);
}

/**
 * Returns the nth child of a container.
 */
GtkWidget *
z_gtk_container_get_nth_child (
  GtkContainer * container,
  int            index)
{
  GList *children, *iter;
  children =
    gtk_container_get_children (container);
  int i = 0;
  for (iter = children;
       iter != NULL;
       iter = g_list_next (iter))
    {
      GtkWidget * widget =
        GTK_WIDGET (iter->data);
      if (i++ == index)
        {
          g_list_free (children);
          return widget;
        }
    }
  g_list_free (children);
  g_return_val_if_reached (NULL);
}

/**
 * Sets the margin on all 4 sides on the widget.
 */
void
z_gtk_widget_set_margin (
  GtkWidget * widget,
  int         margin)
{
  gtk_widget_set_margin_start (widget, margin);
  gtk_widget_set_margin_end (widget, margin);
  gtk_widget_set_margin_top (widget, margin);
  gtk_widget_set_margin_bottom (widget, margin);
}

GtkFlowBoxChild *
z_gtk_flow_box_get_selected_child (
  GtkFlowBox * self)
{
  GList * list =
    gtk_flow_box_get_selected_children (self);
  GtkFlowBoxChild * sel_child = NULL;
  for (GList * l = list; l != NULL; l = l->next)
    {
      sel_child = (GtkFlowBoxChild *) l->data;
      break;
    }
  g_list_free (list);

  return sel_child;
}

/**
 * Callback to use for simple directory links.
 */
bool
z_gtk_activate_dir_link_func (
  GtkLabel * label,
  char *     uri,
  void *     data)
{
  io_open_directory (uri);

  return TRUE;
}
