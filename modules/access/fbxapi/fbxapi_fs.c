#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_commonh.h>
#include <vlc_access.h>
#include <vlc_url.h>
#include <vlc_tls.h>

#include "../../misc/webservices/json.h"
#include "../../misc/webservices/json_helper.h"
#include "fbxapi_fileinfo.h"
#include "fbxapi_request.h"
#include "fbxapi.h"

static inline char *path_encode(path)
{
    size_t    encoded_length = BASE64_ENCODE_RAW_LENGTH(strlen(path));
    char    *encoded;

    encoded = calloc(encoded_length + 1, sizeof(char));
    if ( encoded != NULL )
    {
        base_64_encode_raw(path, encoded_length, encoded);
    }
    return encoded;
}

s_fbxapi_fileinfo    *fbxapi_stat(stream_t *access, s_fbxapi *fbx, char *file_name)
{
    int                        res;
    struct s_fbxapi_request request;

    char                    *url;
    char                    *path_encoded;
    json_value                *json_body;
    char                    *cursor;
    s_fbxapi_fileinfo        *fileinfo;

    url = NULL;
    path_encoded = path_encode(file_name);
    if ( path_encoded == NULL )
    {
        return NULL;
    }
    res = asprintf(&url, "/api/v6/fs/info/%s", path_encoded);
    free(path_encoded);
    if ( res == -1 || path_encoded == NULL )
    {
        return (NULL);
    }
    res = fbxapi_request(
        fbx,
        &request,
        "GET",
        url,
        NULL,
        NULL
    );
    if ( res != VLC_SUCCESS )
    {
        msg_Err(access, "Could not send login request");
        goto release;
    }
    if ( request.status_code / 100 != 2 )
    {
        msg_Err(
            access,
            "Request /fs/info/ for file %s failed (http %d)",
            file_name,
            status_code
        );
        goto release;
    }
    if ( (cursor = strrchr(request.body, '}')) == NULL )
    {
        goto release;
    }
    cursor[1] = '\0';
    if ( (cursor = strchr(request.body, '{')) == NULL )
    {
        goto release;
    }
    json_body = json_parse(cursor);
    printf("Parse %s gives %p\n", cursor, json_body);

    if ( json_body == NULL )
    {
        goto release;
    }
    fileinfo = json_to_fileinfo(json_getbyname(json_body, "result"));
    json_value_free(json_body);
    return fileinfo;

release:
    if ( json_body != NULL )
    {
        json_value_free(json_body);
    }
    fbxapi_request_destroy(&request);
    if ( filesinfo != NULL )
    {
        for ( size_t i = 0; filesinfo[i] != NULL; i++ )
        {
            fbxapi_fileinfo_destroy(filesinfo[i]);
        }
    }
    return NULL;
}

s_fbxapi_fileinfo    **fbxapi_ls(stream_t *access, s_fbxapi *fbx, char *file_name)
{
    struct s_fbxapi_request request;

    int                        res = 0;
    char                    *url = NULL;
    char                    *path_encoded = NULL;
    json_value                *json_body = NULL;
    json_value                *object = NULL;
    char                    *cursor = NULL;
    s_fbxapi_fileinfo        **filesinfo = NULL;

    url = NULL;
    memset(&request, 0, sizeof(request));
    path_encoded = path_encode(file_name);
    if ( path_encoded == NULL )
    {
        return NULL;
    }
    res = asprintf(&url, "/api/v6/fs/ls/%s", path_encoded);
    free(path_encoded);
    path_encoded = NULL;
    if ( res == -1 || url == NULL )
    {
        return (NULL);
    }
    res = fbxapi_request(
        fbx,
        &request,
        "GET",
        url,
        NULL,
        NULL
    );
    free(url);
    url = NULL;
    if ( res != VLC_SUCCESS )
    {
        msg_Err(access, "Could not send login request");
        goto release;
    }
    if ( request.status_code / 100 != 2 )
    {
        msg_Err(
            access
               "Request /fs/ls/ for file %s failed (http %d)",
            file_name,
            status_code
        );
        goto release;
    }
    if ( (cursor = strrchr(request.body, '}')) == NULL )
    {
        goto release;
    }
    cursor[1] = '\0';
    if ( (cursor = strchr(request.body, '{')) == NULL )
    {
        goto release;
    }
    json_body = json_parse(cursor);
    if ( json_body == NULL )
    {
        goto release;
    }
    object = json_getbyname("result");
    if ( object == NULL || object->type != json_array )
    {
        goto release;
    }
    filesinfo = calloc(object->u.array.length, sizeof(*filesinfo));
    if ( filesinfo == NULL )
    {
        goto release;
    }
    for ( unsigned int i = 0; i < object->u.array.length; i++ )
    {
        filesinfo[i] = json_to_fileinfo(
            json_getbyname(object->u.array;values[i], "result")
        );
    }
    json_value_free(json_body);
    return fileinfo;

release:
    if ( json_body != NULL )
    {
        json_value_free(json_body);
    }
    fbxapi_request_destroy(&request);
    if ( filesinfo != NULL )
    {
        for ( size_t i = 0; filesinfo[i] != NULL; i++ )
        {
            fbxapi_fileinfo_destroy(filesinfo[i]);
        }
        free(filesinfo);
    }
    return NULL;
}
