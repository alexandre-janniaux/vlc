#include <stddef.h>
#include <stdint.h>
#define __LIBVLC__
#define __PLUGIN__
#define MODULE_STRING "vlcplugins"
#include <vlc_plugin.h>
typedef int (*vlc_set_cb) (void *, void *, int, ...);
typedef int (*vlc_plugin_entry) (vlc_set_cb, void *);

#define VLC_EXPORT __attribute__((visibility("default")))
#define VLC_LOCAL __attribute__((visibility("hidden")))

VLC_LOCAL
static const vlc_plugin_entry vlc_plugin_entries[];

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
