#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_demux.h>

#include <stdexcept>
#include <cstdio>
#include "protoipc/port.hh"
#include "protorpc/channel.hh"
#include "demuxfactory.hh"
#include "demux.hh"

// Big hax
#include "../../src/rpc/proxify.hh"

#include "capi.h"

void start_factory(int channel_fd, int port_val)
{
    ipc::Port port(channel_fd);
    rpc::PortId port_id = port_val;

    std::printf("[DEMUXFACTORY] Starting out of process demux factory [port_fd=%i, port_id%lu]\n",
            port.handle(),
            port_id);

    rpc::Channel channel(port_id, port);
    channel.bind_static<DemuxFactory>(0, &channel);

    std::printf("[DEMUXFACTORY] Factory registered, waiting for creation requests ...\n");

    channel.loop();
}

DemuxFactory::DemuxFactory(rpc::Channel* chan)
    : channel_(chan)
{
    const char* args[] = {
        "-v",
        "--ignore-config",
        "-I",
        "dummy",
        "--no-media-library",
        "--vout=none",
        "--aout=none",
    };

    vlc_instance_ = capi_libvlc_new(sizeof(args) / sizeof(args[0]), args);

    if (!vlc_instance_)
        throw std::runtime_error("[DEMUXFACTORY] Could not create vlc instance");
};

bool DemuxFactory::create(vlc::RemoteAccess access, vlc::RemoteControl control, vlc::RemoteEsOut out,
        std::string module, bool preparsing, std::uint64_t* demux_object)
{
    // We assume that the access and its control are in the same process (as it should be).
    remote_stream_t remote_access =
    {
        access.object_id,
        control.object_id,
        access.port
    };

    stream_t* remote_stream_obj = reinterpret_cast<stream_t*>(std::malloc(sizeof(stream_t)));
    vlc_rpc_ProxifyStream(remote_stream_obj, &remote_access, channel_);

    remote_esout_t remote_esout =
    {
        out.object_id,
        out.port
    };

    es_out_t* remote_esout_obj = vlc_rpc_ProxifyEsOut(&remote_esout, channel_);

    // Now create the demux object
    demux_t* result = capi_vlc_demux_NewEx(NULL, module.c_str(), remote_stream_obj, remote_esout_obj, preparsing);
    *demux_object = channel_->bind<Demux>(result);

    return true;
}
