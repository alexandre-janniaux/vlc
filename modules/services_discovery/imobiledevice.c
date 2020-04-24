#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_services_discovery.h>

#include <libimobiledevice/libimobiledevice.h>

struct vlc_sd_imobiledevice
{
};

static void device_event_cb(const idevice_event_t* event, void* userdata)
{
    services_discovery_t *sd = userdata;
    struct vlc_sd_imobiledevice *sys = sd->p_sys;

    if (event->event == IDEVICE_DEVICE_ADD)
    {
        msg_Info(sd, "Apple Device added : %s", event->udid);
    }
    else if (event->event == IDEVICE_DEVICE_REMOVE)
    {
        msg_Info(sd, "Apple Device removed : %s", event->udid);
    }
}

static int Open(vlc_object_t *obj)
{
    services_discovery_t *sd = (services_discovery_t*)obj;
    struct vlc_sd_imobiledevice *sys;

    sys = malloc(sizeof *sys );
    if (sys == NULL)
        return VLC_ENOMEM;

    msg_Info(sd, "IMOBILEDEVICE");

    idevice_event_subscribe(device_event_cb, sd);

    //service_discovery_AddItem();

    sd->p_sys = sys;
    sd->description = _("Apple devices");

    //if(vlc_clone(&sys->thread, Thread, obj, VLC_THREAD_PRIORITY_LOW)
    //        != VLC_SUCCESS)
    //    goto error;

    return VLC_SUCCESS;

error:
    free(sys);
    return VLC_EGENERIC;
}

static void Close(vlc_object_t *obj)
{
    services_discovery_t *sd = (services_discovery_t*)obj;
    struct vlc_sd_imobiledevice *sys = sd->p_sys;

    free(sys);
}

VLC_SD_PROBE_HELPER("imobiledevice", N_("Apple devices"), SD_CAT_DEVICES)

vlc_module_begin()
    set_shortname( "imobiledevice" )
    set_description( N_( "Apple devices" ) )
    set_category( CAT_PLAYLIST )
    set_subcategory( SUBCAT_PLAYLIST_SD )
    set_capability( "services_discovery", 0 )
    set_callbacks( Open, Close )
vlc_module_end()
