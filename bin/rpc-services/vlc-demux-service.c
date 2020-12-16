#include "../lib/libvlc_internal.h"
#include <vlc/vlc.h>

#include "demuxfactory.hh"

/*
 * what we need:
 *
 * 1: the file descriptor for the channel
 * 2: the current process port id
 * 2: the port id for the broker (if we need to contact the broker)
 * 3: the object id for the broker object (if we need to contact the broker)
 *
 * vlc-demux-service <channel fd> <port id> <broker port id> <broker object id>
 */

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        printf("usage: %s <channel fd> <port id>\n", argv[0]);
        return 1;
    }

#ifdef TOP_BUILDDIR
    setenv ("VLC_PLUGIN_PATH", TOP_BUILDDIR"/modules", 1);
    setenv ("VLC_DATA_PATH", TOP_SRCDIR"/share", 1);
    setenv ("VLC_LIB_PATH", TOP_BUILDDIR"/modules", 1);
#endif

    // XXX: This will break if we use the full range of values as port id's are u64.
    int channel_fd = atoi(argv[1]);
    int port_id = atoi(argv[2]);

    start_factory(channel_fd, port_id);

    return 0;
}
