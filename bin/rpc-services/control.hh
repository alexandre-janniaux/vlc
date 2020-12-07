#ifndef RPC_VLC_CONTROL_HH
#define RPC_VLC_CONTROL_HH

#include "vlc_rpc/streamcontrol.sidl.hh"

struct stream_t;

class Control : public vlc::StreamControlReceiver
{
public:
    Control(stream_t* access)
        : access_(access)
    {}

    bool can_seek(std::int64_t* status, bool* result) override;
    bool can_fastseek(std::int64_t* status, bool* result) override;
    bool can_pause(std::int64_t* status, bool* result) override;
    bool can_control_pace(std::int64_t* status, bool* result) override;
    bool get_size(std::int64_t* status, std::uint64_t* result) override;
    bool get_pts_delay(std::int64_t* status, std::int64_t* result) override;
    bool set_pause_state(bool state, std::int64_t* status) override;

private:
    stream_t* access_;
};

#endif
