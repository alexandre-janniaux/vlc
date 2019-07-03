#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <assert.h>

#include <vlc_common.h>
#include <vlc_strings.h>
#include <vlc_access.h>
#include <vlc_stream.h>
#include <vlc_plugin.h>
#include <vlc_input_item.h>
#include <vlc_url.h>
#include <vlc_tls.h>
#include <vlc_keystore.h>

#include "../../misc/webservices/json.h"
#include "../../misc/webservices/json_helper.h"
#include "fbxapi_app_register.h"
#include "fbxapi.h"
#include "fbxapi_request.h"

const char * const      values[KEY_MAX] = {
    [KEY_PROTOCOL] = "https",
    [KEY_USER] = "VLC_FBXAPI",
    [KEY_SERVER] = NULL,
    [KEY_PATH] = NULL,
    [KEY_PORT] = "922",
    [KEY_REALM] = NULL,
    [KEY_AUTHTYPE] = NULL,
};

static int      fbxapi_poll_app_validation(
    stream_t *access,
    const s_fbxapi *fbx,
    int track_id
)
{
    int                 res = VLC_SUCCESS;
    char                *url = NULL;
    json_value          *json_body = NULL;
    const json_value    *obj = NULL;
    char                *status = NULL;
    char                *cursor = NULL;
    s_fbxapi_request    request;

    assert(fbx != NULL);

    if (
        asprintf(&url, "/api/v6/login/authorize/%d", track_id) == -1
        || url == NULL
    )
    {
        return VLC_ENOMEM;
    }
    memset(&request, 0, sizeof(request));

    while ( "app validation is pending" )
    {
        res = fbxapi_request(
            fbx,
            &request,
            "GET",
            url,
            NULL,
            NULL
        );
        if ( res == -1 )
        {
            vlc_tls_Close(fbx->http.tls);
              
            if ( fbxapi_open_tls( (void *)access ) != VLC_SUCCESS )
            {
                return VLC_EGENERIC;
            }
            usleep(500000);
            continue ;
        }
        if ( res !=  VLC_SUCCESS )
        {
            msg_Err(access, "Could not send app validation request");
            goto release;
        }
        if ( request.status_code / 100 != 2 || request.body == NULL )
        {
            msg_Err(
                access,
                "Could not send app validation request ( %d : %s )",
                request.status_code,
                request.status_text
            );
            res = VLC_EGENERIC;
            goto release;
        }

        // FIXME Get rid of these ugly strchrs, make sure that fbxapi_request
        // sanitize itself or fix this in any clean way. Pls don't git blame
        if ( (cursor = strrchr(request.body, '}')) == NULL )
        {
            res = VLC_EGENERIC;
            goto release;
        }
        cursor[1] = '\0';
        if ( (cursor = strchr(request.body, '{')) == NULL )
        {
            res = VLC_EGENERIC;
            goto release;
        }

        json_body = json_parse(cursor);
        if ( json_body == NULL )
        {
            msg_Err(
                access,
                "Could not parse received body for app registration request"
            );
            res = VLC_EGENERIC;
            goto release;
        }

        obj = json_getbyname(json_body, "success");
        if ( obj == NULL || obj->type != json_boolean || obj->u.boolean != true )
        {
            res = VLC_EGENERIC;
            goto release;
        }

        obj = json_getbyname(json_body, "result");
        if ( obj == NULL || obj->type != json_object )
        {
            res = VLC_EGENERIC;
            goto release;
        }
        obj = json_getbyname(obj, "status");
        if ( obj == NULL || obj->type != json_string )
        {
            res = VLC_EGENERIC;
            goto release;
        }
        if ( strcmp("pending", obj->u.string.ptr) != 0 )
        {
            break ;
        }

        usleep(2000000);

        json_value_free(json_body);
        obj = json_body = NULL; // Avoid danging pointer
        fbxapi_request_destroy(&request);
    }

    res = strcmp(status, "granted") != 0 ? VLC_EGENERIC : VLC_SUCCESS;

release:
    fbxapi_request_destroy(&request);
    if ( url != NULL )
    {
        free(url);
    }
    if ( json_body != NULL )
    {
        obj = json_body = NULL; // Avoid danging pointer
        json_value_free(json_body);

    }
    return res;
}

