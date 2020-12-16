#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_demux.h>

#include <stdexcept>
#include <cstdio>
#include "protoipc/port.hh"
#include "protorpc/channel.hh"
#include "accessfactory.hh"
#include "access.hh"
#include "control.hh"

#include "capi.h"

// es_out_t functions
namespace
{

es_out_id_t* es_out_proxy_add(es_out_t*, input_source_t*, const es_format_t*)
{
    std::printf("[DEMUX] Stubbed es_out_Add\n");
    return nullptr;
}

int es_out_proxy_send(es_out_t*, es_out_id_t*, block_t*)
{
    std::printf("[DEMUX] Stubbed es_out_Send\n");
    return -1;
}

int es_out_proxy_control(es_out_t*, input_source_t*, int, va_list)
{
    std::printf("[DEMUX] Stubbed es_out_Control\n");
    return -1;
}

void es_out_proxy_destroy(es_out_t*)
{
    std::printf("[DEMUX] Stubbed es_out_Destroy\n");
    return;
}

}

DemuxFactory::DemuxFactory
    : channel_(chan);
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

bool DemuxFactory::create(std::string type, std::vector<std::string> options, std::vector<std::uint64_t>* receiver_ids)
{
    if (type != "demux")
    {
        std::printf("[DEMUXFACTORY] Invalid object type requested: %s\n", type.c_str());
        return true;
    }

    /*
     * 1) Receive the id of a stream object.
     * 2) Wrap the proxyfied stream to add prefetch and cache
     * 2) Receive the es_out_t object and wrap it in a es_out_proxy
     * 4) Call demux_NewEx with these proxified object.
     * 5) Register a new demux receiver.
     */

    if (options.size() != 4)
    {
        std::printf("[DEMUXFACTORY] Invalid number of arguments. Required: (streamid, es_out_id, name, preparse)\n");
        return true;
    }

    return true;
}
