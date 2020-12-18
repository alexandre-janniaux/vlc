#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stddef.h>
#include "capi.h"

#include <vlc_common.h>
#include <vlc_demux.h>
#include "../../lib/libvlc_internal.h"

libvlc_instance_t* capi_libvlc_new(int argc, const char *const *argv)
{
    return libvlc_new(argc, argv);
}

vlc_object_t* capi_libvlc_instance_obj(libvlc_instance_t* instance)
{
    return VLC_OBJECT(instance->p_libvlc_int);
}

stream_t* capi_vlc_stream_NewURLEx(libvlc_instance_t* vlc, const char* url, int preparse)
{
    return vlc_stream_NewURLEx(vlc->p_libvlc_int, url, preparse);
}

demux_t* capi_vlc_demux_NewEx(vlc_object_t* obj, const char* name,
        stream_t* s, es_out_t* out, bool preparsing)
{
    return demux_NewEx(obj, name, s, out, preparsing);
}
