#ifndef VLC_PROXIFY_HH
#define VLC_PROXIFY_HH

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <vlc_common.h>
#include "protorpc/channel.hh"

struct stream_t;
typedef struct stream_t demux_t;
struct es_out_t;

struct remote_stream_t
{
    rpc::ObjectId object_id;
    rpc::PortId port;
};

struct remote_esout_t
{
    rpc::ObjectId object_id;
    rpc::PortId port;
};

struct remote_demux_t
{
    rpc::PortId port;
    remote_stream_t stream;
    remote_esout_t esout;
    rpc::ObjectId object_id;
};

VLC_API void vlc_rpc_ProxifyStream(stream_t* local, remote_stream_t* remote, rpc::Channel* chan);
VLC_API void vlc_rpc_ProxifyDemux(demux_t* local, remote_demux_t* remote, rpc::Channel* chan);
VLC_API es_out_t* vlc_rpc_ProxifyEsOut(remote_esout_t* remote, rpc::Channel* chan);

#endif
