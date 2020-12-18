#ifndef RPC_CAPI_H
#define RPC_CAPI_H

#ifndef __cplusplus
#include <stdbool.h>
#endif

struct libvlc_instance_t;
struct vlc_object_t;
struct stream_t;
struct es_out_t;

typedef struct libvlc_instance_t libvlc_instance_t;
typedef struct vlc_object_t vlc_object_t;
typedef struct stream_t stream_t;
typedef stream_t demux_t;
typedef struct es_out_t es_out_t;

#ifdef __cplusplus
extern "C" {
#endif

libvlc_instance_t* capi_libvlc_new(int argc, const char *const *argv);
vlc_object_t* capi_libvlc_instance_obj(libvlc_instance_t* instance);
stream_t* capi_vlc_stream_NewURLEx(libvlc_instance_t* vlc, const char* mrl, int preparse);
demux_t* capi_vlc_demux_NewEx(vlc_object_t* obj, const char* name,
        stream_t* s, es_out_t* out, bool preparsing);

#ifdef __cplusplus
}
#endif

#endif
