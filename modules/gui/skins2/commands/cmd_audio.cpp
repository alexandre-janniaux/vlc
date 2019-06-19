/*****************************************************************************
 * cmd_audio.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
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

#include "cmd_audio.hpp"
#include "../src/vlcproc.hpp"
#include <vlc_playlist.h>
#include <vlc_player.h>
#include <vlc_input.h>
#include <string>

void CmdSetEqualizer::execute()
{
    auto *playlist = getPL();
    auto *player = vlc_playlist_GetPlayer(playlist);

    vlc_player_aout_EnableFilter(player, "equalizer", m_enable);
}
