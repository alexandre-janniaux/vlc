#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_demux.h>

#include <cstdio>

#include "demux.hh"

Demux::Demux(demux_t* demux)
    : demux_(demux)
{}

bool Demux::demux(std::int32_t* result)
{
    std::printf("[DEMUX] Demux() called\n");
    *result = demux_Demux(demux_);
    return true;
}
