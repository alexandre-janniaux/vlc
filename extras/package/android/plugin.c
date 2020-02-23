#include <stddef.h>
#include <stdint.h>
#define __LIBVLC__
#define __PLUGIN__
#define MODULE_STRING "vlcplugins"
#include <vlc_plugin.h>
typedef int (*vlc_plugin_entry) (vlc_set_cb, void *);

#define VLC_EXPORT __attribute__((visibility("default")))
#define VLC_LOCAL __attribute__((visibility("hidden")))

#define VLC_PLUGIN(name) int name();
#include "plugins.manifest.h"
#undef VLC_PLUGIN

#define VLC_PLUGIN(name) name,
static const vlc_plugin_entry vlc_plugin_entries[] = {
#   include "plugins.manifest.h"
    NULL
};
#undef VLC_PLUGIN

EXTERN_SYMBOL DLL_SYMBOL \
int CDECL_SYMBOL VLC_SYMBOL(vlc_entry)(vlc_set_cb func_set, void *opaque)
{{
    for(const vlc_plugin_entry *entry=vlc_plugin_entries;
        *entry != NULL; entry++)
    {{
        int ret = (*entry)(func_set, opaque);
        if (ret != 0)
            return ret;
    }}
    return 0;
}}

VLC_METADATA_EXPORTS
VLC_MODULE_NAME_HIDDEN_SYMBOL
