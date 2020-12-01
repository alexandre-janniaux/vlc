#ifndef RPC_CAPI_H
#define RPC_CAPI_H

struct libvlc_instance_t;
struct stream_t;

typedef struct libvlc_instance_t libvlc_instance_t;
typedef struct stream_t stream_t;

#ifdef __cplusplus
extern "C" {
#endif

libvlc_instance_t* capi_libvlc_new(int argc, const char *const *argv);
stream_t* capi_vlc_stream_NewURLEx(libvlc_instance_t* vlc, const char* mrl, int preparse);

#ifdef __cplusplus
}
#endif

#endif
