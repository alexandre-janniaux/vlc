/*****************************************************************************
 * qvlcframe.cpp : A few helpers
 *****************************************************************************
 * Copyright (C) 2006-2007 the VideoLAN team
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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

#include "qvlcframe.hpp"

#include <QStyle>
#include <QDesktopWidget>
#include <QKeyEvent>
#include <QScreen>

#include "qt.hpp"
#include "main_interface.hpp"

void QVLCTools::saveWidgetPosition(QSettings *settings, QWidget *widget)
{
    settings->setValue("geometry", widget->saveGeometry());
}

void QVLCTools::saveWidgetPosition(intf_thread_t *p_intf,
                                   const QString& configName,
                                   QWidget *widget)
{
    getSettings()->beginGroup(configName);
    QVLCTools::saveWidgetPosition(getSettings(), widget);
    getSettings()->endGroup();
}

bool QVLCTools::restoreWidgetPosition(QSettings *settings,
                                      QWidget *widget, QScreen *defScreen,
                                      QSize defSize, QPoint defPos)
{
    if (defScreen == nullptr)
        defScreen = QGuiApplication::primaryScreen();

    if (!widget->restoreGeometry(settings->value("geometry").toByteArray()))
    {
        widget->move(defPos);
        widget->resize(defSize);

        if (defPos.x() == 0 && defPos.y() ==0 && defScreen)
        {
            auto rect = QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter,
                                            widget->size(),
                                            defScreen->availableGeometry());
            widget->setGeometry(rect);
        }

        return true;
    }
    return false;
}

bool QVLCTools::restoreWidgetPosition(intf_thread_t *p_intf,
                                      const QString& configName,
                                      QWidget *widget, QSize defSize,
                                      QPoint defPos)
{
    MainInterface *p_mi = p_intf->p_sys->p_mi;
    QScreen *screen = p_mi->windowHandle()->screen();

    return restoreWidgetPosition(p_intf, configName, widget,
                                 screen, defSize, defPos);
}


bool QVLCTools::restoreWidgetPosition(intf_thread_t *p_intf,
                                     const QString& configName,
                                     QWidget *widget, QScreen *defScreen,
                                     QSize defSize, QPoint defPos)
{
    getSettings()->beginGroup(configName);
    bool defaultUsed =
        QVLCTools::restoreWidgetPosition(getSettings(), widget, defScreen,
                                         defSize, defPos);
    getSettings()->endGroup();

    return defaultUsed;
}

void QVLCFrame::keyPressEvent(QKeyEvent *keyEvent)
{
    if (keyEvent->key() == Qt::Key_Escape)
    {
        this->cancel();
    }
    else if (keyEvent->key() == Qt::Key_Return ||
             keyEvent->key() == Qt::Key_Enter)
    {
        this->close();
    }
}

void QVLCDialog::keyPressEvent(QKeyEvent *keyEvent)
{
    if (keyEvent->key() == Qt::Key_Escape)
    {
        this->cancel();
    }
    else if (keyEvent->key() == Qt::Key_Return ||
             keyEvent->key() == Qt::Key_Enter)
    {
        this->close();
    }
}
