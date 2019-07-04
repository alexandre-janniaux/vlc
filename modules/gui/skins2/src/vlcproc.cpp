/*****************************************************************************
 * vlcproc.cpp
 *****************************************************************************
 * Copyright (C) 2003-2009 the VideoLAN team
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
 *          Olivier Teulière <ipkiss@via.ecp.fr>
 *          Erwan Tulou      <erwan10@videolan.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_aout.h>
#include <vlc_vout.h>
#include <vlc_playlist.h>
#include <vlc_player.h>
#include <vlc_url.h>
#include <vlc_strings.h>

#include "vlcproc.hpp"
#include "os_factory.hpp"
#include "os_loop.hpp"
#include "os_timer.hpp"
#include "var_manager.hpp"
#include "vout_manager.hpp"
#include "fsc_window.hpp"
#include "theme.hpp"
#include "window_manager.hpp"
#include "../commands/async_queue.hpp"
#include "../commands/cmd_change_skin.hpp"
#include "../commands/cmd_show_window.hpp"
#include "../commands/cmd_quit.hpp"
#include "../commands/cmd_resize.hpp"
#include "../commands/cmd_vars.hpp"
#include "../commands/cmd_dialogs.hpp"
#include "../commands/cmd_audio.hpp"
#include "../commands/cmd_callbacks.hpp"
#include "../utils/var_bool.hpp"
#include "../utils/var_string.hpp"
#include <sstream>

#include <assert.h>

VlcProc *VlcProc::instance( intf_thread_t *pIntf )
{
    if( pIntf->p_sys->p_vlcProc == NULL )
    {
        pIntf->p_sys->p_vlcProc = new VlcProc( pIntf );
    }

    return pIntf->p_sys->p_vlcProc;
}


void VlcProc::destroy( intf_thread_t *pIntf )
{
    delete pIntf->p_sys->p_vlcProc;
    pIntf->p_sys->p_vlcProc = NULL;
}

#define SET_BOOL(m,v)         ((VarBoolImpl*)(m).get())->set(v)
#define SET_STREAMTIME(m,v,b) ((StreamTime*)(m).get())->set(v,b)
#define SET_TEXT(m,v)         ((VarText*)(m).get())->set(v)
#define SET_STRING(m,v)       ((VarString*)(m).get())->set(v)
#define SET_VOLUME(m,v,b)     ((Volume*)(m).get())->setVolume(v,b)

void skins2_volume_changed(vlc_player_t *player, float volume, void *data)
{
    VlcProc *proc = static_cast<VlcProc *>(data);
}

static const vlc_player_cbs skins2_player_cbs =
{
    nullptr, /* on_current_media_changed */
    nullptr, /* on_state_changed */
    nullptr, /* on_error_changed */
    nullptr, /* on_buffering_changed */
    nullptr, /* on_rate_changed */
    nullptr,
    nullptr, /* on_position_changed */
    nullptr,
    nullptr, /* on_program_list_changed */
    nullptr,
    nullptr, /* on_titles_changed */
    nullptr,
    nullptr, /* on_chapter_selection_changed */
    nullptr,
    nullptr, /* on_teletext_enabled_changed */
    nullptr,
    nullptr, /* on_teletext_page_changed */
    nullptr,
    nullptr, /* on_teletext_transparency_changed */
    nullptr,
    nullptr, /* on_subtitle_delay_changed */
    nullptr,
    nullptr, /* on_renderer_changed */
    nullptr,
    nullptr, /* on_signal_changed */
    nullptr,
    nullptr, /* on_atobloop_changed */
    nullptr,
    nullptr, /* on_media_meta_changed */
    nullptr,
    nullptr, /* on_media_subitems_changed */
    nullptr,
    nullptr  /* on_cork_changed */
};

static const vlc_player_vout_cbs skins2_player_vout_cbs =
{
    nullptr, /* on_fullscreen_changed */
    nullptr  /* on_wallpaper_mode_changed */
};

