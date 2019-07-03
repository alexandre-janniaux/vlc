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
#include <vlc_memstream.h>
#include <vlc_plugin.h>
#include <vlc_input_item.h>
#include <vlc_url.h>
#include <vlc_tls.h>

#include "../../misc/webservices/json.h"
#include "../../misc/webservices/json_helper.h"
#include "fbxapi_request.h"
#include "fbxapi_fs.h"
#include "fbxapi_app_register.h"
#include "fbxapi_fileinfo.h"
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
fbxapi_rest_connect( stream_t *access )
{
    const char    base_get_challenge[] = "/login";
    const char    base_get_login[] = "/login/session";
}*/

int             fbxapi_open_tls( vlc_object_t *self )
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

static char     *fbxapi_get_challenge( stream_t *access, s_fbxapi *fbx )
{
    json_value              *json_body;
    const json_value        *obj;
    int                     res;
    struct s_fbxapi_request request;
    char                    *challenge;
    char                    *cursor;

    /* In order to connect we first have to retrieve a challenge */
    res = fbxapi_request(fbx, &request, "GET", "/api/v6/login", NULL, NULL);
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

static char     *encrypt_password( const char *app_token, const char *challenge )
{
    char                        password[SHA1_DIGEST_SIZE + 1] = {0}; // NULL terminated
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
        base64_encode_raw(b64_password, b64_length, (uint8_t *)password);
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
    if ( password == NULL )
    {
        return NULL;
    }
    if (
        asprintf(
            &payload,
            "{\"app_id\":\"" FBXAPI_APP_ID "\",\"password\":\"%s\", \"app_version\": \"%s\"}",
            password,
            fbx->app_version
        ) == -1
    )
    {
        return NULL;
    }
    free(password);
    password = NULL;
    /* In order to connect we first have to retrieve a challenge */
    res = fbxapi_request(
        fbx,
        &request,
        "POST",
        "/api/v6/login/session",
        NULL,
        payload
    );
    free(payload);
    payload = NULL;

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

static int        fbxapi_connect( vlc_object_t *self )
{
    stream_t            *access = (stream_t *)self;
    s_fbxapi            *fbx;
    char                *challenge;
    int                 res;

    access->p_sys = fbx = calloc(1, sizeof(*fbx));
    /* Hard coded settings to retrieve later */

    fbx->http.domain = strdup("k0bazxqu.fbxos.fr");
    fbx->http.port = 922;
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

    fbx->app_token = fbxapi_get_app_token(self);
    if ( fbx->app_token == NULL )
    {
        vlc_tls_Close(fbx->http.tls);
        free(fbx);
        access->p_sys = fbx = NULL;
        return VLC_EGENERIC;
    }

    challenge = fbxapi_get_challenge(access, fbx);
    if (challenge == NULL)
    {
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

static void        fbxapi_disconnect( vlc_object_t *self )
{
    stream_t    *access = (stream_t *)self;
    s_fbxapi    *fbx = access->p_sys;

    vlc_tls_SessionDelete(fbx->http.tls);
    free(access->p_sys);
    access->p_sys = fbx = NULL;
    (void)self;
}

static int        fbxapi_seek( stream_t *access, uint64_t u )
{
    (void)access;
    (void)u;
    return 1;
}

static int        fbxapi_readdir(
    stream_t *access,
    input_item_node_t *current_node
)
{
    struct vlc_readdir_helper   rdh;
    s_fbxapi_fileinfo           **files;

    files = fbxapi_ls(access, access->p_sys, "/"); // FIXME
    if ( files == NULL )
    {
        return -1;
    }

    vlc_readdir_helper_init( &rdh, access, current_node );

    for ( int i = 0; files[i] != NULL; i++ )
    {
        vlc_readdir_helper_additem(
            &rdh,
            "some uri",
            files[i]->path,
            files[i]->name,
            1,
            ITEM_NET
        );
        fbxapi_fileinfo_destroy(files[i]);
        free(files[i]);
    }
    free(files);
    vlc_readdir_helper_finish( &rdh, 1 );
    return VLC_SUCCESS;
}

static ssize_t    fbxapi_read(
    stream_t *access,
    void *buffer,
    size_t buffer_length
)
{
    s_fbxapi    *fbx = access->p_sys;

    if ( fbx->first_read == 0 )
    {
        /* First, send request to the freebox to have a file */
        int                     res = 0;
        char                    *url = NULL;
        char                    *path_encoded = NULL;
        struct vlc_memstream    stream;
        const char *file_name = NULL; // FIXME

        url = NULL;
        path_encoded = path_encode(file_name);
        if ( path_encoded == NULL )
        {
            return -1;
        }
        res = asprintf(&url, "/api/v6/dl/%s", path_encoded);
        free(path_encoded);
        path_encoded = NULL;
        if ( res == -1 || url == NULL )
        {
            return -1;
        }

        /*
         * Not using fbx_request because we don't want to receive all the file at one
         */
        memset(&stream, 0, sizeof(stream));
        vlc_memstream_open(&stream);
        fbxapi_set_request_bases(&stream, fbx, "GET", url);
        free(url);
        if ( vlc_memstream_flush(&stream) || vlc_memstream_close(&stream) )
        {
            return -1;
        }

        vlc_tls_Write(fbx->http.tls, stream.ptr, stream.length);
        free(stream.ptr);

        /* Then read all the http stuff before the file itself */
        {
            int     status_code;
            char    **headers;

            if ( (res = fbxapi_get_http(fbx, &status_code, NULL)) != VLC_SUCCESS )
            {
                return -1;
            }
            if ( (headers = fbxapi_get_headers(fbx)) == NULL )
            {
                return -1;
            }
            for ( size_t i = 0; headers[i] != NULL; i++ )
            {
                free(headers[i]);
            }
            free(headers);
        }
    }

    vlc_tls_Read(fbx->http.tls, buffer, buffer_length, false);
}
