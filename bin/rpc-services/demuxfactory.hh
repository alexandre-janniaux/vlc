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
#include "demuxfactory.sidl.hh"

struct libvlc_instance_t;

class DemuxFactory : public vlc::DemuxFactoryReceiver
{
public:
    DemuxFactory(rpc::Channel* chan);
    bool create(vlc::RemoteAccess access, vlc::RemoteControl control, vlc::RemoteEsOut out, std::string module,
            bool preparsing, std::uint64_t* demux_object, std::uint64_t* control_object) override;

private:
    rpc::Channel* channel_;
    libvlc_instance_t* vlc_instance_;
};

#endif
#endif
