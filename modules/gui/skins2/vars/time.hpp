/*****************************************************************************
 * time.hpp
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef TIME_HPP
#define TIME_HPP

#include "../utils/var_percent.hpp"
#include <string>

struct vlc_player;

/// Variable for VLC stream time
class StreamTime: public VarPercent
{
public:
    StreamTime( vlc_player_t *player, intf_thread_t *pIntf )
        : VarPercent( pIntf ), m_player( player ) { }

    virtual ~StreamTime() { }

    virtual void set( float percentage, vlc_tick_t time, bool updateVLC );

    virtual void set( float percentage ) { set( percentage, 0, true ); }

    void set_duration( vlc_tick_t duration ) { m_duration = duration; }

    /// Return a string containing a value from 0 to 100
    virtual std::string getAsStringPercent() const;
    /// Return the current time formatted as a time display (h:mm:ss)
    virtual std::string getAsStringCurrTime( bool bShortFormat = false ) const;
    /// Return the time left formatted as a time display (h:mm:ss)
    virtual std::string getAsStringTimeLeft( bool bShortFormat = false ) const;
    /// Return the duration formatted as a time display (h:mm:ss)
    virtual std::string getAsStringDuration( bool bShortFormat = false ) const;

private:
    /// Convert a number of seconds into "h:mm:ss" format
    std::string formatTime( int seconds, bool bShortFormat ) const;
    /// Return true when there is a non-null input and its position is not 0.0.
    bool havePosition() const;

    vlc_player_t *m_player;
    vlc_tick_t m_time = 0;
    vlc_tick_t m_duration = 0;
};

#endif
