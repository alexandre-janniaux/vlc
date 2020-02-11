#include <stddef.h>
typedef int (*vlc_set_cb) (void *, void *, int, ...);
typedef int (*vlc_plugin_entry) (vlc_set_cb, void *);

#define VLC_EXPORT __attribute__((visibility("default")))
#define VLC_LOCAL __attribute__((visibility("hidden")))

VLC_EXPORT
const char vlc_module_name[] = "vlcplugins";

VLC_LOCAL
static const vlc_plugin_entry vlc_plugin_entries[];

VLC_EXPORT
int vlc_entry(vlc_set_cb func_set, void *opaque)
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

VLC_EXPORT
const char * vlc_entry_license()
{{
    return "LICENSE";
}}

VLC_EXPORT
const char * vlc_entry_copyright()
{{
    return "COPYRIGHT";
}}

VLC_EXPORT
const char * vlc_entry_api_version()
{{
    return "4.0.0";
}}
