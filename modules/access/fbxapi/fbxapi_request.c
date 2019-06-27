#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>

#include <vlc_common.h>
#include <vlc_vector.h>
#include <vlc_stream.h>
#include <vlc_tls.h>
#include <vlc_memstream.h>

#include "fbxapi.h"
#include "fbxapi_request.h"

static void        fbxapi_set_headers(
    struct vlc_memstream *stream,
    const char **headers
)
{
    size_t        i;

    for ( i = 0; headers[i] != NULL; i++ )
    {
        vlc_memstream_printf(stream, "%s\r\n", headers[i]);
    }
}

void                fbxapi_set_request_bases(
    struct vlc_memstream *stream,
    const struct s_fbxapi *fbx,
    const char *verb,
    const char *endpoint
)
{
    vlc_memstream_printf(stream, "%s %s HTTP/1.1\r\n", verb, endpoint);
    vlc_memstream_printf(
        stream,
        "Host: %s:%d\r\n",
        fbx->http.domain,
        fbx->http.port
    );
    vlc_memstream_puts(stream, "Accept: text/html,Application/json,*/*;q=8\r\n");
    vlc_memstream_puts(stream, "Accept-Language: en-US,en,q=0.5\r\n");
    vlc_memstream_puts(stream, "Accept-Encoding: " FBX_ACCEPTED_ENCODING "\r\n\r\n");
}

static int      fbx_get_body(
    const s_fbxapi *fbx,
    s_fbxapi_request *request
)
{
    struct VLC_VECTOR(char *)   vec = VLC_VECTOR_INITIALIZER;
    size_t                      body_length;
    size_t                      length;
    size_t                      index;
    char                        *input;

    /**
     * In order to avoid reallocation of the body everytime, store it line by
     * line in a vector and make the final allocation at once
     */
    body_length = 0;
    index = 0;
    printf("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
    while ( (input = vlc_tls_GetLine(fbx->http.tls)) != NULL )
    {
        body_length += strlen(input);
        if ( vlc_vector_push(&vec, input) == false )
        {
            goto eclear_and_return;
        }
    }

    request->body = malloc(sizeof(char) * (body_length + 1));
    if (request->body == NULL)
    {
        goto eclear_and_return;
    }

    vlc_vector_foreach(input, &vec)
    {
        length = strlen(input);
        memcpy(request->body + index, input, length);
        index += length;
        free(input);
    }
    request->body[index] = '\0';
    /* Warning : The vector is full of freed pointers not nulled */
    vlc_vector_clear(&vec);
    return VLC_SUCCESS;

eclear_and_return:

    if ( vec.size > 0 )
    {
        vlc_vector_foreach(input, &vec)
        {
            free(input);
        }
    }
    if ( vec.cap > 0 )
    {
        vlc_vector_clear(&vec);
    }
    if ( request->body != NULL )
    {
        free(request->body);
    }
    return VLC_ENOMEM;
}

static int        fbx_get_response(
    const s_fbxapi *fbx,
    s_fbxapi_request *request
)
{
    char                *input;
    char                *iterator;
    size_t              index;

    memset(request, 0, sizeof(* request));

    /*
     * First step : Parse the first line
     * Expected format : HTTP/[12].[01] GET|POST|... Some text
     */
    input = vlc_tls_GetLine(fbx->http.tls);
    if ( input == NULL )
    {
        return VLC_EGENERIC;
    }
    if (
        strncmp(input, "HTTP/", 5) != 0
        || (iterator = strchr(input, ' ')) == NULL
    )
    {
        return VLC_EGENERIC;
    }
   
    iterator++;
    for ( size_t i = 0; i < 3; i++ )
    {
        if ( isdigit(iterator[i]) == 0 )
        {
            return VLC_EGENERIC;
        }
    }
   
    if ( ! isblank(iterator[3]) && iterator[3] != '\0' )
    {
        return VLC_EGENERIC;
    }
    request->status_code = atoi(iterator);
    if (request->status_code < 100 || request->status_code > 599)
    {
        return VLC_EGENERIC;
    }
    iterator += 3; /* Points after the third digit */
    while ( isblank(*iterator) )
    {
        iterator++;
    }
    request->status_text = iterator[0] == '\0' ? NULL : strdup(iterator);

    free(input);
    input = iterator = NULL;

    /*
     *  While we do not have an empty line, we are supposed to have http header
     */
    index = 0;
    while (
        (input = vlc_tls_GetLine(fbx->http.tls)) != NULL
        && input[0] != '\0'
    )
    {
        char **tmp = realloc(request->headers, sizeof(* request->headers) * (index + 2));

        if ( tmp == NULL )
        {
            for ( size_t i = 0; i < index; i++ )
            {
                free(request->headers[i]);
            }
            free(request->headers);
            free(request->status_text);
            return VLC_ENOMEM;
        }
        request->headers = tmp;
        request->headers[index] = input;
        index++;
    }
    request->headers[index] = NULL;
    if ( input != NULL )
    {
        free(input);
    }

    fbx_get_body(fbx, request);
    return VLC_SUCCESS;
}

int                fbxapi_request(
    const s_fbxapi *fbx,
    s_fbxapi_request *request,
    const char *verb,
    const char *endpoint,
    const char **headers,
    const char *body
)
{
    struct vlc_memstream    stream;

    memset(&stream, 0, sizeof(stream));
    vlc_memstream_open(&stream);
    fbxapi_set_request_bases(&stream, fbx, verb, endpoint);
    if ( headers != NULL )
    {
        fbxapi_set_headers(&stream, headers);
    }
    if ( body != NULL )
    {
        vlc_memstream_printf(&stream, "\r\n%s\r\n", body);
    }
    if ( vlc_memstream_flush(&stream) || vlc_memstream_close(&stream) )
    {
        return VLC_EGENERIC;
    }

    vlc_tls_Write(fbx->http.tls, stream.ptr, stream.length);
    free(stream.ptr);

    fbx_get_response(fbx, request);

    return VLC_SUCCESS;
}

void         fbxapi_request_destroy(struct s_fbxapi_request *request)
{
    char        **headers = request->headers;
    size_t      i;

    if ( request->status_text != NULL )
    {
        free(request->status_text);
    }
    if ( headers != NULL )
    {
        for ( i = 0; headers[i] != NULL; i++ )
        {
            free(headers[i]);
        }
        free(headers);
    }
    if ( request->body != NULL )
    {
        free(request->body);
    }
    memset(request, 0, sizeof(*request));
}