static const vlc_player_aout_cbs skins2_player_aout_cbs =
{
    skins2_volume_changed, /* on_volume_changed */
    nullptr, /* on_mute_changed */
    nullptr  /* on_device_changed */
};

VlcProc::VlcProc( intf_thread_t *pIntf ): SkinObject( pIntf ),
    m_varEqBands( pIntf ), m_pVout( NULL )
{
    auto *playlist = vlc_intf_GetMainPlaylist(pIntf);
    auto *player = vlc_playlist_GetPlayer(playlist);

    vlc_player_Lock(player);
    vlc_player_AddListener(player, &skins2_player_cbs, this);
    vlc_player_aout_AddListener(player, &skins2_player_aout_cbs, this);
    vlc_player_vout_AddListener(player, &skins2_player_vout_cbs, this);
    vlc_player_Unlock(player);

    // Create and register VLC variables
    VarManager *pVarManager = VarManager::instance( getIntf() );

#define REGISTER_VAR( var, type, name ) \
    var = VariablePtr( new type( getIntf() ) ); \
    pVarManager->registerVar( var, name );
    REGISTER_VAR( m_cVarRandom, VarBoolImpl, "playlist.isRandom" )
    REGISTER_VAR( m_cVarLoop, VarBoolImpl, "playlist.isLoop" )
    REGISTER_VAR( m_cVarRepeat, VarBoolImpl, "playlist.isRepeat" )
    REGISTER_VAR( m_cPlaytree, Playtree, "playtree" )
    pVarManager->registerVar( getPlaytreeVar().getPositionVarPtr(),
                              "playtree.slider" );
    pVarManager->registerVar( m_cVarRandom, "playtree.isRandom" );
    pVarManager->registerVar( m_cVarLoop, "playtree.isLoop" );

    REGISTER_VAR( m_cVarPlaying, VarBoolImpl, "vlc.isPlaying" )
    REGISTER_VAR( m_cVarStopped, VarBoolImpl, "vlc.isStopped" )
    REGISTER_VAR( m_cVarPaused, VarBoolImpl, "vlc.isPaused" )

    /* Input variables */
    pVarManager->registerVar( m_cVarRepeat, "playtree.isRepeat" );
    REGISTER_VAR( m_cVarTime, StreamTime, "time" )
    REGISTER_VAR( m_cVarSeekable, VarBoolImpl, "vlc.isSeekable" )
    REGISTER_VAR( m_cVarDvdActive, VarBoolImpl, "dvd.isActive" )

    REGISTER_VAR( m_cVarRecordable, VarBoolImpl, "vlc.canRecord" )
    REGISTER_VAR( m_cVarRecording, VarBoolImpl, "vlc.isRecording" )

    /* Vout variables */
    REGISTER_VAR( m_cVarFullscreen, VarBoolImpl, "vlc.isFullscreen" )
    REGISTER_VAR( m_cVarHasVout, VarBoolImpl, "vlc.hasVout" )

    /* Aout variables */
    REGISTER_VAR( m_cVarHasAudio, VarBoolImpl, "vlc.hasAudio" )
    REGISTER_VAR( m_cVarVolume, Volume, "volume" )
    REGISTER_VAR( m_cVarMute, VarBoolImpl, "vlc.isMute" )
    REGISTER_VAR( m_cVarEqualizer, VarBoolImpl, "equalizer.isEnabled" )
    REGISTER_VAR( m_cVarEqPreamp, EqualizerPreamp, "equalizer.preamp" )

#undef REGISTER_VAR
    m_cVarSpeed = VariablePtr( new VarText( getIntf(), false ) );
    pVarManager->registerVar( m_cVarSpeed, "speed" );
    SET_TEXT( m_cVarSpeed, UString( getIntf(), "1") );
    m_cVarStreamName = VariablePtr( new VarText( getIntf(), false ) );
    pVarManager->registerVar( m_cVarStreamName, "streamName" );
    m_cVarStreamURI = VariablePtr( new VarText( getIntf(), false ) );
    pVarManager->registerVar( m_cVarStreamURI, "streamURI" );
    m_cVarStreamBitRate = VariablePtr( new VarText( getIntf(), false ) );
    pVarManager->registerVar( m_cVarStreamBitRate, "bitrate" );
    m_cVarStreamSampleRate = VariablePtr( new VarText( getIntf(), false ) );
    pVarManager->registerVar( m_cVarStreamSampleRate, "samplerate" );
    m_cVarStreamArt = VariablePtr( new VarString( getIntf() ) );
    pVarManager->registerVar( m_cVarStreamArt, "streamArt" );

    // Register the equalizer bands
    for( int i = 0; i < EqualizerBands::kNbBands; i++)
    {
        std::stringstream ss;
        ss << "equalizer.band(" << i << ")";
        pVarManager->registerVar( m_varEqBands.getBand( i ), ss.str() );
    }

    // XXX WARNING XXX
    // The object variable callbacks are called from other VLC threads,
    // so they must put commands in the queue and NOT do anything else
    // (X11 calls are not reentrant)

//    var_AddCallback( pIntf, "interaction", onInteraction, this );

    // initialize variables refering to libvlc and playlist objects
    init_variables();
}


