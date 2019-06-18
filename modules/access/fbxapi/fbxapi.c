#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdint.h>
#include <stdlib.h>

#include <vlc_common.h>
#include <vlc_access.h>
#include <vlc_stream.h>
#include <vlc_plugin.h>
#include <vlc_input_item.h>
#include <vlc_url.h>
#include <vlc_tls.h>

#include "fbxapi_request.h"
#include "fbxapi.h"


static int      fbxapi_connect( vlc_object_t * );
static void     fbxapi_disconnect( vlc_object_t * );

vlc_module_begin()
    set_shortname( "FBXAPI" )
    set_description( N_( "Freebox Api" ) )
    set_capability( "access", 42 )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )
    add_shortcut( "fbxapi", "fbx-api" )
    set_callbacks( fbxapi_connect, fbxapi_disconnect )
vlc_module_end()

//static ssize_t    fbxapi_read( stream_t *, void *, size_t );
//static int        fbxapi_readdir( stream_t *, input_item_node_t * );
static int          fbxapi_seek( stream_t *, uint64_t );

/*static int
fbxapi_rest_connect( stream_t *p_access )
{
    const char	base_get_challenge[] = "/login";
    const char	base_get_login[] = "/login/session";
}*/

static int		fbxapi_open_tls( vlc_object_t *self )
{
    stream_t    *access = (stream_t *)self;
    vlc_url_t   url;
    s_fbxapi    *fbx;

    fbx = access->p_sys;
    if (
        vlc_UrlParse(&url, /*"https://k0bazxqu.fbxos.fr:922"*/access->psz_url)
        || url.psz_host == NULL
        || url.i_port == 0
    )
    {
        msg_Err(access, "invalid location: %s", access->psz_location);
        vlc_UrlClean(&url);
        return VLC_EGENERIC;
    }
    fbx->http.tls = vlc_tls_SocketOpenTCP(self, url.psz_host, url.i_port);
    vlc_UrlClean(&url);
    if ( fbx->http.tls == NULL )
    {
        free(fbx);
        access->p_sys = fbx = NULL;
        return VLC_ENOMEM;
    }
    return VLC_SUCCESS;
}


static int		fbxapi_connect( vlc_object_t *self )
{
    stream_t    *access = (stream_t *)self;
    s_fbxapi    *fbx;
    int         res;

    access->p_sys = fbx = calloc(1, sizeof(*fbx));
    /* Hard coded settings to retrieve later */

    fbx->http.domain = "k0bazxqu.fbxos.fr";
    fbx->http.port = 922;

    /* \\ Hard coded settings to retrieve later */

    if ( fbx == NULL )
    {
        return VLC_ENOMEM;
    }
    res = fbxapi_open_tls(self);
    if ( res != VLC_SUCCESS )
    {
        return res;
    }

    access->pf_control = access_vaDirectoryControlHelper;
    access->pf_seek = fbxapi_seek;

    /*
     *	GET /api/v6/login
     *	Host: any.host
     */
    fbxapi_request(
        fbx,
        "GET",
        "/api/v6/login",
        NULL,
        NULL
    );

    return VLC_SUCCESS;
}

static void		fbxapi_disconnect( vlc_object_t *self )
{
    stream_t    *p_access = (stream_t *)self;
    s_fbxapi    *fbx = p_access->p_sys;

    vlc_tls_SessionDelete(fbx->http.tls);
    free(p_access->p_sys);
    p_access->p_sys = fbx = NULL;
    (void)self;
}

static int		fbxapi_seek( stream_t *p_access, uint64_t u )
{
    (void)p_access;
    (void)u;
    return 1;
}

/*
static int		fbxapi_readdir(
    stream_t *p_access,
    input_item_node_t *current_node
)
{
    char    uri[] = "fbxapi://where:4545?/tmp/path9";
    char    file_name[] = "/tmp/path4";

    struct vlc_readdir_helper   rdh;

    printf(
        "From %s -> '%s' %s\n",
        __FUNCTION__,
        p_access->psz_name,
        p_access->psz_url
    );


    vlc_readdir_helper_init( &rdh, p_access, current_node );

    for ( int i = 0; i < 5; i++ )
    {
        uri[sizeof(uri) - 2] = (char)('0' + i);
        file_name[sizeof(file_name) - 2] = (char)('0' + i);
        vlc_readdir_helper_additem( &rdh, (uri), "/tmp", (file_name), 1, ITEM_NET );
    }
    vlc_readdir_helper_finish( &rdh, 1 );
    return VLC_SUCCESS;
}

static ssize_t      fbxapi_read( stream_t *p_access, void *buffer, size_t len )
{
    const char sample[] = "gdsgjsfdlgfdgjsfdkghsfdjkghsfdjghsfdlkgjsfdlghsfdlghsfdg";
    static int i = 0;

    (void)buffer;
    (void)len;
    printf("From %s\n", __FUNCTION__);
    if ( i != 0 )
    {
        return 0;
    }
    i++;
    printf(
        "Function %s called on %s - %s - %s - %s\n",
        __FUNCTION__,
        p_access->psz_name,
        p_access->psz_url,
        p_access->psz_location,
        p_access->psz_filepath
    );

    input_item_t	*item = p_access->p_input_item;

    size_t      l = len >= sizeof(sample) ? sizeof(sample) : len;
    memcpy(buffer, sample, l);
    return l;
}
*/
