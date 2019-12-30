/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2009 Carlos Garnacho <carlosg@gnome.org>
 * Copyright (C) 2015 Red Hat
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */
#include "gdk-private.h"
#include <gdk/gdkwayland.h>

typedef struct _GdkWaylandTouchData GdkWaylandTouchData;
typedef struct _GdkWaylandPointerFrameData GdkWaylandPointerFrameData;
typedef struct _GdkWaylandPointerData GdkWaylandPointerData;
typedef struct _GdkWaylandTabletData GdkWaylandTabletData;
typedef struct _GdkWaylandSeat GdkWaylandSeat;
#define GDK_WAYLAND_SEAT(seat) ((GdkWaylandSeat*)(void*) (seat));

struct _GdkWaylandTouchData
{
  uint32_t id;
  gdouble x;
  gdouble y;
  GdkWindow *window;
  uint32_t touch_down_serial;
  guint initial_touch : 1;
};

struct _GdkWaylandPointerFrameData
{
  GdkEvent *event;

  /* Specific to the scroll event */
  gdouble delta_x, delta_y;
  int32_t discrete_x, discrete_y;
  gint8 is_scroll_stop;
  enum wl_pointer_axis_source source;
};

struct _GdkWaylandPointerData {
  GdkWindow *focus;

  double surface_x, surface_y;

  GdkModifierType button_modifiers;

  uint32_t time;
  uint32_t enter_serial;
  uint32_t press_serial;

  GdkWindow *grab_window;
  uint32_t grab_time;

  struct wl_surface *pointer_surface;
  GdkCursor *cursor;
  guint cursor_timeout_id;
  guint cursor_image_index;
  guint cursor_image_delay;

  guint current_output_scale;
  GSList *pointer_surface_outputs;

  /* Accumulated event data for a pointer frame */
  GdkWaylandPointerFrameData frame;
};

struct _GdkWaylandTabletData
{
  struct zwp_tablet_v2 *wp_tablet;
  gchar *name;
  gchar *path;
  uint32_t vid;
  uint32_t pid;

  GdkDevice *master;
  GdkDevice *stylus_device;
  GdkDevice *eraser_device;
  GdkDevice *current_device;
  GdkSeat *seat;
  GdkWaylandPointerData pointer_info;

  // GList *pads;
  //
  // GdkWaylandTabletToolData *current_tool;
  //
  // gint axis_indices[GDK_AXIS_LAST];
  // gdouble *axes;
};

struct _GdkWaylandSeat
{
  GdkSeat parent_instance;

  guint32 id;
  struct wl_seat *wl_seat;
  struct wl_pointer *wl_pointer;
  struct wl_keyboard *wl_keyboard;
  struct wl_touch *wl_touch;
  struct zwp_pointer_gesture_swipe_v1 *wp_pointer_gesture_swipe;
  struct zwp_pointer_gesture_pinch_v1 *wp_pointer_gesture_pinch;
  struct zwp_tablet_seat_v2 *wp_tablet_seat;

  GdkDisplay *display;
  GdkDeviceManager *device_manager;

  GdkDevice *master_pointer;
  GdkDevice *master_keyboard;
  GdkDevice *pointer;
  GdkDevice *wheel_scrolling;
  GdkDevice *finger_scrolling;
  GdkDevice *continuous_scrolling;
  GdkDevice *keyboard;
  GdkDevice *touch_master;
  GdkDevice *touch;
  GdkCursor *cursor;
  GdkKeymap *keymap;

  GHashTable *touches;
  GList *tablets;
  GList *tablet_tools;
  GList *tablet_pads;

  GdkWaylandPointerData pointer_info;
  GdkWaylandPointerData touch_info;

  GdkModifierType key_modifiers;
  GdkWindow *keyboard_focus;
  GdkAtom pending_selection;
  GdkWindow *grab_window;
  uint32_t grab_time;
  gboolean have_server_repeat;
  uint32_t server_repeat_rate;
  uint32_t server_repeat_delay;

  struct wl_callback *repeat_callback;
  guint32 repeat_timer;
  guint32 repeat_key;
  guint32 repeat_count;
  gint64 repeat_deadline;
  GSettings *keyboard_settings;
  uint32_t keyboard_time;
  uint32_t keyboard_key_serial;

  struct gtk_primary_selection_device *primary_data_device;
  struct wl_data_device *data_device;
  GdkDragContext *drop_context;

  /* Source/dest for non-local dnd */
  GdkWindow *foreign_dnd_window;

  /* Some tracking on gesture events */
  guint gesture_n_fingers;
  gdouble gesture_scale;

  GdkCursor *grab_cursor;
};


uint32_t
_gdk_wayland_seat_get_last_implicit_grab_serial (GdkSeat           *seat,
                                                 GdkEventSequence **sequence)
{
  GdkWaylandSeat *wayland_seat;
  GdkWaylandTouchData *touch;
  GHashTableIter iter;
  GList *l;
  uint32_t serial;

  wayland_seat = GDK_WAYLAND_SEAT (seat);
  g_hash_table_iter_init (&iter, wayland_seat->touches);

  if (sequence)
     *sequence = NULL;

  serial = wayland_seat->keyboard_key_serial;

  if (wayland_seat->pointer_info.press_serial > serial)
    serial = wayland_seat->pointer_info.press_serial;

  for (l = wayland_seat->tablets; l; l = l->next)
    {
      GdkWaylandTabletData *tablet = l->data;

      if (tablet->pointer_info.press_serial > serial)
        serial = tablet->pointer_info.press_serial;
    }

  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &touch))
    {
      if (touch->touch_down_serial > serial)
        {
          // if (sequence)
          //  *sequence = GDK_SLOT_TO_EVENT_SEQUENCE (touch->id);
          serial = touch->touch_down_serial;
        }
    }

  return serial;
}
