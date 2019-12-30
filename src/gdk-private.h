#ifndef GDK_PRIVATE_H
#define GDK_PRIVATE_H
#include <gdk/gdk.h>

uint32_t
_gdk_wayland_seat_get_last_implicit_grab_serial (GdkSeat           *seat,
                                                 GdkEventSequence **sequence);
#endif // GDK_PRIVATE_H
