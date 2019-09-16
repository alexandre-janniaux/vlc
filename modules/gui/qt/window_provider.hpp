/*****************************************************************************
 * window_provider.hpp : Link between the video window (vout_window) and Qt
 ****************************************************************************
 * Copyright (C) 2019 VideoLabs
 *
 * Authors: Alexandre Janniaux <ajanni@videolabs.io>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef QVLC_WINDOW_PROVIDER_HPP
#define QVLC_WINDOW_PROVIDER_HPP

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <QtCore/QObject>
#include <vlc_vout_window.h>

class MainInterface;
struct vlc_object_t;
struct module_t;

class WaylandWindowProvider : public QObject
{
    Q_OBJECT

public:
    WaylandWindowProvider(vlc_object_t *obj, MainInterface *intf);

    virtual ~WaylandWindowProvider();

    static vout_window_t *
    GetWindow(vlc_window_provider_t *opaque_provider,
              vlc_object_t *parent);

    vlc_window_provider_t *GetProvider();

public slots:
    void Resize(unsigned width, unsigned height);

private:
    vlc_window_provider_t provider;
    vout_window_t* parent_window;
    vout_window_t* window;

    MainInterface *intf;
    vlc_object_t *obj;

    module_t *module = nullptr;

    unsigned width, height;
};

#endif
