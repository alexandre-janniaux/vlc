#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_demux.h>
#include <vlc_stream.h>

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

    std::printf("[DEMUXFACTORY] Starting out of process demux factory [port_fd=%i, port_id=%lu]\n",
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

bool DemuxFactory::create(vlc::RemoteAccess access, vlc::RemoteEsOut out,
        std::string module, std::string filepath, bool preparsing, std::uint64_t* demux_object)
{
    vlc_object_t* instance_obj = capi_libvlc_instance_obj(vlc_instance_);

    // We assume that the access and its control are in the same process (as it should be).
    remote_stream_t remote_access =
    {
        access.object_id,
        access.port
    };

    std::printf("[DEMUXFACTORY] Remote access [access_id=%lu, port=%lu]\n",
            remote_access.object_id, remote_access.port);

    // Destroy ptr can be anything as it will be set by vlc_rpc_ProxifyStream
    stream_t* remote_stream_obj = vlc_stream_CommonNew(instance_obj, (void (*)(stream_t*))0xdeadbeef);
    remote_stream_obj->psz_url = "fake://lol";

    vlc_rpc_ProxifyStream(remote_stream_obj, &remote_access, channel_);

    remote_esout_t remote_esout =
    {
        out.object_id,
        out.port
    };

    std::printf("[DEMUXFACTORY] Remote esout [esout_id=%lu, port=%lu]\n",
            remote_esout.object_id, remote_esout.port);

    es_out_t* remote_esout_obj = vlc_rpc_ProxifyEsOut(&remote_esout, channel_);

    std::printf("[DEMUXFACTORY] Proxified esout\n");
    std::fflush(stdout);

    demux_t* result = capi_vlc_demux_NewEx(instance_obj, module.c_str(), filepath.c_str(), remote_stream_obj, remote_esout_obj, preparsing);
    *demux_object = channel_->bind<Demux>(result);

    std::printf("[DEMUXFACTORY] Created demux id=%lu\n", *demux_object);

    return true;
}
