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
#include <vlc_playlist.h>
#include <vlc_player.h>

void CmdDvdNextTitle::execute()
{
    auto *playlist = getPL();
    auto *player = vlc_playlist_GetPlayer(playlist);

    vlc_player_Lock(player);
    vlc_player_SelectNextTitle(player);
    vlc_player_Unlock(player);
}


void CmdDvdPreviousTitle::execute()
{
    auto *playlist = getPL();
    auto *player = vlc_playlist_GetPlayer(playlist);

    vlc_player_Lock(player);
    vlc_player_SelectPrevTitle(player);
    vlc_player_Unlock(player);
}


void CmdDvdNextChapter::execute()
{
    auto *playlist = getPL();
    auto *player = vlc_playlist_GetPlayer(playlist);

    vlc_player_Lock(player);
    vlc_player_SelectNextChapter(player);
    vlc_player_Unlock(player);
}


void CmdDvdPreviousChapter::execute()
{
    auto *playlist = getPL();
    auto *player = vlc_playlist_GetPlayer(playlist);

    vlc_player_Lock(player);
    vlc_player_SelectPrevChapter(player);
    vlc_player_Unlock(player);
}


void CmdDvdRootMenu::execute()
{
    auto *playlist = getPL();
    auto *player = vlc_playlist_GetPlayer(playlist);

    vlc_player_Lock(player);
    // TODO: is it how we go to rootmenu ?
    vlc_player_Navigate(player, VLC_PLAYER_NAV_MENU);
    vlc_player_Unlock(player);
}
