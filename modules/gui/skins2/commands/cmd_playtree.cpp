/*****************************************************************************
 * cmd_playtree.cpp
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 *
 * Authors: Antoine Cellerier <dionoea@videolan.org>
 *          Clément Stenac <zorglub@videolan.org>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "cmd_playtree.hpp"
#include <vlc_playlist.h>
#include "../src/vlcproc.hpp"
#include "../utils/var_bool.hpp"

void CmdPlaytreeDel::execute()
{
    m_rTree.delSelected();
}

void CmdPlaytreeSort::execute()
{
    /// \todo Choose sort method/order - Need more commands
    /// \todo Choose the correct view
    auto *playlist = getPL();

    vlc_playlist_Lock(playlist);
    vlc_playlist_sort_criterion criterion {};
    criterion.key = VLC_PLAYLIST_SORT_KEY_TITLE;
    criterion.order = VLC_PLAYLIST_SORT_ORDER_DESCENDING;

    vlc_playlist_Sort(playlist, &criterion, 1);
    // TODO
    //playlist_RecursiveNodeSort( p_playlist, &p_playlist->root,
    //                            SORT_TITLE, ORDER_NORMAL );
    vlc_playlist_Unlock(playlist);

    // Ask for rebuild
    VlcProc::instance( getIntf() )->getPlaytreeVar().onChange();
}
