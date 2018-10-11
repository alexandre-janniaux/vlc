/*****************************************************************************
 * vlc-qt-static.hpp: Provide required Qt modules for static build
 ****************************************************************************
 * Copyright © 2006-2018 the VideoLAN team
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

#ifndef VLCQTSTATIC_HPP
#define VLCQTSTATIC_HPP

#ifdef QT_STATIC /* For static builds */
 #include <QtPlugin>

 #ifdef QT_STATICPLUGIN
  Q_IMPORT_PLUGIN(QSvgIconPlugin)
  Q_IMPORT_PLUGIN(QSvgPlugin)
  Q_IMPORT_PLUGIN(QJpegPlugin)
  Q_IMPORT_PLUGIN(QtQuick2Plugin)
  Q_IMPORT_PLUGIN(QtQuickControls2Plugin)
  Q_IMPORT_PLUGIN(QtQuickLayoutsPlugin)
  Q_IMPORT_PLUGIN(QtQuick2WindowPlugin)
  Q_IMPORT_PLUGIN(QtQuickTemplates2Plugin)
  Q_IMPORT_PLUGIN(QtQmlModelsPlugin)
  Q_IMPORT_PLUGIN(QtGraphicalEffectsPlugin)
  Q_IMPORT_PLUGIN(QtGraphicalEffectsPrivatePlugin)

  #if !HAS_QT56
   Q_IMPORT_PLUGIN(AccessibleFactory)
  #endif
  #ifdef _WIN32
   Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
   Q_IMPORT_PLUGIN(QWindowsVistaStylePlugin)
  #else
    #if defined(QT5_HAS_X11)
     Q_IMPORT_PLUGIN(QXcbIntegrationPlugin)
    #endif
    #if defined(QT5_HAS_WAYLAND)
     Q_IMPORT_PLUGIN(QWaylandEglPlatformIntegrationPlugin)
     Q_IMPORT_PLUGIN(QWaylandWlShellIntegrationPlugin)
    #endif
  #endif
 #endif
#endif

#endif // VLCQTSTATIC_HPP
