/*
 * Copyright 2011 Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#include <QtCore>
#include <QX11Info>
#include <cstdlib>

#include <stdio.h>

#include "atoms.h"

static Atom appAtoms[ATOM_COUNT];
// Keep this in sync with the AtomType enum in atoms.h
static const char *appAtomNames[ATOM_COUNT] = {
    "_MEEGO_ORIENTATION",
    "_MEEGO_INHIBIT_SCREENSAVER",
    "_MEEGO_TABLET_NOTIFY",
    "_MEEGO_STACKING_LAYER",
    "_MEEGOTOUCH_SKIP_ANIMATIONS",
    "_NET_ACTIVE_WINDOW",
    "_NET_CLIENT_LIST",
    "_NET_CLOSE_WINDOW",
    "_NET_WM_WINDOW_TYPE",
    "_NET_WM_WINDOW_TYPE_NORMAL",
    "_NET_WM_WINDOW_TYPE_DESKTOP",
    "_NET_WM_WINDOW_TYPE_NOTIFICATION",
    "_NET_WM_WINDOW_TYPE_DOCK",
    "_NET_WM_STATE_SKIP_TASKBAR",
    "_NET_WM_STATE",
    "_NET_WM_PID",
    "_NET_WM_ICON_GEOMETRY",
    "_NET_WM_ICON_NAME",
    "Backlight",
    "BACKLIGHT",
    "UTF8_STRING",
    "WM_CHANGE_STATE",
    "_MEEGO_SYSTEM_DIALOG"
};

void initAtoms ()
{
    Status status;
    Display *dpy = QX11Info::display ();

    status = XInternAtoms (dpy, (char **) appAtomNames, ATOM_COUNT, False, appAtoms);
    if (status == 0) 
    {
        fprintf (stderr, "Critical: Error getting atoms");
        abort ();
    }
}

Atom getAtom (AtomType type)
{
    return appAtoms[type];
}
