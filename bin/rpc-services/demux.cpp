#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_demux.h>

#include <cstdio>

#include "demux.hh"

bool Demux::demux(std::int32_t* result)
{
    *result = demux_Demux(demux_);
    return true;
}
