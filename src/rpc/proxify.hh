#ifndef VLC_PROXIFY_HH
#define VLC_PROXIFY_HH

#include "protorpc/channel.hh"

struct stream_t;
typedef struct stream_t demux_t;
struct es_out_t;

struct remote_stream_t
{
    rpc::ObjectId object_id;
    rpc::ObjectId control_id;
    ipc::PortId port;
};

struct remote_esout_t
{
    rpc::ObjectId object_id;
    ipc::PortId port;
};

struct remote_demux_t
{
    remote_stream_t stream;
    remote_esout_t esout;
};

#ifdef __cplusplus
extern "C" {
#endif

void vlc_rpc_ProxifyStream(stream_t* local, remote_stream_t* remote, rpc::Channel* chan);
void vlc_rpc_ProxifyDemux(demux_t* local, remote_demux_t* remote, rpc::Channel* chan);
es_out_t* vlc_rpc_ProxifyEsOut(remote_esout_t* remote, rpc::Channel* chan);

#ifdef __cplusplus
}
#endif

#endif
