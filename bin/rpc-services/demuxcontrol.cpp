#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_demux.h>

#include <cstdio>
#include "demuxcontrol.hh"

bool DemuxControl::can_seek(std::int64_t* status, bool* result)
{
    *status = vlc_stream_Control(demux_, DEMUX_CAN_SEEK, result);

    std::printf("[DEMUX-CONTROL] can_seek(result=%i) = %li\n", *result, *status);
    return true;
}

bool DemuxControl::can_pause(std::int64_t* status, bool* result)
{
    *status = vlc_stream_Control(demux_, DEMUX_CAN_PAUSE, result);
    std::printf("[DEMUX-CONTROL] can_pause(result=%i) = %li\n", *result, *status);
    return true;
}

bool DemuxControl::can_control_pace(std::int64_t* status, bool* result)
{
    *status = vlc_stream_Control(demux_, DEMUX_CAN_CONTROL_PACE, result);

    std::printf("[DEMUX-CONTROL] can_control_pace(result=%i) = %li\n", *result, *status);
    return true;
}

bool DemuxControl::get_pts_delay(std::int64_t* status, std::int64_t* result)
{
    *status = vlc_stream_Control(demux_, DEMUX_GET_PTS_DELAY, result);
    std::printf("[DEMUX-CONTROL] get_pts_delay(result=%li) = %li\n", *result, *status);
    return true;
}

bool DemuxControl::set_pause_state(bool state, std::int64_t* status)
{
    *status = vlc_stream_Control(demux_, DEMUX_SET_PAUSE_STATE, state);
    std::printf("[DEMUX-CONTROL] set_pause_state(%i) = %li\n", state, *status);
    return true;
}
