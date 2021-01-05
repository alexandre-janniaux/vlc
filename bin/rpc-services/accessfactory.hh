#ifndef RPC_VLC_ACCESSFACTORY_HH
#define RPC_VLC_ACCESSFACTORY_HH

#ifdef __cplusplus
extern "C" {
#endif
void start_factory(int channel_fd, int port_id);
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include "streamfactory.sidl.hh"

struct libvlc_instance_t;

class AccessFactory: public vlc::StreamFactoryReceiver
{
public:
    AccessFactory(rpc::Channel* chan);
    bool create(std::string url, bool preparsing, std::uint64_t* stream_object) override;

private:
    rpc::Channel* channel_;
    libvlc_instance_t* vlc_instance_;
};

#endif
#endif
