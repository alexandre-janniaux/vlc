#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_stream.h>

#include <cstdio>
#include "access.hh"

bool Access::read(std::uint64_t length, std::int64_t* status, std::vector<std::uint8_t>* buffer)
{
    buffer->resize(length);
    *status = vlc_stream_Read(access_, buffer->data(), buffer->size());
    std::printf("[ACCESS] Read(length=%lu) = %li\n", length, *status);

    return true;
}

bool Access::block(std::uint8_t* eof, std::optional<vlc::Block>* result_block)
{
    block_t* block = vlc_stream_ReadBlock(access_);

    if (block)
    {
        vlc::Block tmp_block;
        std::vector<std::uint8_t> data(block->p_buffer, block->p_buffer + block->i_buffer);

        tmp_block.buffer = std::move(data);
        tmp_block.flags = block->i_flags;
        tmp_block.nb_samples = block->i_nb_samples;
        tmp_block.pts = block->i_pts;
        tmp_block.dts = block->i_dts;
        tmp_block.length = block->i_length;

        *result_block = std::move(tmp_block);
        block_Release(block);
    }
    else
    {
        *result_block = std::nullopt;
    }

    *eof = vlc_stream_Eof(access_);

    std::printf("[ACCESS] Block(); eof = %u\n", *eof);

    return true;
}

bool Access::seek(std::uint64_t offset, std::int32_t* status)
{
    *status = vlc_stream_Seek(access_, offset);
    std::printf("[ACCESS] Seed(offset=%lu) = %i\n", offset, *status);
    return true;
}

bool Access::destroy()
{
    return true;
}

bool Access::control_can_seek(std::int64_t* status, bool* result)
{
    *status = vlc_stream_Control(access_, STREAM_CAN_SEEK, result);

    std::printf("[ACCESS] control can_seek(result=%i) = %li\n", *result, *status);
    return true;
}

bool Access::control_can_fastseek(std::int64_t* status, bool* result)
{
    *status = vlc_stream_Control(access_, STREAM_CAN_FASTSEEK, result);
    std::printf("[ACCESS] control can_fastseek(result=%i) = %li\n", *result, *status);
    return true;
}

bool Access::control_can_pause(std::int64_t* status, bool* result)
{
    *status = vlc_stream_Control(access_, STREAM_CAN_PAUSE, result);
    std::printf("[ACCESS] control can_pause(result=%i) = %li\n", *result, *status);
    return true;
}

bool Access::control_can_control_pace(std::int64_t* status, bool* result)
{
    *status = vlc_stream_Control(access_, STREAM_CAN_CONTROL_PACE, result);
    std::printf("[ACCESS] control can_control_pace(result=%i) = %li\n", *result, *status);
    return true;
}

bool Access::control_get_size(std::int64_t* status, std::uint64_t* result)
{
    *status = vlc_stream_Control(access_, STREAM_GET_SIZE, result);
    std::printf("[ACCESS] control get_size(result=%lu) = %li\n", *result, *status);
    return true;
}

bool Access::control_get_pts_delay(std::int64_t* status, std::int64_t* result)
{
    *status = vlc_stream_Control(access_, STREAM_GET_PTS_DELAY, result);
    std::printf("[ACCESS] control get_pts_delay(result=%li) = %li\n", *result, *status);
    return true;
}

bool Access::control_set_pause_state(bool state, std::int64_t* status)
{
    *status = vlc_stream_Control(access_, STREAM_SET_PAUSE_STATE, state);
    std::printf("[ACCESS] control set_pause_state(%i) = %li\n", state, *status);
    return true;
}
