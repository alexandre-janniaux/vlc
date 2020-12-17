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

#include "capi.h"

void start_factory(int channel_fd, int port_id)
{
    (void)channel_fd;
    (void)port_id;
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

bool DemuxFactory::create(vlc::RemoteAccess access, vlc::RemoteControl control, vlc::RemoteEsOut out, std::uint64_t* demux_object)
{
    return true;
}
