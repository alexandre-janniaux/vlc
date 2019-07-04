/*****************************************************************************
 * freeboxos.c : Indexation controller for freeboxOS
 *****************************************************************************
 * Copyright (C) 2019 VideoLabs
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_media_source.h>
#include <vlc_media_library.h>
#include <vlc_threads.h>
#include <vlc_keystore.h>
#include <vlc_url.h>


typedef struct
{
    struct vlc_media_tree *tree;
} intf_thread_sys_t;

static void test_keystore(intf_thread_t *obj, const char *mrl);

static void OnItemCleared(
    struct vlc_media_tree *tree,
    input_item_node_t *node,
    void *userdata
) {
    intf_thread_t *intf = userdata;
    intf_thread_sys_t *sys = intf->p_sys;
    msg_Info(intf, "Children cleared: node=%p", node->p_item);
}

static void OnItemAdded(
    struct vlc_media_tree *tree,
    input_item_node_t *parent,
    input_item_node_t *const children[],
    size_t count,
    void *userdata
) {
    intf_thread_t *intf = userdata;
    intf_thread_sys_t *sys = intf->p_sys;

    for(size_t i=0; i<count; ++i)
    {
        const char *mrl = children[i]->p_item->psz_uri;
        const char *scheme_end = strchr(mrl, ':');
        int scheme_length = scheme_end ? (int)(scheme_end - mrl) : 0;
        msg_Info(intf, "Children added: length=%d, scheme=%.*s, uri=%s", scheme_length, scheme_length, mrl, mrl);

        if (!strncmp(mrl, "fbxapi", scheme_length))
        {
            msg_Info(intf, "Found freebox, asking usage");
            test_keystore(intf, mrl);
            vlc_media_tree_Preparse(tree, vlc_object_instance(intf), children[i]->p_item);
        }
    }
}

void OnItemRemoved(
    struct vlc_media_tree *tree,
    input_item_node_t *node,
    input_item_node_t *const children[],
    size_t count,
    void *userdata
) {
    intf_thread_t *intf = userdata;
    intf_thread_sys_t *sys = intf->p_sys;
    msg_Err(intf, "Children removed");
}

static void test_keystore(intf_thread_t *obj, const char *mrl)
{
    vlc_url_t url;
    vlc_UrlParse(&url, mrl);

    vlc_keystore *store = vlc_keystore_create(obj);
    if (store == NULL)
    {
        msg_Err(obj, "No keystore available");
        return VLC_EGENERIC;
    }

    const char * const      values[KEY_MAX] = {
        [KEY_PROTOCOL] = "fbxapi",
        [KEY_USER] = "VLC_FBXAPI",
        [KEY_SERVER] = url.psz_host,
        [KEY_PATH] = NULL,
        [KEY_PORT] = "922",
        [KEY_REALM] = NULL,
        [KEY_AUTHTYPE] = NULL,
    };


    vlc_keystore_entry *entries;
    unsigned int entries_number = vlc_keystore_find(store, values, &entries);

    msg_Info(obj, "There are %u entries", entries_number);

    if (entries_number < 1)
    {
        char *app_token = strdup("fHknH11EcmMYGt9lmghirzDRUvtb+OTn1Q7lKDU+nhdZ5yQPUj51HDb2WMgOZmUj"); // FIXME
        vlc_keystore_store(store, values, app_token, strlen(app_token), "app_token");
    }
    else
    {
        vlc_keystore_release_entries(entries, entries_number);
    }

    vlc_keystore_release(store);
}

static int Open(vlc_object_t *obj)
{
    libvlc_int_t *libvlc = vlc_object_instance(obj);
    struct vlc_media_source *provider = vlc_media_source_provider_Get(libvlc);

    static const struct vlc_media_tree_callbacks media_tree_cbs =
    {
        .on_children_reset = OnItemCleared,
        .on_children_added = OnItemAdded,
        .on_children_removed = OnItemRemoved,
    };

    /* We'll list all media source provider to find one with a fbxapi://
     * scheme. */
    struct vlc_media_source_meta_list *provider_list =
        vlc_media_source_provider_List(provider, SD_CAT_LAN);

    unsigned count = vlc_media_source_meta_list_Count(provider_list);
    for(unsigned source_idx=0; source_idx < count; source_idx++)
    {
        struct vlc_media_source_meta *meta =
            vlc_media_source_meta_list_Get(provider_list, source_idx);

        msg_Info(obj, "Found source: name=\"%s\", longname=\"%s\"",
                 meta->name, meta->longname);

        if (strcmp("microdns", meta->name))
            continue;

        vlc_media_source_t *source =
            vlc_media_source_provider_GetMediaSource(provider, meta->name);

        vlc_media_tree_AddListener(source->tree, &media_tree_cbs, obj, true);
    }


    vlc_media_source_meta_list_Delete(provider_list);

}

static void Close(vlc_object_t *obj)
{
    /* Nothing to do */
}

vlc_module_begin()
    set_shortname("Freebox indexation service")
    set_category(CAT_INTERFACE)
    set_subcategory(SUBCAT_INTERFACE_MAIN)
    set_description("Indexation manager for freebox files via FreeboxOS")
    set_capability("interface", 0)

    set_callbacks(Open, Close)
    add_shortcut("freeboxos")
vlc_module_end()