VlcProc::~VlcProc()
{
    if( m_pVout )
    {
        vout_Release(m_pVout);
        m_pVout = NULL;
    }

    // var_DelCallback( getIntf(), "interaction", onInteraction, this );
}

int VlcProc::onEqBandsChange( vlc_object_t *pObj, const char *pVariable,
                              vlc_value_t oldVal, vlc_value_t newVal,
                              void *pParam )
{
    (void)pObj; (void)pVariable; (void)oldVal;
    VlcProc *pThis = (VlcProc*)pParam;

    // Post a set equalizer bands command
    CmdSetEqBands *pCmd = new CmdSetEqBands( pThis->getIntf(),
                                             pThis->m_varEqBands,
                                             newVal.psz_string );
    AsyncQueue *pQueue = AsyncQueue::instance( pThis->getIntf() );
    pQueue->push( CmdGenericPtr( pCmd ) );

    return VLC_SUCCESS;
}


int VlcProc::onEqPreampChange( vlc_object_t *pObj, const char *pVariable,
                               vlc_value_t oldVal, vlc_value_t newVal,
                               void *pParam )
{
    (void)pObj; (void)pVariable; (void)oldVal;
    VlcProc *pThis = (VlcProc*)pParam;
    EqualizerPreamp *pVarPreamp = (EqualizerPreamp*)(pThis->m_cVarEqPreamp.get());

    // Post a set preamp command
    CmdSetEqPreamp *pCmd = new CmdSetEqPreamp( pThis->getIntf(), *pVarPreamp,
                                              (newVal.f_float + 20.0) / 40.0 );
    AsyncQueue *pQueue = AsyncQueue::instance( pThis->getIntf() );
    pQueue->push( CmdGenericPtr( pCmd ) );

    return VLC_SUCCESS;
}

void VlcProc::reset_input()
{
    SET_BOOL( m_cVarSeekable, false );
    SET_BOOL( m_cVarRecordable, false );
    SET_BOOL( m_cVarRecording, false );
    SET_BOOL( m_cVarDvdActive, false );
    SET_BOOL( m_cVarHasAudio, false );
    SET_BOOL( m_cVarHasVout, false );
    SET_BOOL( m_cVarStopped, true );
    SET_BOOL( m_cVarPlaying, false );
    SET_BOOL( m_cVarPaused, false );

    SET_STREAMTIME( m_cVarTime, 0, false );
    SET_TEXT( m_cVarStreamName, UString( getIntf(), "") );
    SET_TEXT( m_cVarStreamURI, UString( getIntf(), "") );
    SET_TEXT( m_cVarStreamBitRate, UString( getIntf(), "") );
    SET_TEXT( m_cVarStreamSampleRate, UString( getIntf(), "") );

    getPlaytreeVar().onUpdateCurrent( false );
}

