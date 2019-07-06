/*****************************************************************************
 * vlcproc.hpp
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

#ifndef VLCPROC_HPP
#define VLCPROC_HPP

#include <set>

#include <vlc_common.h>
#include <vlc_input.h>
#include <vlc_vout.h>
#include <vlc_player.h>
#include "../vars/equalizer.hpp"
#include "../vars/playtree.hpp"
#include "../vars/time.hpp"
#include "../vars/volume.hpp"
#include "../utils/position.hpp"
#include "../utils/var_text.hpp"
#include "../utils/var_string.hpp"
#include "../controls/ctrl_video.hpp"

class OSTimer;
class VarBool;
struct vout_window_t;


/// Singleton object handling VLC internal state and playlist
class VlcProc: public SkinObject
{
public:
    /// Get the instance of VlcProc
    /// Returns NULL if the initialization of the object failed
    static VlcProc *instance( intf_thread_t *pIntf );

    /// Delete the instance of VlcProc
    static void destroy( intf_thread_t *pIntf );

    /// Getter for the playtree variable
    Playtree &getPlaytreeVar() { return *((Playtree*)m_cPlaytree.get()); }

    /// Getter for the time variable
    StreamTime &getTimeVar() { return *((StreamTime*)(m_cVarTime.get())); }

    /// Getter for the volume variable
    Volume &getVolumeVar() { return *((Volume*)(m_cVarVolume.get())); }

    /// Getter for the current playback speed
    VarText &getSpeedVar()
       { return *((VarText*)(m_cVarSpeed.get())); }

    /// Getter for the stream name variable
    VarText &getStreamNameVar()
       { return *((VarText*)(m_cVarStreamName.get())); }

    /// Getter for the stream URI variable
    VarText &getStreamURIVar()
        { return *((VarText*)(m_cVarStreamURI.get())); }

    /// Getter for the stream bitrate variable
    VarText &getStreamBitRateVar()
        { return *((VarText*)(m_cVarStreamBitRate.get())); }

    /// Getter for the stream sample rate variable
    VarText &getStreamSampleRateVar()
        { return *((VarText*)(m_cVarStreamSampleRate.get())); }

    /// Getter for the stream Art url variable
    VarString &getStreamArtVar()
       { return *((VarString*)(m_cVarStreamArt.get())); }

    /// Getter/Setter for the fullscreen variable
    VarBool &getFullscreenVar() { return *((VarBool*)(m_cVarFullscreen.get())); }
    void setFullscreenVar( bool );

    /// Indicate whether the embedded video output is currently used
    bool isVoutUsed() const { return m_pVout != NULL; }

    /// initialize equalizer
    void init_equalizer( );

    /// update global variables for the current input
    void update_current_input( );

private:

    static void
    on_current_media_changed(vlc_player_t* player,
                             input_item_t *new_media,
                             void *data);

    static void
    on_state_changed(vlc_player_t *player,
                     vlc_player_state new_state,
                     void *data);

    static void
    on_error_changed(vlc_player_t *player,
                     vlc_player_error error,
                     void *data);

    static void
    on_buffering_changed(vlc_player_t *player,
                         float new_buffering,
                         void *data);

    static void
    on_rate_changed(vlc_player_t *player,
                    float new_rate,
                    void *data);

    static void
    on_capabilities_changed(vlc_player_t *player,
                            int old_caps, int new_caps,
                            void *data);

    static void
    on_position_changed(vlc_player_t *player,
                        vlc_tick_t new_time,
                        float new_pos,
                        void *data);

    static void
    on_length_changed(vlc_player_t *player,
                      vlc_tick_t new_length,
                      void *data);

    static void
    on_track_list_changed(vlc_player_t *player,
                          vlc_player_list_action action,
                          const vlc_player_track *track,
                          void *data);

    static void
    on_track_selection_changed(vlc_player_t *player,
                               vlc_es_id_t *unselected_id,
                               vlc_es_id_t *selected_id,
                               void *data);

    static void
    on_program_list_changed(vlc_player_t *player,
                            vlc_player_list_action action,
                            const vlc_player_program *prgm,
                            void *data);

    static void
    on_program_selection_changed(vlc_player_t *player,
                                 int unselected_id, int selected_id,
                                 void *data);

    static void
    on_titles_changed(vlc_player_t *player,
                      vlc_player_title_list *titles,
                      void *data);

    static void
    on_title_selection_changed(vlc_player_t *player,
                               const struct vlc_player_title *new_title,
                               size_t new_idx,
                               void *data);

    static void
    on_chapter_selection_changed(vlc_player_t *player,
                                 const vlc_player_title *title,
                                 size_t t_idx,
                                 const vlc_player_chapter *chap,
                                 size_t c_idx,
                                 void *data);

    static void
    on_teletext_menu_changed(vlc_player_t *player,
                             bool has_teletext_menu,
                             void *data);

    static void
    on_teletext_enabled_changed(vlc_player_t *player,
                                bool enabled,
                                void *data);

    static void
    on_teletext_page_changed(vlc_player_t *player,
                             unsigned new_page, void *data);

    static void
    on_teletext_transparency_changed(vlc_player_t *player,
                                     bool enabled,
                                     void *data);

    static void
    on_category_delay_changed(vlc_player_t *player,
                              es_format_category_e cat,
                              vlc_tick_t new_delay,
                              void *data);

    static void
    on_associated_subs_fps_changed(vlc_player_t *player,
                                   float subs_fps,
                                   void *data);

    static void
    on_renderer_changed(vlc_player_t *player,
                        vlc_renderer_item_t *new_item,
                        void *data);

    static void
    on_recording_changed(vlc_player_t *player,
                         bool recording,
                         void *data);

    static void
    on_signal_changed(vlc_player_t *player,
                      float quality,
                      float strength,
                      void *data);

    static void
    on_statistics_changed(vlc_player_t *player,
                          const input_stats_t *stats,
                          void *data);

    static void
    on_atobloop_changed(vlc_player_t *player,
                        vlc_player_abloop new_state,
                        vlc_tick_t time, float pos,
                        void *data);

    static void
    on_media_stopped_action_changed(vlc_player_t *player,
                                    vlc_player_media_stopped_action new_action,
                                    void *data);

    static void
    on_media_meta_changed(vlc_player_t *player,
                          input_item_t *media,
                          void *data);

    static void
    on_media_epg_changed(vlc_player_t *player,
                         input_item_t *media,
                         void *data);

    static void
    on_media_subitems_changed(vlc_player_t *player,
                              input_item_t *media,
                              input_item_node_t *new_subitems,
                              void *data);

    static void
    on_vout_changed(vlc_player_t *player,
                    vlc_player_vout_action action,
                    vout_thread_t *vout,
                    vlc_vout_order order,
                    vlc_es_id_t *es,
                    void *data);

    static void
    on_cork_changed(vlc_player_t *player,
                    unsigned cork_count,
                    void *data);

    static const vlc_player_cbs player_cbs;

    static void
    on_fullscreen_changed(vlc_player_t *player,
                          vout_thread_t *vout,
                          bool enabled,
                          void *data);

    static void
    on_wallpaper_mode_changed(vlc_player_t *player,
                              vout_thread_t *vout,
                              bool enabled,
                              void *data);

    static const vlc_player_vout_cbs player_vout_cbs;

    static void
    on_volume_changed(vlc_player_t *player,
                      float new_volume,
                      void *data);

    static void
    on_mute_changed(vlc_player_t *player,
                    bool is_muted,
                    void *data);

    static void
    on_device_changed(vlc_player_t *player,
                      bool is_muted,
                      void *data);

    static const vlc_player_aout_cbs player_aout_cbs;

    static void
    on_items_reset(vlc_playlist_t *playlist,
                   vlc_playlist_item_t *const items[],
                   size_t count,
                   void *data);

    static void
    on_items_added(vlc_playlist_t *playlist,
                   size_t index,
                   vlc_playlist_item_t *const items[],
                   size_t count,
                   void *data);

    static void
    on_items_moved(vlc_playlist_t *playlist,
                   size_t index,
                   size_t count,
                   size_t target,
                   void *userdata);

    static void
    on_items_removed(vlc_playlist_t *playlist,
                     size_t index,
                     size_t count,
                     void *data);

    static void
    on_items_updated(vlc_playlist_t *playlist,
                     size_t index,
                     vlc_playlist_item_t *const items[],
                     size_t count,
                     void *data);

    static void
    on_playback_repeat_changed(vlc_playlist_t *playlist,
                               vlc_playlist_playback_repeat repeat,
                               void *data);

    static void
    on_playback_order_changed(vlc_playlist_t *playlist,
                              vlc_playlist_playback_order order,
                              void *data);

    static void
    on_current_index_changed(vlc_playlist_t *playlist,
                             ssize_t index,
                             void *data);

    static void
    on_has_prev_changed(vlc_playlist_t *playlist,
                        bool has_prev,
                        void *data);

    static void
    on_has_next_changed(vlc_playlist_t *playlist,
                        bool has_next,
                        void *data);

    static const vlc_playlist_callbacks playlist_cbs;

protected:
    // Protected because it is a singleton
    VlcProc( intf_thread_t *pIntf );
    virtual ~VlcProc();

private:
    // Player listeners
    vlc_player_listener_id *m_playerListener;
    vlc_player_aout_listener_id *m_playerAoutListener;
    vlc_player_vout_listener_id *m_playerVoutListener;

    /// Playtree variable
    VariablePtr m_cPlaytree;
    VariablePtr m_cVarRandom;
    VariablePtr m_cVarLoop;
    VariablePtr m_cVarRepeat;
    /// Variable for current position of the stream
    VariablePtr m_cVarTime;
    /// Variable for audio volume
    VariablePtr m_cVarVolume;
    /// Variable for speed playback
    VariablePtr m_cVarSpeed;
    /// Variable for current stream properties
    VariablePtr m_cVarStreamName;
    VariablePtr m_cVarStreamURI;
    VariablePtr m_cVarStreamBitRate;
    VariablePtr m_cVarStreamSampleRate;
    VariablePtr m_cVarStreamArt;
    /// Variable for the "mute" state
    VariablePtr m_cVarMute;
    /// Variables related to the input
    VariablePtr m_cVarPlaying;
    VariablePtr m_cVarStopped;
    VariablePtr m_cVarPaused;
    VariablePtr m_cVarSeekable;
    VariablePtr m_cVarRecordable;
    VariablePtr m_cVarRecording;
    /// Variables related to the vout
    VariablePtr m_cVarFullscreen;
    VariablePtr m_cVarHasVout;
    /// Variables related to audio
    VariablePtr m_cVarHasAudio;
    /// Equalizer variables
    EqualizerBands m_varEqBands;
    VariablePtr m_cVarEqPreamp;
    VariablePtr m_cVarEqualizer;
    /// Variable for DVD detection
    VariablePtr m_cVarDvdActive;

    VariablePtr m_cVarHasPrevious;
    VariablePtr m_cVarHasNext;

    /// Vout thread
    vout_thread_t *m_pVout;

    // reset variables when input is over
    void reset_input();

    // init variables (libvlc and playlist levels)
    void init_variables();

    /// Callback for input-current variable
    static int onInputNew( vlc_object_t *pObj, const char *pVariable,
                           vlc_value_t oldVal, vlc_value_t newVal,
                           void *pParam );

    /// Callback for item-change variable
    static int onItemChange( vlc_object_t *pObj, const char *pVariable,
                             vlc_value_t oldVal, vlc_value_t newVal,
                             void *pParam );

    /// Callback for item-change variable
    static int onItemAppend( vlc_object_t *pObj, const char *pVariable,
                             vlc_value_t oldVal, vlc_value_t newVal,
                             void *pParam );

    /// Callback for item-change variable
    static int onItemDelete( vlc_object_t *pObj, const char *pVariable,
                             vlc_value_t oldVal, vlc_value_t newVal,
                             void *pParam );

    static int onInteraction( vlc_object_t *pObj, const char *pVariable,
                              vlc_value_t oldVal, vlc_value_t newVal,
                              void *pParam );

    static int onEqBandsChange( vlc_object_t *pObj, const char *pVariable,
                                vlc_value_t oldVal, vlc_value_t newVal,
                                void *pParam );

    static int onEqPreampChange( vlc_object_t *pObj, const char *pVariable,
                                 vlc_value_t oldVal, vlc_value_t newVal,
                                 void *pParam );

    /// Generic Callback
    static int onGenericCallback( vlc_object_t *pObj, const char *pVariable,
                                  vlc_value_t oldVal, vlc_value_t newVal,
                                  void *pParam );
    static int onInputCallback( vlc_object_t *pObj, const char *pVariable,
                                vlc_value_t oldVal, vlc_value_t newVal,
                                void *pParam );

    static int onVoutCallback( vlc_object_t *pObj, const char *pVariable,
                               vlc_value_t oldVal, vlc_value_t newVal,
                               void *pParam );

    /// Generic Callback for intf-event
    static int onIntfEvent( vlc_object_t *pObj, const char *pVariable,
                            vlc_value_t oldVal, vlc_value_t newVal,
                            void *pParam );
};

#endif
