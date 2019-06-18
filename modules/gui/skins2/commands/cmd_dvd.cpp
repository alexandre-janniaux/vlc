/*****************************************************************************
 * cmd_dvd.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 *
 * Authors: Olivier Teulière <ipkiss@via.ecp.fr>
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

#include "cmd_dvd.hpp"
#include <vlc_input.h>
#include <vlc_playlist_legacy.h>

void CmdDvdNextTitle::execute()
{
    auto *playlist = GetPL();

    vlc_playlist_Lock(playlist);
    auto index = vlc_playlist_GetCurrentIndex(playlist);
    auto *item = vlc_playlist_Get(playlist, index);
    auto *input = vlc_playlist_item_GetMedia(item);

    if (input)
        var_TriggerCallback(input, "next-title");

    vlc_playlist_Unlock(playlist);
}


void CmdDvdPreviousTitle::execute()
{
    auto *playlist = GetPL();

    vlc_playlist_Lock(playlist);
    auto index = vlc_playlist_GetCurrentIndex(playlist);
    auto *item = vlc_playlist_Get(playlist, index);
    auto *input = vlc_playlist_item_GetMedia(item);

    if (input)
        var_TriggerCallback(input, "prev-title");

    vlc_playlist_Unlock(playlist);
}


void CmdDvdNextChapter::execute()
{
    auto *playlist = GetPL();

    vlc_playlist_Lock(playlist);
    auto index = vlc_playlist_GetCurrentIndex(playlist);
    auto *item = vlc_playlist_Get(playlist, index);
    auto *input = vlc_playlist_item_GetMedia(item);

    if (input)
        var_TriggerCallback(input, "next-chapter");

    vlc_playlist_Unlock(playlist);
}


void CmdDvdPreviousChapter::execute()
{
    auto *playlist = GetPL();

    vlc_playlist_Lock(playlist);
    auto index = vlc_playlist_GetCurrentIndex(playlist);
    auto *item = vlc_playlist_Get(playlist, index);
    auto *input = vlc_playlist_item_GetMedia(item);

    if (input)
        var_TriggerCallback(input, "prev-chapter");

    vlc_playlist_Unlock(input);
}


void CmdDvdRootMenu::execute()
{
    auto *playlist = GetPL();

    vlc_playlist_Lock(playlist);
    auto index = vlc_playlist_GetCurrentIndex(playlist);
    auto *item = vlc_playlist_Get(playlist, index);
    auto *input = vlc_playlist_item_GetMedia(item);

    if (input)
        var_SetInteger(input, "title  0", 2);

    vlc_playlist_Unlock(playlist);
}
