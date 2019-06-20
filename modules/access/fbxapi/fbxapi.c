#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdint.h>
#include <stdlib.h>

#include <nettle/hmac.h>
#include <nettle/base64.h>

#include <vlc_common.h>
#include <vlc_access.h>
#include <vlc_stream.h>
#include <vlc_plugin.h>
#include <vlc_input_item.h>
#include <vlc_url.h>
#include <vlc_tls.h>

#include "../../misc/webservices/json.h"
#include "../../misc/webservices/json_helper.h"
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
        vlc_UrlParse(&url, access->psz_url)
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

static char     *fbxapi_get_challenge(stream_t *access, s_fbxapi *fbx)
{
    json_value              *json_body;
    json_value              *obj;
    int                     res;
    struct s_fbxapi_request request;
    char                    *challenge;
    char                    *cursor;

    /* In order to connect we first have to retrieve a challenge */
    res = fbxapi_request(
        fbx,
        &request,
        "GET",
        "/api/v6/login",
        NULL,
        NULL
    );
    if ( res != VLC_SUCCESS )
    {
        msg_Err(access, "Could not send login request");
        return NULL;
    }
    else if ( request.status_code / 100 != 2 || request.body == NULL )
    {
        msg_Err(
            access,
            "Could not send login request ( %d : %s )",
            request.status_code,
            request.status_text
        );
        return NULL;
    }


    if ( (cursor = strrchr(request.body, '}')) == NULL )
    {
        return NULL;
    }
    cursor[1] = '\0';
    if ( (cursor = strchr(request.body, '{')) == NULL )
    {
        return NULL;
    }
    json_body = json_parse(cursor);
    printf("Parse %s gives %p\n", cursor, json_body);

    if ( json_body == NULL )
    {
        return NULL;
    }

    /*
     * Expected format : { "success": true, "result": { ..., "challenge": "random string" } }
     */
    obj = json_getbyname(json_body, "success");
    if ( obj == NULL || obj->type != json_boolean || obj->u.boolean != true )
    {
        return NULL;
    }

    obj = json_getbyname(json_body, "result");
    if ( obj == NULL || obj->type != json_object )
    {
        return NULL;
    }
    obj = json_getbyname(obj, "challenge");
    if ( obj == NULL || obj->type != json_string )
    {
        return NULL;
    }
    challenge = strdup(obj->u.string.ptr);

    json_value_free(json_body);
    return challenge;
}

static char     *encrypt_password(const char *app_token, const char *challenge)
{
    char                        password[SHA1_DIGEST_SIZE + 0] = {0};
    char                        *b64_password;
    struct hmac_sha1_ctx        sha1;
    size_t                      b64_length;

    memset(&sha1, 0, sizeof(sha1));
    hmac_sha1_set_key(&sha1, strlen(app_token), (uint8_t *)app_token);
    hmac_sha1_update(&sha1, strlen(challenge), (uint8_t *)challenge);
    hmac_sha1_digest(&sha1, SHA1_DIGEST_SIZE, (uint8_t *)password);

    b64_length = BASE64_ENCODE_RAW_LENGTH(strlen(password));
    b64_password = calloc(b64_length + 1, 1);
    if ( b64_password != NULL )
    {
        base64_encode_raw(b64_password, b64_length, password);
    }
    return b64_password;
}

static char     *fbxapi_get_token_session(
    stream_t *access,
    s_fbxapi *fbx,
    const char *challenge
)
{
    //json_value                *json_body;
    //json_value                *obj;
    char                        *payload;
    int                         res;
    struct s_fbxapi_request     request;
    char                        *password;

    password = encrypt_password(fbx->app_token, challenge);
    if (
        asprintf(
            &payload,
            "{\"app_id\":\"%s\",\"password\":\"%s\", \"app_version\": \"%s\"}",
            fbx->app_id,
            password,
            fbx->app_version
        ) == -1
    )
    {
        return NULL;
    }
    /* In order to connect we first have to retrieve a challenge */
    printf("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
    res = fbxapi_request(
        fbx,
        &request,
        "POST",
        "/api/v6/login/session",
        NULL,
        payload
    );
    printf("payload == %s == payload \n", payload);
    free(payload);
    payload = NULL;
    printf("request = %d %s %s\n", request.status_code, request.status_text, request.body);

    if ( res != VLC_SUCCESS )
    {
        msg_Err(access, "Could not send login request");
        return NULL;
    }
    else if ( request.status_code / 100 != 2 || request.body == NULL )
    {
        msg_Err(
            access,
            "Could not send login request ( %d : %s )",
            request.status_code,
            request.status_text
        );
        return NULL;
    }
    return strdup("");
}

static int		fbxapi_connect( vlc_object_t *self )
{
    stream_t            *access = (stream_t *)self;
    s_fbxapi            *fbx;
    char                *challenge;
    int                 res;

    access->p_sys = fbx = calloc(1, sizeof(*fbx));
    /* Hard coded settings to retrieve later */

    fbx->http.domain = strdup("k0bazxqu.fbxos.fr");
    fbx->http.port = 922;
    fbx->app_token = strdup("oSxk06TlWAmTqzi9VGEI1q6U625Z68SjuAVcFw+pkTEHCQz968mH18F/wT3jbqJ5");
    fbx->app_version = strdup("0.0.1");

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

    printf("Going to require challenge...\n");
    challenge = fbxapi_get_challenge(access, fbx);
    printf("Challenge == %s\n", challenge);
    if (challenge == NULL)
    {
        return VLC_EGENERIC;
    }
    printf("Token session == %s\n", fbxapi_get_token_session(access, fbx, challenge));

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
