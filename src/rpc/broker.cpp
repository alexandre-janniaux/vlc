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
#include "vlc_rpc/objectfactory.sidl.hh"
#include "vlc_rpc/stream.sidl.hh"
#include "broker.h"

namespace
{
static ipc::Router* main_router = nullptr;
static rpc::Channel* broker_channel = nullptr;
static rpc::PortId broker_port_id = 0;

rpc::Proxy<vlc::ObjectFactoryProxy> access_factory = nullptr;
rpc::PortId access_factory_port = 0;
std::vector<rpc::Proxy<vlc::StreamProxy>> remote_streams_;
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

rpc::PortId vlc_broker_CreateProcess(const char* program_name)
{
    // 1: Resolve path to the binary (according to program_name)
    // 2: Create unix socketpair (parent - child)
    // 3: Add the ipc::Port to the router and obtain the portid.
    // 4: Fork
    //        - Build the argument list for the child and call execve.
    //          If execve fails then crash (all binary paths should be valid as
    //          they are hardcoded).
    //        - In the parent, return the portid

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

rpc::Proxy<vlc::StreamProxy> get_stream_for_id(size_t id)
{
    for (auto& obj : remote_streams_)
    {
        if (obj->id() == id)
            return obj;
    }

    return nullptr;
}

}

int vlc_broker_Init(void)
{
    ipc::Port router_to_broker;
    ipc::Port broker_to_router;

    if (!ipc::Port::create_pair(router_to_broker, broker_to_router))
        return -1;

    main_router = new ipc::Router();
    broker_port_id = main_router->add_port(router_to_broker);
    broker_channel = new rpc::Channel(broker_port_id, broker_to_router);

    std::thread router_thread([&]() {
        main_router->loop();
    });

    router_thread.detach();

    return 0;
}

int vlc_broker_CreateAccess(const char* url, bool preparse)
{
    std::lock_guard<std::mutex> lock(remote_stream_lock_);

    if (!access_factory)
    {
        access_factory_port = vlc_broker_CreateProcess("vlc-access-service");

        if (!access_factory_port)
            return -1;

        // We connect to the remote process ObjectFactory
        access_factory = broker_channel->connect<vlc::ObjectFactoryProxy>(access_factory_port, 0);
    }

    // Now we need to create the access object itself
    rpc::ObjectId access_obj = 0;
    std::vector<std::string> args = {
        url,
        (preparse) ? "preparse" : "nopreparse"
    };

    if (!access_factory->create("access", args, &access_obj))
        return -1;

    std::printf("[BROKER] Created access for url: %s (port: %lu, object id: %lu)\n",
            url, access_factory_port, access_obj);

    // We create the proxy and store the id for future use
    auto proxy = broker_channel->connect<vlc::StreamProxy>(access_factory_port, access_obj);
    remote_streams_.push_back(proxy);

    return proxy->id();
}

/*
 * Stream apis.
 */
ssize_t vlc_RemoteStream_Read(stream_t* s, void* buf, size_t len)
{
    std::lock_guard<std::mutex> lock(remote_stream_lock_);
    auto stream = get_stream_for_id(s->object_id);

    if (!stream)
        return -1;

    std::printf("[PROXY] Read request to #%lu\n", s->object_id);
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
    auto stream = get_stream_for_id(s->object_id);

    if (!stream)
        return -1;

    std::printf("[PROXY] Seek request to #%lu\n", s->object_id);
    int32_t status = 0;

    if (!stream->seek(offset, &status))
        return -1;

    return status;
}

block_t* vlc_RemoteStream_Block(stream_t* s, bool* out_eof)
{
    std::lock_guard<std::mutex> lock(remote_stream_lock_);
    auto stream = get_stream_for_id(s->object_id);

    if (!stream)
        return NULL;

    std::printf("[PROXY] Block #%lu\n", s->object_id);
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
    auto stream = get_stream_for_id(s->object_id);

    if (!stream)
        return;

    std::printf("[PROXY] Destroy #%lu\n", s->object_id);
    stream->destroy();
}

int vlc_RemoteStream_Readdir(stream_t* s, input_item_node_t*)
{
    std::printf("[PROXY] Stream #%lu tried to call Readdir\n", s->object_id);
    return -1;
}

int vlc_RemoteStream_Demux(stream_t* s)
{
    std::printf("[PROXY] Stream #%lu tried to call Demux\n", s->object_id);
    return -1;
}

int vlc_RemoteStream_Control(stream_t* s, int cmd, va_list args)
{
    // Control commands used when reading the mp3 file
    // 0    : STREAM_CAN_SEEK
    // 1    : STREAM_CAN_FASTSEEK
    // 2    : STREAM_CAN_PAUSE
    // 3    : STREAM_CAN_CONTROL_PACE
    // 6    : STREAM_GET_SIZE
    // 257  : STREAM_GET_PTS_DELAY
    // 261  : STREAM_GET_META
    // 262  : STREAM_GET_CONTENT_TYPE
    // 263  : STREAM_GET_SIGNAL
    // 264  : STREAM_GET_TAGS
    // 265  : STREAM_GET_TYPE
    std::printf("[PROXY] Stubbed pf_control called by Stream #%lu (cmd: %i)\n", s->object_id, cmd);

    switch (cmd)
    {
        case STREAM_CAN_SEEK:
        case STREAM_CAN_FASTSEEK:
        case STREAM_CAN_PAUSE:
        case STREAM_CAN_CONTROL_PACE:
            *va_arg( args, bool * ) = true;
            break;
        default:
            return VLC_EGENERIC;
    };

    return VLC_SUCCESS;
}
