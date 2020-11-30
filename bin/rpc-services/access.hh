#ifndef RPC_VLC_ACCESS_HH
#define RPC_VLC_ACCESS_HH

#include "vlc_rpc/stream.sidl.hh"

struct stream_t;

class Access: public vlc::StreamReceiver
{
public:
    Access(rpc::Channel* chan, std::uint64_t object_id, stream_t* access)
        : vlc::StreamReceiver(chan, object_id), access_(access)
    {}

    bool read(std::size_t length, std::int64_t* status, std::vector<std::uint8_t>* buffer) override;
    bool block(std::uint8_t* eof, std::optional<vlc::Block>* block) override;
    bool seek(std::uint64_t offset, std::int32_t* status) override;
    bool destroy() override;

private:
    stream_t* access_;
};

#endif
