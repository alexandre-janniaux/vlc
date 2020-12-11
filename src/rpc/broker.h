#ifndef RPC_BROKER_HH
#define RPC_BROKER_HH

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_stream.h>

#ifdef __cplusplus
extern "C" {
#endif

int vlc_broker_Init(libvlc_int_t* libvlc);
int vlc_broker_CreateAccess(stream_t* s, const char* url, bool preparse);

#ifdef __cplusplus
}
#endif


#endif
