/*****************************************************************************
 * qvoutwindow.cpp: TODO
 ****************************************************************************
 * Copyright (C) 2019 VideoLAN TODO:
 *
 * Authors: Jean-Baptiste Kempf <jb (at) videolan.org>
 *          Jean-Philippe André <jpeg (at) videolan.org>
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
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "qt.hpp"

#include "qvoutwindow.hpp"
#include "util/customwidgets.hpp" //for qtEventToVLCKey

QVoutWindow::QVoutWindow(QObject* parent)
    : QObject(parent)
{
}

QVoutWindow::~QVoutWindow()
{
}

bool QVoutWindow::setupVoutWindow(vout_window_t* window)
{
    QMutexLocker lock(&m_voutlock);
    m_voutWindow = window;
    m_hasVideo = false;
    return true;
}

void QVoutWindow::enableVideo(unsigned /*width*/, unsigned /*height*/, bool /*fullscreen*/)
{
    QMutexLocker lock(&m_voutlock);
    if (m_voutWindow) {
        m_hasVideo = true;
        if (m_surfaceSize.isValid())
            vout_window_ReportSize(m_voutWindow, m_surfaceSize.width(), m_surfaceSize.height());
    }
}

void QVoutWindow::disableVideo()
{
    QMutexLocker lock(&m_voutlock);
    if (m_voutWindow)
        m_hasVideo = false;
}

void QVoutWindow::windowClosed()
{
    QMutexLocker lock(&m_voutlock);
    if (m_voutWindow)
        vout_window_ReportClose(m_voutWindow);
}

void QVoutWindow::onMousePressed(int vlcButton)
{
    QMutexLocker lock(&m_voutlock);
    if (m_hasVideo)
        vout_window_ReportMousePressed(m_voutWindow, vlcButton);
}

void QVoutWindow::onMouseReleased(int vlcButton)
{
    QMutexLocker lock(&m_voutlock);
    if (m_hasVideo)
        vout_window_ReportMouseReleased(m_voutWindow, vlcButton);
}

void QVoutWindow::onMouseDoubleClick(int vlcButton)
{
    QMutexLocker lock(&m_voutlock);
    if (m_hasVideo)
        vout_window_ReportMouseDoubleClick(m_voutWindow, vlcButton);
}

void QVoutWindow::onMouseMoved(float x, float y)
{
    QMutexLocker lock(&m_voutlock);
    if (m_hasVideo)
        vout_window_ReportMouseMoved(m_voutWindow, x, y);
}

void QVoutWindow::onMouseWheeled(const QPointF& pos, int delta, Qt::MouseButtons buttons,  Qt::KeyboardModifiers modifiers, Qt::Orientation orient)
{
    QWheelEvent event(pos, delta, buttons, modifiers, orient);
    int vlckey = qtWheelEventToVLCKey(&event);
    QMutexLocker lock(&m_voutlock);
    if (m_hasVideo)
        vout_window_ReportKeyPress(m_voutWindow, vlckey);
}

void QVoutWindow::onKeyPressed(int key, Qt::KeyboardModifiers modifiers)
{
    QKeyEvent event(QEvent::KeyPress, key, modifiers);
    int vlckey = qtEventToVLCKey(&event);
    QMutexLocker lock(&m_voutlock);
    if (m_hasVideo)
        vout_window_ReportKeyPress(m_voutWindow, vlckey);

}

void QVoutWindow::onSurfaceSizeChanged(QSizeF size)
{
    m_surfaceSize = size;
    QMutexLocker lock(&m_voutlock);
    if (m_hasVideo)
        vout_window_ReportSize(m_voutWindow, size.width(), size.height());
}
