#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_stream.h>

#include <cstdio>
#include "control.hh"

bool Control::can_seek(std::int64_t* status, bool* result)
{
    *status = vlc_stream_Control(access_, STREAM_CAN_SEEK, result);

    std::printf("[CONTROL] can_seek(result=%i) = %li\n", *result, *status);
    return true;
}

bool Control::can_fastseek(std::int64_t* status, bool* result)
{
    *status = vlc_stream_Control(access_, STREAM_CAN_FASTSEEK, result);
    std::printf("[CONTROL] can_fastseek(result=%i) = %li\n", *result, *status);
    return true;
}

bool Control::can_pause(std::int64_t* status, bool* result)
{
    *status = vlc_stream_Control(access_, STREAM_CAN_PAUSE, result);
    std::printf("[CONTROL] can_pause(result=%i) = %li\n", *result, *status);
    return true;
}

bool Control::can_control_pace(std::int64_t* status, bool* result)
{
    *status = vlc_stream_Control(access_, STREAM_CAN_CONTROL_PACE, result);
    std::printf("[CONTROL] can_control_pace(result=%i) = %li\n", *result, *status);
    return true;
}

bool Control::get_size(std::int64_t* status, std::uint64_t* result)
{
    *status = vlc_stream_Control(access_, STREAM_GET_SIZE, result);
    std::printf("[CONTROL] get_size(result=%lu) = %li\n", *result, *status);
    return true;
}

bool Control::get_pts_delay(std::int64_t* status, std::int64_t* result)
{
    *status = vlc_stream_Control(access_, STREAM_GET_PTS_DELAY, result);
    std::printf("[CONTROL] get_pts_delay(result=%li) = %li\n", *result, *status);
    return true;
}

bool Control::set_pause_state(bool state, std::int64_t* status)
{
    *status = vlc_stream_Control(access_, STREAM_SET_PAUSE_STATE, state);
    std::printf("[CONTROL] set_pause_state(%i) = %li\n", state, *status);
    return true;
}
