#ifndef RPC_VLC_ACCESS_HH
#define RPC_VLC_ACCESS_HH

#include "stream.sidl.hh"

struct stream_t;

class Access: public vlc::StreamReceiver
{
public:
    Access(stream_t* access)
        : access_(access)
    {}

    bool read(std::size_t length, std::int64_t* status, std::vector<std::uint8_t>* buffer) override;
    bool block(std::uint8_t* eof, std::optional<vlc::Block>* block) override;
    bool seek(std::uint64_t offset, std::int32_t* status) override;
    bool destroy() override;

    // Control part
    bool control_can_seek(std::int64_t* status, bool* result) override;
    bool control_can_fastseek(std::int64_t* status, bool* result) override;
    bool control_can_pause(std::int64_t* status, bool* result) override;
    bool control_can_control_pace(std::int64_t* status, bool* result) override;
    bool control_get_size(std::int64_t* status, std::uint64_t* result) override;
    bool control_get_pts_delay(std::int64_t* status, std::int64_t* result) override;
    bool control_set_pause_state(bool state, std::int64_t* status) override;
    bool control_get_content_type(std::int64_t* status, std::optional<std::string>* type) override;

private:
    stream_t* access_;
};

#endif
