#ifndef RPC_VLC_DEMUX_HH
#define RPC_VLC_DEMUX_HH

#include "demux.sidl.hh"

struct stream_t;
typedef struct stream_t demux_t;

class Demux : public vlc::DemuxReceiver
{
public:
    Demux(demux_t* demux)
        : demux_(demux)
    {}

    bool demux(std::int32_t* result) override;

    // Control part
    bool control_can_seek(std::int64_t* status, bool* result) override;
    bool control_can_pause(std::int64_t* status, bool* result) override;
    bool control_can_control_pace(std::int64_t* status, bool* result) override;
    bool control_get_pts_delay(std::int64_t* status, std::int64_t* result) override;
    bool control_set_pause_state(bool state, std::int64_t* status) override;
    bool control_test_and_clear_flags(std::uint32_t in_flags, std::int64_t* status, std::uint32_t* out_flags) override;
    bool control_get_length(std::int64_t* status, std::int64_t* ticks) override;
    bool control_get_time(std::int64_t* status, std::int64_t* ticks) override;
    bool control_get_normal_time(std::int64_t* status, std::int64_t* ticks) override;

private:
    demux_t* demux_;
};

#endif
