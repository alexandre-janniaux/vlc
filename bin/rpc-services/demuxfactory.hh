#ifndef RPC_VLC_DEMUXFACTORY
#define RPC_VLC_DEMUXFACTORY

#ifdef __cplusplus
extern "C" {
#endif
void start_factory(int channel_fd, int port_id);
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include "objectfactory.sidl.hh"

struct libvlc_instance_t;

class DemuxFactory : public vlc::ObjectFactoryReceiver
{
public:
    DemuxFactory(rpc::Channel* chan);
    bool create(std::string type, std::vector<std::string> options, std::vector<std::uint64_t>* receiver_ids) override;

private:
    rpc::Channel* channel_;
    libvlc_instance_t* vlc_instance_;
};

#endif
#endif
