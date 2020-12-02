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

int vlc_broker_Init(void);
int vlc_broker_CreateAccess(const char* url, bool preparse);

/*
 * stream apis
 */

ssize_t vlc_RemoteStream_Read(stream_t* s, void* buf, size_t len);
int vlc_RemoteStream_Seek(stream_t* s, uint64_t offset);
block_t* vlc_RemoteStream_Block(stream_t* s, bool* eof);
void vlc_RemoteStream_Destroy(stream_t* s);
int vlc_RemoteStream_Readdir(stream_t*, input_item_node_t*);
int vlc_RemoteStream_Demux(stream_t* s);
int vlc_RemoteStream_Control(stream_t* s, int, va_list);


#ifdef __cplusplus
}
#endif


#endif
