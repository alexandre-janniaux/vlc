/*****************************************************************************
 * cmd_input.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
 *          Olivier Teulière <ipkiss@via.ecp.fr>
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

#include "cmd_input.hpp"
#include "cmd_dialogs.hpp"
#include <vlc_input.h>
#include <vlc_playlist.h>
#include <vlc_player.h>

void CmdPlay::execute()
{
    auto *playlist = getPL();
    auto *player = vlc_playlist_GetPlayer(playlist);

    vlc_playlist_Lock(playlist);

    if (vlc_playlist_GetCurrentIndex(playlist) != -1)
    {
        // if already playing an input, reset rate to normal speed
        vlc_player_ChangeRate(player, 1.f);
    }

    if (vlc_playlist_Count(playlist) > 0)
    {
        vlc_playlist_Start(playlist);
    }
    else
    {
        // If the playlist is empty, open a file requester instead
        CmdDlgFile( getIntf() ).execute();
    }

    vlc_playlist_Unlock(playlist);
}


void CmdPause::execute()
{
    auto *playlist = getPL();

    vlc_playlist_Lock(playlist);
    vlc_playlist_Pause(playlist);
    vlc_playlist_Unlock(playlist);
}


void CmdStop::execute()
{
    auto *playlist = getPL();

    vlc_playlist_Lock(playlist);
    vlc_playlist_Stop(playlist);
    vlc_playlist_Unlock(playlist);

}


void CmdSlower::execute()
{
    auto *playlist = getPL();
    auto *player = vlc_playlist_GetPlayer(playlist);

    vlc_playlist_Lock(playlist);
    // TODO: how to call rate-slower ?
    //vlc_player_ChangeRate(playlist);
    vlc_playlist_Unlock(playlist);
    //var_TriggerCallback( getPL(), "rate-slower" );
}


void CmdFaster::execute()
{
    auto *playlist = getPL();

    // TODO: how to call rate-faster ?
    vlc_playlist_Lock(playlist);
    vlc_playlist_Unlock(playlist);
    //var_TriggerCallback( getPL(), "rate-faster" );
}


void CmdMute::execute()
{
    auto *playlist = getPL();
    auto *player = vlc_playlist_GetPlayer(playlist);

    vlc_player_Lock(player);
    vlc_player_aout_Mute(player, !vlc_player_aout_IsMuted(player));
    vlc_player_Unlock(player);
}


void CmdVolumeUp::execute()
{
    auto *playlist = getPL();
    auto *player = vlc_playlist_GetPlayer(playlist);

    vlc_player_Lock(player);
    auto volume = vlc_player_aout_GetVolume(player);
    // TODO: increase volume
    vlc_player_aout_SetVolume(player, volume);
    vlc_player_Unlock(player);
}


void CmdVolumeDown::execute()
{
    auto *playlist = getPL();
    auto *player = vlc_playlist_GetPlayer(playlist);

    vlc_player_Lock(player);
    auto volume = vlc_player_aout_GetVolume(player);
    // TODO: decrease volume
    vlc_player_aout_SetVolume(player, volume);
    vlc_player_Unlock(player);
}
