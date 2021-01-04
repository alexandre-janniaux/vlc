#ifndef RPC_VLC_DEMUX_CONTROL_HH
#define RPC_VLC_DEMUX_CONTROL_HH

#include "demuxcontrol.sidl.hh"

struct stream_t;
typedef struct stream_t demux_t;

class DemuxControl : public vlc::DemuxControlReceiver
{
public:
    DemuxControl(demux_t* demux)
        : demux_(demux)
    {}

    bool can_seek(std::int64_t* status, bool* result) override;
    bool can_pause(std::int64_t* status, bool* result) override;
    bool can_control_pace(std::int64_t* status, bool* result) override;
    bool get_pts_delay(std::int64_t* status, std::int64_t* result) override;
    bool set_pause_state(bool state, std::int64_t* status) override;

private:
    demux_t* demux_;
};

#endif
