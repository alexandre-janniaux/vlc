/*****************************************************************************
 * idviu.c: idviu interface module
 *****************************************************************************
 * Copyright © 2019 VideoLabs
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define _XOPEN_SOURCE_EXTENDED 1

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_player.h>
#include <vlc_playlist_legacy.h>

static int  Open           (vlc_object_t *);
static void Close          (vlc_object_t *);

vlc_module_begin ()
    set_shortname("idviu")
    set_description(N_("Idviu latency interface"))
    set_capability("interface", 10)
    set_category(CAT_INTERFACE)
    set_subcategory(SUBCAT_INTERFACE_MAIN)
    set_callbacks(Open, Close)
    add_shortcut("idviu")

    add_loadfile( "idviu-avsync-device", "/proc/idviu/avsync",
              "Proc device for avsync",
              "Proc device for audio-video synchronization" );
vlc_module_end ()

struct intf_sys_t
{
    vlc_thread_t    thread;
    int             avsync_fd;
};

static void *Run(void *data)
{
    intf_thread_t *intf = data;
    intf_sys_t    *sys  = intf->p_sys;
    //vlc_playlist_t *playlist = vlc_intf_GetMainPlaylist(intf);
    //vlc_player_t *player = vlc_playlist_GetPlayer(playlist);

    playlist_t *playlist = pl_Get(intf);

    char buffer[100];

    for (;;) {
        vlc_testcancel();

        //vlc_player_Lock(player);
        //input_item_t *item = vlc_player_GetCurrentMedia(player);
        //vlc_player_Unlock(player);

        input_thread_t *input = playlist_CurrentInput(playlist);

        //if (item != NULL)
        if (input != NULL)
        {

            int canc = vlc_savecancel();

            /* flush current data */
            lseek(sys->avsync_fd, 0, SEEK_SET);
            ssize_t size = read(sys->avsync_fd, buffer, sizeof(buffer));
            if (size <= 0) {}

            msg_Info(intf, "Read \"%*s\" from avsync device", size, buffer);

            int avsync_delay;
            if (sscanf(buffer, "%d", &avsync_delay) != 1)
            {
                // read error
            }

            //vlc_player_Lock(player);
            //vlc_player_SetAudioDelay(player, VLC_TICK_FROM_MS(avsync_delay), VLC_PLAYER_WHENCE_ABSOLUTE);
            //vlc_player_Unlock(player);

            var_SetInteger(input, "audio-delay", VLC_TICK_FROM_MS(avsync_delay));

            vlc_restorecancel(canc);

            msg_Info(intf, "Setting audio delay to %d ms", avsync_delay);
        }
        else
        {
            msg_Warn(intf, "No current media, not setting audio delay");
        }

        vlc_tick_sleep(VLC_TICK_FROM_MS(1000));
    }
    vlc_assert_unreachable();
}

static int Open(vlc_object_t *p_this)
{
    intf_thread_t *intf = (intf_thread_t *)p_this;
    intf_sys_t    *sys  = intf->p_sys = calloc(1, sizeof(intf_sys_t));

    if (!sys)
        return VLC_ENOMEM;

    char *avsync_device = var_InheritString(intf, "idviu-avsync-device");
    sys->avsync_fd = open(avsync_device, O_RDONLY);
    if (sys->avsync_fd < 0)
    {
        free(sys);
        msg_Err(intf, "cannot open avsync device at path: %s",
                avsync_device);
        free(avsync_device);
        return VLC_EGENERIC;
    }
    msg_Info(intf, "using %s as avsync device", avsync_device);
    free(avsync_device);

    if (vlc_clone(&sys->thread, Run, intf, VLC_THREAD_PRIORITY_LOW))
        abort(); /* TODO */

    return VLC_SUCCESS;
}

static void Close(vlc_object_t *p_this)
{
    intf_thread_t *intf = (intf_thread_t *)p_this;
    intf_sys_t *sys = intf->p_sys;

    vlc_cancel(sys->thread);
    vlc_join(sys->thread, NULL);

    close(sys->avsync_fd);

    free(sys);
}
