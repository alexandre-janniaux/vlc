/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef AUDIODEVICEMODEL_H
#define AUDIODEVICEMODEL_H

#include <QAbstractListModel>

#include "qt.hpp"
#include <QObject>
#include <QStringList>
#include <vlc_aout.h>
#include "components/player_controller.hpp"

class AudioDeviceModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit AudioDeviceModel(intf_thread_t *p_intf, QObject *parent = nullptr);

    // Basic functionality:
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
        
    QHash<int, QByteArray> roleNames() const override;

private:
    int i_inputs;
    char **names;
    char **ids;

};

#endif // AUDIODEVICEMODEL_H


