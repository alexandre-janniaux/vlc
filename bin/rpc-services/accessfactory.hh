#ifndef RPC_VLC_ACCESSFACTORY_HH
#define RPC_VLC_ACCESSFACTORY_HH

#include "vlc_rpc/objectfactory.sidl.hh"

class AccessFactory: public vlc::ObjectFactoryReceiver
{
public:
    AccessFactory(rpc::Channel* chan)
        : channel_(chan)
    {}

    bool create(std::string type, std::vector<std::string> options, std::uint64_t* receiver_id) override;

private:
    rpc::Channel* channel_;
};

#endif
