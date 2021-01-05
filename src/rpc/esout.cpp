#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_block.h>
#include <vlc_es.h>
#include <vlc_es_out.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "esout.hh"

bool EsOut::add(std::optional<vlc::EsFormat> fmt, std::uint64_t* fake_es_out_id)
{
    struct es_format_t c_fmt;

    if (fmt->raw_struct.size() != sizeof(c_fmt))
    {
        std::printf("[EsOut] Received fmt struct has wrong size (%lu instead of %lu)\n",
                fmt->raw_struct.size(), sizeof(c_fmt));
        return false;
    }

    std::memcpy(&c_fmt, fmt->raw_struct.data(), sizeof(c_fmt));
    c_fmt.i_extra_languages = 0;
    c_fmt.p_extra_languages = nullptr;
    c_fmt.i_extra = 0;
    c_fmt.p_extra = nullptr;

    if (fmt->extra_languages)
    {
        c_fmt.i_extra_languages = fmt->extra_languages->size();
        c_fmt.p_extra_languages = reinterpret_cast<extra_languages_t*>(std::malloc(c_fmt.i_extra_languages));
        std::memcpy(c_fmt.p_extra_languages, fmt->extra_languages->data(), c_fmt.i_extra_languages);
    }

    if (fmt->extra)
    {
        c_fmt.i_extra = fmt->extra->size();
        c_fmt.p_extra = std::malloc(c_fmt.i_extra);
        std::memcpy(c_fmt.p_extra, fmt->extra->data(), c_fmt.i_extra);
    }

    es_out_id_t* id = es_out_Add(esout_,  &c_fmt);

    // Free the format
    if (c_fmt.p_extra_languages)
        std::free(c_fmt.p_extra_languages);

    if (c_fmt.p_extra)
        std::free(c_fmt.p_extra);

    if (!id)
    {
        *fake_es_out_id = 0;
        return true;
    }

    // We give an id > 0 as 0 would otherwise be considered as a null pointer
    *fake_es_out_id = esout_ids_.size() + 1;
    esout_ids_.push_back(id);

    return true;
}

bool EsOut::send(std::uint64_t fake_es_out_id, std::optional<vlc::EsBlock> block, std::int32_t* ret)
{
    // Get the es_out_id
    if (fake_es_out_id == 0 || fake_es_out_id > esout_ids_.size() || !esout_ids_[fake_es_out_id - 1])
    {
        *ret = VLC_EGENERIC;
        return true;
    }

    es_out_id_t* esout_id = esout_ids_[fake_es_out_id - 1];

    if (!block)
    {
        *ret = es_out_Send(esout_, esout_id, nullptr);
        return true;
    }

    block_t* result = block_Alloc(block->buffer.size());
    result->i_flags = block->flags;
    result->i_nb_samples = block->nb_samples;
    result->i_pts = block->pts;
    result->i_dts = block->dts;
    result->i_length = block->length;
    std::memcpy(result->p_buffer, block->buffer.data(), block->buffer.size());

    *ret = es_out_Send(esout_, esout_id, result);

    return true;
}

bool EsOut::del(std::uint64_t fake_es_out_id)
{
    if (fake_es_out_id == 0 || fake_es_out_id > esout_ids_.size() || !esout_ids_[fake_es_out_id - 1])
        return true;

    es_out_Del(esout_, esout_ids_[fake_es_out_id - 1]);
    esout_ids_[fake_es_out_id - 1] = nullptr;

    return true;
}

bool EsOut::destroy()
{
    es_out_Delete(esout_);
    return true;
}

// Control

bool EsOut::control_set_pcr(std::int64_t i_pcr, std::int64_t* status)
{
    *status = es_out_SetPCR(esout_, i_pcr);
    std::printf("[ESOUT-CONTROL] SetPcr(%li) = %li\n", i_pcr, *status);
    return true;
}