void VlcProc::init_variables()
{
    auto *pPlaylist = getPL();

    init_equalizer();
}

void VlcProc::update_current_input()
{
    //input_thread_t* pInput = getIntf()->p_sys->p_input;
    //if( !pInput )
    //    return;

    //input_item_t *pItem = input_GetItem( pInput );
    //if( pItem )
    //{
    //    // Update short name (as defined by --input-title-format)
    //    char *psz_fmt = var_InheritString( getIntf(), "input-title-format" );
    //    char *psz_name = NULL;
    //    if( psz_fmt != NULL )
    //    {
    //        psz_name = vlc_strfinput( pInput, NULL, psz_fmt );
    //        free( psz_fmt );
    //    }

    //    SET_TEXT( m_cVarStreamName, UString( getIntf(),
    //                                         psz_name ? psz_name : "" ) );
    //    free( psz_name );

    //    // Update local path (if possible) or full uri
    //    char *psz_uri = input_item_GetURI( pItem );
    //    char *psz_path = vlc_uri2path( psz_uri );
    //    char *psz_save = psz_path ? psz_path : psz_uri;
    //    SET_TEXT( m_cVarStreamURI, UString( getIntf(), psz_save ) );
    //    free( psz_path );
    //    free( psz_uri );

    //    // Update art uri
    //    char *psz_art = input_item_GetArtURL( pItem );
    //    SET_STRING( m_cVarStreamArt, std::string( psz_art ? psz_art : "" ) );
    //    free( psz_art );
    //}
}

void VlcProc::init_equalizer()
{
    auto *playlist = getPL();
    auto *player = vlc_playlist_GetPlayer(playlist);
    auto *pAout = vlc_player_aout_Hold(player);

    if( pAout )
    {
        if( !var_Type( pAout, "equalizer-bands" ) )
            var_Create( pAout, "equalizer-bands",
                        VLC_VAR_STRING | VLC_VAR_DOINHERIT);
        if( !var_Type( pAout, "equalizer-preamp" ) )
            var_Create( pAout, "equalizer-preamp",
                        VLC_VAR_FLOAT | VLC_VAR_DOINHERIT);

        // New Aout (addCallbacks)
        var_AddCallback( pAout, "audio-filter", onAoutCallback, this );
        var_AddCallback( pAout, "equalizer-bands",
                         onEqBandsChange, this );
        var_AddCallback( pAout, "equalizer-preamp",
                         onEqPreampChange, this );
    }

    // is equalizer enabled ?
    char *pFilters = pAout ?
                   var_GetNonEmptyString( pAout, "audio-filter" ) :
                   var_InheritString( getIntf(), "audio-filter" );
    bool b_equalizer = pFilters && strstr( pFilters, "equalizer" );
    free( pFilters );
    SET_BOOL( m_cVarEqualizer, b_equalizer );

    // retrieve initial bands
    char* bands = pAout ?
                  var_GetString( pAout, "equalizer-bands" ) :
                  var_InheritString( getIntf(), "equalizer-bands" );
    if( bands )
    {
        m_varEqBands.set( bands );
        free( bands );
    }

    // retrieve initial preamp
    float preamp = pAout ?
                   var_GetFloat( pAout, "equalizer-preamp" ) :
                   var_InheritFloat( getIntf(), "equalizer-preamp" );
    EqualizerPreamp *pVarPreamp = (EqualizerPreamp*)m_cVarEqPreamp.get();
    pVarPreamp->set( (preamp + 20.0) / 40.0 );

    if( pAout )
        aout_Release(pAout);
}

void VlcProc::setFullscreenVar( bool b_fullscreen )
{
    SET_BOOL( m_cVarFullscreen, b_fullscreen );
}

#undef  SET_BOOL
#undef  SET_STREAMTIME
#undef  SET_TEXT
#undef  SET_STRING
#undef  SET_VOLUME
