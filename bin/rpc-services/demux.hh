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

private:
    demux_t* demux_;
};

#endif
