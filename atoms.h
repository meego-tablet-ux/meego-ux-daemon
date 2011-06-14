/*
 * Copyright 2011 Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#ifndef _ATOMS_H_
#define _ATOMS_H_

typedef enum _AtomType {
    ATOM_MEEGO_ORIENTATION,
    ATOM_MEEGO_INHIBIT_SCREENSAVER,
    ATOM_MEEGO_TABLET_NOTIFY,
    ATOM_MEEGO_STACKING_LAYER,
    ATOM_MEEGOTOUCH_SKIP_ANIMATIONS,
    ATOM_NET_ACTIVE_WINDOW,
    ATOM_NET_CLIENT_LIST,
    ATOM_NET_CLOSE_WINDOW,
    ATOM_NET_WM_WINDOW_TYPE,
    ATOM_NET_WM_WINDOW_TYPE_NORMAL,
    ATOM_NET_WM_WINDOW_TYPE_DESKTOP,
    ATOM_NET_WM_WINDOW_TYPE_NOTIFICATION,
    ATOM_NET_WM_WINDOW_TYPE_DOCK,
    ATOM_NET_WM_STATE_SKIP_TASKBAR,
    ATOM_NET_WM_STATE,
    ATOM_NET_WM_PID,
    ATOM_NET_WM_ICON_GEOMETRY,
    ATOM_NET_WM_ICON_NAME,
    ATOM_Backlight,
    ATOM_BACKLIGHT,
    ATOM_UTF8_STRING,
    ATOM_WM_CHANGE_STATE,
    ATOM_COUNT
} AtomType;

void initAtoms ();
Atom getAtom (AtomType type);

#endif
