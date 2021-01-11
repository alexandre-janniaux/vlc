#ifndef RPC_VLC_ESOUT_HH
#define RPC_VLC_ESOUT_HH

#include <vector>
#include "esout.sidl.hh"

struct es_out_t;
struct es_out_id_t;

class EsOut : public vlc::EsOutReceiver
{
public:
    EsOut(es_out_t* esout)
        : esout_(esout)
    {}

    bool add(std::optional<vlc::EsFormat> fmt, std::uint64_t* fake_es_out_id) override;
    bool send(std::uint64_t fake_es_out_id, std::optional<vlc::EsBlock> block, std::int32_t* ret) override;
    bool del(std::uint64_t fake_es_out_id) override;
    bool destroy();

    // Control part
    bool control_set_pcr(std::int64_t i_pcr, std::int64_t* status) override;
    bool control_set_next_display_time(std::int64_t i_pts, std::int64_t* status) override;

private:
    es_out_t* esout_;
    std::vector<es_out_id_t*> esout_ids_;
};

#endif
