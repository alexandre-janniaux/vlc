#include <thread>
#include <mutex>
#include <vector>
#include <filesystem>
#include <charconv>
#include <cstdio>
#include <cstring>

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "protoipc/router.hh"
#include "protorpc/channel.hh"
#include "streamfactory.sidl.hh"
#include "streamcontrol.sidl.hh"
#include "demuxfactory.sidl.hh"
#include "stream.sidl.hh"
#include "broker.h"
#include "esout.hh"
#include "proxify.hh"

namespace
{

struct vlc_stream_proxy_objects
{
    rpc::Proxy<vlc::StreamProxy> object_proxy;
    rpc::Proxy<vlc::StreamControlProxy> control_proxy;
};

std::mutex remote_stream_lock_;

bool resolve_helper_path(const char* program_name, std::string& output)
{
    char path_buff[4096];
    ssize_t count = readlink("/proc/self/exe", path_buff, sizeof(path_buff));

    if (count < 0)
        return false;

    std::filesystem::path bin_path(path_buff);
    std::filesystem::path bin_folder = bin_path.parent_path();
    std::filesystem::path prog_path = bin_folder / program_name;

    if (!std::filesystem::exists(prog_path))
        return false;

    auto status = std::filesystem::status(prog_path);

    if (status.type() != std::filesystem::file_type::regular)
        return false;

    auto perms = status.permissions();

    if ((perms & std::filesystem::perms::owner_exec) == std::filesystem::perms::none)
        return false;

    output = prog_path;

    return true;
}

/*
 * Creates a new process and returns its port id.
 *
 * XXX: Maybe return an optional to mark an error ?
 */

rpc::PortId vlc_broker_CreateProcess(libvlc_int_t* libvlc, const char* program_name)
{
    // 1: Resolve path to the binary (according to program_name)
    // 2: Create unix socketpair (parent - child)
    // 3: Add the ipc::Port to the router and obtain the portid.
    // 4: Fork
    //        - Build the argument list for the child and call execve.
    //          If execve fails then crash (all binary paths should be valid as
    //          they are hardcoded).
    //        - In the parent, return the portid
    auto* main_router = static_cast<ipc::Router*>(var_InheritAddress(libvlc, "rpc-broker-router"));

    if (!main_router)
        return 0;

    std::string bin_path;

    if (!resolve_helper_path(program_name, bin_path))
        return 0;

    // Create the ports for communication with the child process.
    ipc::Port tmp;
    ipc::Port child_to_broker;

    if (!ipc::Port::create_pair(tmp, child_to_broker))
        return 0;

    ipc::Port broker_to_child(fcntl(tmp.handle(), F_DUPFD_CLOEXEC, 0));

    if (broker_to_child.handle() == -1)
        return 0;

    tmp.close();
    rpc::PortId child_port_id = main_router->add_port(broker_to_child);

    pid_t pid = fork();

    if (pid == -1)
        return 0;

    if (pid == 0)
    {
        char fd_str[32] = {0};
        char channel_port_str[32] = {0};

        std::to_chars(fd_str, fd_str + sizeof(fd_str), child_to_broker.handle());
        std::to_chars(channel_port_str, channel_port_str + sizeof(channel_port_str), child_port_id);

        execl(bin_path.c_str(),
              bin_path.c_str(),
              fd_str,
              channel_port_str,
              0);

        throw std::runtime_error("execl failed");
    }

    child_to_broker.close();

    return child_port_id;
}

/*
 * Stream apis.
 */
ssize_t vlc_RemoteStream_Read(stream_t* s, void* buf, size_t len)
{
    std::lock_guard<std::mutex> lock(remote_stream_lock_);
    auto* sys = reinterpret_cast<vlc_stream_proxy_objects*>(s->p_sys);
    auto& stream = sys->object_proxy;

    std::printf("[STREAM-PROXY] Read request to #%lu\n", stream->remote_id());
    std::vector<std::uint8_t> data;
    int64_t status = 0;

    if (!stream->read(len, &status, &data))
        return -1;

    std::memcpy(buf, data.data(), data.size());

    return status;
}

int vlc_RemoteStream_Seek(stream_t* s, uint64_t offset)
{
    std::lock_guard<std::mutex> lock(remote_stream_lock_);
    auto* sys = reinterpret_cast<vlc_stream_proxy_objects*>(s->p_sys);
    auto& stream = sys->object_proxy;

    std::printf("[STREAM-PROXY] Seek request to #%lu\n", stream->remote_id());
    int32_t status = 0;

    if (!stream->seek(offset, &status))
        return -1;

    return status;
}

block_t* vlc_RemoteStream_Block(stream_t* s, bool* out_eof)
{
    std::lock_guard<std::mutex> lock(remote_stream_lock_);
    auto* sys = reinterpret_cast<vlc_stream_proxy_objects*>(s->p_sys);
    auto& stream = sys->object_proxy;

    std::printf("[STREAM-PROXY] Block #%lu\n", stream->remote_id());
    std::optional<vlc::Block> block;
    std::uint8_t eof = 0;

    if (!stream->block(&eof, &block))
        return NULL;

    *out_eof = eof;

    if (!block)
        return NULL;

    block_t* result = block_Alloc(block->buffer.size());
    result->i_flags = block->flags;
    result->i_nb_samples = block->nb_samples;
    result->i_pts = block->pts;
    result->i_dts = block->dts;
    result->i_length = block->length;

    std::memcpy(result->p_buffer, block->buffer.data(), block->buffer.size());

    return result;
}

void vlc_RemoteStream_Destroy(stream_t* s)
{
    std::lock_guard<std::mutex> lock(remote_stream_lock_);
    auto* sys = reinterpret_cast<vlc_stream_proxy_objects*>(s->p_sys);
    auto& stream = sys->object_proxy;

    std::printf("[STREAM-PROXY] Destroy #%lu\n", stream->remote_id());
    stream->destroy();
}

int vlc_RemoteStream_Readdir(stream_t* s, input_item_node_t*)
{
    std::printf("[STREAM-PROXY] Stream %p tried to call Readdir\n", s);
    return -1;
}

int vlc_RemoteStream_Demux(stream_t* s)
{
    std::printf("[STREAM-PROXY] Stream %p tried to call Demux\n", s);
    return -1;
}

int vlc_RemoteStream_Control(stream_t* s, int cmd, va_list args)
{
    std::lock_guard<std::mutex> lock(remote_stream_lock_);
    auto* sys = reinterpret_cast<vlc_stream_proxy_objects*>(s->p_sys);
    auto& control = sys->control_proxy;

    std::int64_t ret = VLC_EGENERIC;

    switch (cmd)
    {
        case STREAM_CAN_SEEK:
        {
            bool* result = va_arg(args, bool*);

            if (!control->can_seek(&ret, result))
                return VLC_EGENERIC;

            return ret;
        }
        case STREAM_CAN_FASTSEEK:
        {
            bool* result = va_arg(args, bool*);

            if (!control->can_fastseek(&ret, result))
                return VLC_EGENERIC;

            return ret;
        }
        case STREAM_CAN_PAUSE:
        {
            bool* result = va_arg(args, bool*);

            if (!control->can_pause(&ret, result))
                return VLC_EGENERIC;

            return ret;
        }
        case STREAM_CAN_CONTROL_PACE:
        {
            bool* result = va_arg(args, bool*);

            if (!control->can_control_pace(&ret, result))
                return VLC_EGENERIC;

            return ret;
        }
        case STREAM_GET_SIZE:
        {
            std::uint64_t* result = va_arg(args, std::uint64_t*);

            if (!control->get_size(&ret, result))
                return VLC_EGENERIC;

            return ret;
        }
        case STREAM_GET_PTS_DELAY:
        {
            vlc_tick_t* result = va_arg(args, vlc_tick_t*);

            if (!control->get_pts_delay(&ret, result))
                return VLC_EGENERIC;

            return ret;
        }
        case STREAM_SET_PAUSE_STATE:
        {
            int state = va_arg(args, int);

            if (!control->set_pause_state(state, &ret))
                return VLC_EGENERIC;

            return ret;
        }
        default:
            std::printf("[CONTROL-PROXY] Unhandled command: %i\n", cmd);
            return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

// Attempts to recover the rpc objects associated with a proxyfied stream.
vlc_stream_proxy_objects* stream_get_access_proxies(stream_t* s)
{
    // Seek to the last stream (should be the proxyfied access).
    while (s->s)
        s = s->s;

    // vlc_stream_proxy_objects is stored in stream_t::p_sys
    if (!s->p_sys)
        return nullptr;

    s = static_cast<stream_t*>(s->p_sys);

    // Proxyfied streams must have the proxyfied shim functions
    if (s->pf_read != &vlc_RemoteStream_Read)
        return nullptr;

    return static_cast<vlc_stream_proxy_objects*>(s->p_sys);
}

}

int vlc_broker_Init(libvlc_int_t* libvlc)
{
    ipc::Port router_to_broker;
    ipc::Port broker_to_router;

    if (!ipc::Port::create_pair(router_to_broker, broker_to_router))
        return -1;

    ipc::Port router_to_esout;
    ipc::Port esout_to_router;

    if (!ipc::Port::create_pair(router_to_esout, esout_to_router))
    {
        router_to_broker.close();
        broker_to_router.close();
        return -1;
    }

    auto* router = new ipc::Router;
    ipc::PortId broker_port_id = router->add_port(router_to_broker);
    ipc::PortId esout_port_id = router->add_port(router_to_esout);
    auto* broker_chan = new rpc::Channel(broker_port_id, broker_to_router);
    auto* esout_chan = new rpc::Channel(esout_port_id, esout_to_router);

    std::thread router_thread([router]() {
        ipc::PortError error = router->loop();

        // If we reached here an unrecoverable error occured
        if (error == ipc::PortError::PollError)
            throw std::runtime_error("ipc router: polling error");
        else if (error == ipc::PortError::BadFileDescriptor)
            throw std::runtime_error("ipc router: bad file descriptor");
        else
            throw std::runtime_error("ipc router: Unknown error");
    });

    std::thread esout_thread([esout_chan]() {
        esout_chan->loop();
    });

    // Add the rpc objects to the vlc instance
    var_Create(libvlc, "rpc-broker-router", VLC_VAR_ADDRESS);
    var_Create(libvlc, "rpc-broker-portid", VLC_VAR_INTEGER);
    var_Create(libvlc, "rpc-broker-channel", VLC_VAR_ADDRESS);
    var_Create(libvlc, "rpc-esout-channel", VLC_VAR_ADDRESS);
    var_Create(libvlc, "rpc-esout-portid", VLC_VAR_INTEGER);

    var_SetAddress(libvlc, "rpc-broker-router", router);
    var_SetInteger(libvlc, "rpc-broker-portid", broker_port_id);
    var_SetAddress(libvlc, "rpc-broker-channel", broker_chan);
    var_SetInteger(libvlc, "rpc-esout-portid", esout_port_id);
    var_SetAddress(libvlc, "rpc-esout-channel", esout_chan);

    router_thread.detach();
    esout_thread.detach();

    return 0;
}

void vlc_rpc_ProxifyStream(stream_t* local, remote_stream_t* remote, rpc::Channel* chan)
{
    vlc_stream_proxy_objects* proxies = new vlc_stream_proxy_objects;

    proxies->object_proxy = chan->connect<vlc::StreamProxy>(remote->port, remote->object_id);
    proxies->control_proxy = chan->connect<vlc::StreamControlProxy>(remote->port, remote->control_id);
    local->p_sys = proxies;

    // Install hooks
    local->pf_read = static_cast<decltype(stream_t::pf_read)>(vlc_RemoteStream_Read);
    local->pf_block = static_cast<decltype(stream_t::pf_block)>(vlc_RemoteStream_Block);
    local->pf_readdir = static_cast<decltype(stream_t::pf_readdir)>(vlc_RemoteStream_Readdir);
    local->pf_demux = static_cast<decltype(stream_t::pf_demux)>(vlc_RemoteStream_Demux);
    local->pf_seek = static_cast<decltype(stream_t::pf_seek)>(vlc_RemoteStream_Seek);
    local->pf_control = static_cast<decltype(stream_t::pf_control)>(vlc_RemoteStream_Control);
}

int vlc_broker_CreateAccess(stream_t* s)
{
    std::lock_guard<std::mutex> lock(remote_stream_lock_);
    auto* broker_channel = static_cast<rpc::Channel*>(var_InheritAddress(s, "rpc-broker-channel"));
    auto* access_factory_ptr = static_cast<rpc::Proxy<vlc::StreamFactoryProxy>*>(var_InheritAddress(s, "rpc-broker-access_factory"));

    if (!access_factory_ptr)
    {
        libvlc_int_t* libvlc = vlc_object_instance(s);
        ipc::PortId access_factory_port = vlc_broker_CreateProcess(libvlc, "vlc-access-service");

        if (!access_factory_port)
            return -1;

        // We connect to the remote process ObjectFactory
        auto factory = broker_channel->connect<vlc::StreamFactoryProxy>(access_factory_port, 0);
        access_factory_ptr = new rpc::Proxy<vlc::StreamFactoryProxy>(std::move(factory));

        // Get the root object and register the access factory
        var_Create(libvlc, "rpc-broker-access_factory", VLC_VAR_ADDRESS);
        var_Create(libvlc, "rpc-broker-access_factory_port", VLC_VAR_INTEGER);

        var_SetAddress(libvlc, "rpc-broker-access_factory", access_factory_ptr);
        var_SetInteger(libvlc, "rpc-broker-access_factory_port", access_factory_port);
    }

    // Now we need to create the access object itself
    auto& access_factory = *access_factory_ptr;
    ipc::PortId access_factory_port = var_InheritInteger(s, "rpc-broker-access_factory_port");

    rpc::ObjectId access_object = 0;
    rpc::ObjectId control_object = 0;

    if (!access_factory->create(s->psz_url, s->b_preparsing, &access_object, &control_object))
        return -1;

    std::printf("[BROKER] Created access for url: %s (port: %lu, object id: %lu)\n",
            s->psz_url, access_factory_port, access_object);
    std::printf("[BROKER] Created control for access #%lu (object id: %lu)\n",
            access_object, control_object);

    remote_stream_t remote_info;
    remote_info.port = access_factory_port;
    remote_info.object_id = access_object;
    remote_info.control_id = control_object;

    // Install the rpc proxies on the current channel and bind them to the C object.
    vlc_rpc_ProxifyStream(s, &remote_info, broker_channel);

    return 0;
}

// EsOut proxification
namespace
{

struct vlc_es_out_proxy_object
{
    struct es_out_t out;
    rpc::Proxy<vlc::EsOutProxy> object;
};

es_out_id_t* vlc_RemoteEsOut_Add(es_out_t* out, input_source_t*, const es_format_t* fmt)
{
    auto* esout = reinterpret_cast<vlc_es_out_proxy_object*>(out);
    auto& remote = esout->object;
    std::uint64_t fake_es_out_id = 0;

    if (!fmt)
    {
        std::printf("[ESOUT-PROXY] Add(<empty fmt>)\n");
        if (!remote->add({}, &fake_es_out_id))
            return NULL;

        return reinterpret_cast<es_out_id_t*>(fake_es_out_id);
    }

    // Convert es_format_t into vlc::EsFormat
    vlc::EsFormat out_fmt;

    out_fmt.raw_struct.resize(sizeof(*fmt));
    std::memcpy(out_fmt.raw_struct.data(), fmt, sizeof(*fmt));

    if (fmt->p_extra_languages && fmt->i_extra_languages > 0)
    {
        std::vector<std::uint8_t> extra_languages;
        extra_languages.resize(fmt->i_extra_languages);
        std::memcpy(extra_languages.data(), fmt->p_extra_languages, extra_languages.size());

        out_fmt.extra_languages = std::move(extra_languages);
    }

    if (fmt->p_extra && fmt->i_extra > 0)
    {
        std::vector<std::uint8_t> extra;
        extra.resize(fmt->i_extra);
        std::memcpy(extra.data(), fmt->p_extra, extra.size());

        out_fmt.extra = std::move(extra);
    }

    std::printf("[ESOUT-PROXY] Add(<some fmt>)\n");

    if (!remote->add(out_fmt, &fake_es_out_id))
        return NULL;

    std::printf("[ESOUT-PROXY] Received fake_es_out_id: %lu\n", fake_es_out_id);

    return reinterpret_cast<es_out_id_t*>(fake_es_out_id);
}

int vlc_RemoteEsOut_Send(es_out_t* out, es_out_id_t* id, block_t* block)
{
    auto* esout = reinterpret_cast<vlc_es_out_proxy_object*>(out);
    auto& remote = esout->object;
    std::uint64_t fake_es_out_id = reinterpret_cast<std::uint64_t>(id);
    std::int32_t ret = VLC_EGENERIC;

    if (!block)
    {
        std::printf("[ESOUT-PROXY] Send(<empty block>)\n");
        remote->send(fake_es_out_id, {}, &ret);
        return ret;
    }

    vlc::EsBlock out_block;
    std::vector<std::uint8_t> data(block->p_buffer, block->p_buffer + block->i_buffer);

    out_block.buffer = std::move(data);
    out_block.flags = block->i_flags;
    out_block.nb_samples = block->i_nb_samples;
    out_block.pts = block->i_pts;
    out_block.dts = block->i_dts;
    out_block.length = block->i_length;

    std::printf("[ESOUT-PROXY] Send(block size=%lu)\n", block->i_length);
    remote->send(fake_es_out_id, out_block, &ret);
    return ret;
}

void vlc_RemoteEsOut_Del(es_out_t* out, es_out_id_t* id)
{
    auto* esout = reinterpret_cast<vlc_es_out_proxy_object*>(out);
    auto& remote = esout->object;
    std::uint64_t fake_es_out_id = reinterpret_cast<std::uint64_t>(id);

    std::printf("[ESOUT-PROXY] Del(es_out_id = %lu)\n", fake_es_out_id);
    remote->del(reinterpret_cast<std::uint64_t>(id));
}

int vlc_RemoteEsOut_Control(es_out_t*, input_source_t*, int query, va_list)
{
    std::printf("[ESOUT-PROXY] Stubbed control command = %i\n", query);
    return VLC_EGENERIC;
}

void vlc_RemoteEsOut_Destroy(es_out_t* out)
{
    std::printf("[ESOUT-PROXY] Destroy()\n");
    auto* esout = reinterpret_cast<vlc_es_out_proxy_object*>(out);
    auto& remote = esout->object;
    remote->destroy();
}

int vlc_RemoteEsOut_PrivControl(es_out_t*, int query, va_list)
{
    std::printf("[ESOUT-PROXY] Stubbed priv_control command = %i\n", query);
    return VLC_EGENERIC;
}

struct es_out_callbacks es_out_proxy_cbs =
{
    vlc_RemoteEsOut_Add,
    vlc_RemoteEsOut_Send,
    vlc_RemoteEsOut_Del,
    vlc_RemoteEsOut_Control,
    vlc_RemoteEsOut_Destroy,
    vlc_RemoteEsOut_PrivControl
};

}

es_out_t* vlc_rpc_ProxifyEsOut(remote_esout_t* remote, rpc::Channel* chan)
{
    vlc_es_out_proxy_object* out = new vlc_es_out_proxy_object;
    out->out.cbs = &es_out_proxy_cbs;
    out->object = chan->connect<vlc::EsOutProxy>(remote->port, remote->object_id);

    return reinterpret_cast<es_out_t*>(out);
}

// Demux proxification
namespace
{

int vlc_RemoteDemux_Readdir(stream_t*, input_item_node_t*)
{
    std::printf("[DEMUX-PROXY] Stubbed Readdir\n");
    return VLC_EGENERIC;
}

int vlc_RemoteDemux_Demux(stream_t* s)
{
    std::printf("[DEMUX-PROXY] Demux()\n");
    return VLC_EGENERIC;
}

int vlc_RemoteDemux_Control(stream_t*, int query, va_list)
{
    std::printf("[DEMUX-PROXY] Stubbed control %i\n", query);
    return VLC_EGENERIC;
}

}

void vlc_rpc_ProxifyDemux(demux_t* local, remote_demux_t* remote, rpc::Channel* chan)
{
    vlc_rpc_ProxifyStream(local->s, &remote->stream, chan);
    local->out = vlc_rpc_ProxifyEsOut(&remote->esout, chan);

    local->pf_readdir = static_cast<decltype(stream_t::pf_readdir)>(vlc_RemoteDemux_Readdir);
    local->pf_demux = static_cast<decltype(stream_t::pf_demux)>(vlc_RemoteDemux_Demux);
    local->pf_control = static_cast<decltype(stream_t::pf_control)>(vlc_RemoteDemux_Control);
}


int vlc_broker_CreateDemux(demux_t* demux, const char* module)
{
    // Step 1: Create a esout rpc object for the demux.
    auto* esout_channel = reinterpret_cast<rpc::Channel*>(var_InheritAddress(demux, "rpc-esout-channel"));
    ipc::PortId esout_port = var_InheritInteger(demux, "rpc-esout-portid");
    rpc::ObjectId esout_id = esout_channel->bind<EsOut>(demux->out);

    // Step 2: Recover access rpc objects
    vlc_stream_proxy_objects* stream_proxies = stream_get_access_proxies(demux->s);

    if (!stream_proxies)
    {
        std::printf("[BROKER] Trying to create demux using non proxyfied access\n");
        return -1;
    }

    rpc::Proxy<vlc::StreamProxy>& stream_proxy = stream_proxies->object_proxy;
    rpc::Proxy<vlc::StreamControlProxy> control_proxy = stream_proxies->control_proxy;

    std::printf("[BROKER] Using remote stream [object=(%lu,%lu), control=(%lu,%lu)] for demux\n",
            stream_proxy->remote_port(),
            stream_proxy->remote_id(),
            control_proxy->remote_port(),
            control_proxy->remote_id());

    // Step 3: Initialize remote demux factory.
    auto* broker_channel = static_cast<rpc::Channel*>(var_InheritAddress(demux, "rpc-broker-channel"));
    auto* demux_factory_ptr = static_cast<rpc::Proxy<vlc::DemuxFactoryProxy>*>(var_InheritAddress(demux, "rpc-broker-demux_factory"));

    if (!demux_factory_ptr)
    {
        libvlc_int_t* libvlc = vlc_object_instance(demux);
        ipc::PortId demux_factory_port = vlc_broker_CreateProcess(libvlc, "vlc-demux-service");

        if (!demux_factory_port)
            return -1;

        auto factory = broker_channel->connect<vlc::DemuxFactoryProxy>(demux_factory_port, 0);
        demux_factory_ptr = new rpc::Proxy<vlc::DemuxFactoryProxy>(std::move(factory));

        // Register the factory
        var_Create(libvlc, "rpc-broker-demux_factory", VLC_VAR_ADDRESS);
        var_Create(libvlc, "rpc-broker-demux_factory_port", VLC_VAR_INTEGER);

        var_SetAddress(libvlc, "rpc-broker-demux_factory", demux_factory_ptr);
        var_SetInteger(libvlc, "rpc-broker-demux_factory_port", demux_factory_port);
    }

    auto& demux_factory = *demux_factory_ptr;
    ipc::PortId demux_factory_port = var_InheritInteger(demux, "rpc-broker-demux_factory_port");

    // Step 4: Serialize partial demux and create remote object.
    vlc::RemoteAccess remote_access = { stream_proxy->remote_id(), stream_proxy->remote_port() };
    vlc::RemoteControl remote_control = { control_proxy->remote_id(), control_proxy->remote_port() };
    vlc::RemoteEsOut remote_esout = { esout_id, esout_port };

    rpc::ObjectId demux_object = 0;

    if (!demux_factory->create(remote_access, remote_control, remote_esout, module, demux->b_preparsing, &demux_object))
    {
        std::printf("[BROKER] Call to DemuxFactory::create(...) failed\n");
        return -1;
    }

    // Step 5: Proxify the passed demux object
    // TODO: Cleanup, this is a bit repetitive
    remote_stream_t rs = { remote_access.object_id, remote_control.object_id, remote_access.port };
    remote_esout_t re = { remote_esout.object_id, remote_esout.port };
    remote_demux_t rd;
    rd.stream = rs;
    rd.esout = re;

    vlc_rpc_ProxifyDemux(demux, &rd, broker_channel);

    return 0;
}
