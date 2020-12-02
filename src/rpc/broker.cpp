#include <thread>
#include <filesystem>
#include <charconv>
#include <cstdio>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "protoipc/router.hh"
#include "protorpc/channel.hh"
#include "broker.h"
#include "vlc_rpc/objectfactory.sidl.hh"

namespace
{
static ipc::Router* main_router = nullptr;
static rpc::Channel* broker_channel = nullptr;
static rpc::PortId broker_port_id = 0;

rpc::Proxy<vlc::ObjectFactoryProxy> access_factory = nullptr;
rpc::PortId access_factory_port = 0;

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

    int socks[2];

    if (socketpair(AF_UNIX, SOCK_DGRAM, 0, socks) == -1)
        return 0; // Port cannot be 0 as it is the broker

    // We don't want child processes to inherit the broker's fds.
    int child_fd = fcntl(socks[0], F_DUPFD_CLOEXEC, 0);

    if (child_fd == -1)
        return 0;

    close(socks[0]);

    ipc::Port broker_to_child(child_fd);
    ipc::Port child_to_broker(socks[1]);

    rpc::PortId child_port_id = main_router->add_port(broker_to_child);

    pid_t pid = fork();

    if (pid == -1)
        return 0;

    if (pid == 0)
    {
        char fd_str[32] = {0};
        char channel_port_str[32] = {0};

        std::to_chars(fd_str, fd_str + sizeof(fd_str), socks[1]);
        std::to_chars(channel_port_str, channel_port_str + sizeof(channel_port_str), child_port_id);

        execl(bin_path.c_str(),
              bin_path.c_str(),
              fd_str,
              channel_port_str,
              0);

        throw std::runtime_error("execl failed");
    }

    close(socks[1]);

    return child_port_id;
}
}

int vlc_broker_Init(void)
{
    int broker_socks[2];
    int err = socketpair(AF_UNIX, SOCK_DGRAM, 0, broker_socks);

    if (err == -1)
        return -1;

    main_router = new ipc::Router();
    broker_port_id = main_router->add_port(ipc::Port(broker_socks[0]));
    broker_channel = new rpc::Channel(broker_port_id, ipc::Port(broker_socks[1]));

    std::thread router_thread([&]() {
        main_router->loop();
    });

    router_thread.detach();

    return 0;
}

int vlc_broker_CreateAccess(const char* url, bool preparse)
{
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

    return 0;
}
