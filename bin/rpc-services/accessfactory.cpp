#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_stream.h>

#include <stdexcept>
#include <cstdio>
#include "protoipc/port.hh"
#include "protorpc/channel.hh"
#include "accessfactory.hh"
#include "access.hh"
#include "control.hh"

#include "capi.h"

void start_factory(int channel_val, int port_val)
{
    ipc::Port port(channel_val);
    rpc::PortId port_id = port_val;

    std::printf("[ACCESSFACTORY] Starting out of process access factory [port_fd=%i, port_id=%lu]\n",
            port.handle(),
            port_id);

    rpc::Channel channel(port_id, port);

    // The factory object for new accesses is bound to object id 0 for
    // bootstrapping. We give it a handle to the channel to be able to
    // create new objects.
    channel.bind_static<AccessFactory>(0, &channel);

    std::printf("[ACCESSFACTORY] Factory registered, waiting for creation requests ...\n");
    channel.loop();
}

AccessFactory::AccessFactory(rpc::Channel* chan)
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
        throw std::runtime_error("[ACCESSFACTORY] Could not create vlc instance");
}

bool AccessFactory::create(std::string url, bool preparsing, std::uint64_t* stream_object, std::uint64_t* control_object)
{
    std::printf("[ACCESSFACTORY] Creating access for url: %s\n", url.c_str());

    stream_t* stream = capi_vlc_stream_NewURLEx(vlc_instance_, url.c_str(), preparsing);
    *stream_object = channel_->bind<Access>(stream);
    *control_object = channel_->bind<Control>(stream);

    std::printf("[ACCESSFACTORY] Created access id: %lu with control %lu\n", *stream_object, *control_object);

    return true;
}