static int      fbxapi_register_app( vlc_object_t *self, char **app_token )
{
    stream_t            *access = (stream_t *)self;
    const s_fbxapi      *fbx = access->p_sys;
    s_fbxapi_request    request;
    const char          payload[] = \
        "{" \
            "\"app_id\":\"" FBXAPI_APP_ID "\"," \
            "\"app_name\":\"" FBXAPI_APP_NAME "\"," \
            "\"app_version\":\"" FBXAPI_APP_VERSION "\"," \
            "\"device_name\":\"" FBXAPI_DEVICE_NAME "\"" \
        "}";
    int                 res = VLC_SUCCESS;
    char                *cursor = NULL;
    json_value          *json_body = NULL;
    const json_value    *obj = NULL;
    const json_value    *obj_result = NULL;
    char                *token = NULL;
    int                 track_id;

    assert(fbx != NULL);

    memset(&request, 0, sizeof(request));

    res = fbxapi_request(
        fbx,
        &request,
        "POST",
        "/api/v6/login/authorize",
        NULL,
        payload
    );
    if ( res !=  VLC_SUCCESS )
    {
        msg_Err(access, "Could not send app registration request");
        goto release;
    }
    if ( request.status_code / 100 != 2 || request.body == NULL )
    {
        msg_Err(
            access,
            "Could not send app registration request ( %d : %s )",
            request.status_code,
            request.status_text
        );
        res = VLC_EGENERIC;
        goto release;
    }

    // FIXME Get rid of these ugly strchrs, make sure that fbxapi_request
    // sanitize itself or fix this in any clean way. Pls don't git blame
    if ( (cursor = strrchr(request.body, '}')) == NULL )
    {
        res = VLC_EGENERIC;
        goto release;
    }
    cursor[1] = '\0';
    if ( (cursor = strchr(request.body, '{')) == NULL )
    {
        res = VLC_EGENERIC;
        goto release;
    }

    json_body = json_parse(cursor);
    if ( json_body == NULL )
    {
        msg_Err(
            access,
            "Could not parse received body for app registration request"
        );
        res = VLC_EGENERIC;
        goto release;
    }

    obj = json_getbyname(json_body, "success");
    if ( obj == NULL || obj->type != json_boolean || obj->u.boolean != true )
    {
        res = VLC_EGENERIC;
        goto release;
    }

    obj_result = json_getbyname(json_body, "result");
    if ( obj_result == NULL || obj_result->type != json_object )
    {
        res = VLC_EGENERIC;
        goto release;
    }
    obj = json_getbyname(obj_result, "app_token");
    if ( obj == NULL || obj->type != json_string )
    {
        res = VLC_EGENERIC;
        goto release;
    }
    token = strdup(obj->u.string.ptr);
    if ( token == NULL )
    {
        res = VLC_ENOMEM;
        goto release;
    }
    obj = json_getbyname(obj_result, "track_id");
    if ( obj == NULL || obj->type != json_integer )
    {
        res = VLC_EGENERIC;
        goto release;
    }
    track_id = obj->u.integer;

    msg_Dbg(access, "App registered, going to poll");
    res = fbxapi_poll_app_validation(access, fbx, track_id);
    msg_Dbg(
        access,
        "App registered, has %sbeen validated",
        res == VLC_SUCCESS ? "": "not "
    );

    if ( token != NULL && res == VLC_SUCCESS )
    {
        *app_token = token;
    }

release:
    if ( json_body != NULL )
    {
        json_value_free(json_body);
    }
    fbxapi_request_destroy(&request);
    return res;

}

char      *fbxapi_get_app_token( vlc_object_t *self )
{
    /*
     *  We should have a token associated to our app. It it is stored in the key
     *  store, nice, else we'll have to register vlc's freebox application and
     *  store the key
     */

    vlc_keystore        *store = NULL;
    vlc_keystore_entry  *entries = NULL;
    unsigned int        entries_number = 0;
    char                *app_token = NULL;

/*    store = vlc_keystore_create(self);
    if ( store == NULL )
    {
        return VLC_EGENERIC;
    }

    entries_number = vlc_keystore_find(store, values, &entries);

    if ( entries_number < 1 )
    {*/
    //fbxapi_register_app(self, &app_token);
    app_token = strdup("fHknH11EcmMYGt9lmghirzDRUvtb+OTn1Q7lKDU+nhdZ5yQPUj51HDb2WMgOZmUj"); // FIXME
    return app_token;
 //       track_id = 15;
        /*entries_number = vlc_keystore_store(
            store,
            values,
            app_token,
            strlen(app_token),
            "app_token"
        );
        entries_number = vlc_keystore_find(store, values, &entries);
    }

    for (unsigned int i = 0; i < entries_number; i++)
    {
    }

    vlc_keystore_release_entries(entries, entries_number);
    vlc_keystore_release(store);
    */
}
