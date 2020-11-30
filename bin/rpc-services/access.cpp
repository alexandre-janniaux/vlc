#include "access.hh"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_stream.h>

bool Access::read(std::uint64_t length, std::int64_t* status, std::vector<std::uint8_t>* buffer)
{
    buffer->resize(length);
    *status = vlc_stream_Read(access_, buffer->data(), buffer->size());

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

        *eof = vlc_stream_Eof(access_);
        *result_block = std::move(tmp_block);
        block_Release(block);
    }
    else
    {
        *result_block = std::nullopt;
    }

    *eof = vlc_stream_Eof(access_);

    return true;
}

bool Access::seek(std::uint64_t offset, std::int32_t* status)
{
    *status = vlc_stream_Seek(access_, offset);
    return true;
}

bool Access::destroy()
{
    return true;
}
