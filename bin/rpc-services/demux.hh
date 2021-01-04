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

private:
    demux_t* demux_;
};

#endif
