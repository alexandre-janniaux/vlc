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
    vlc_memstream_puts(stream, "Connection: keep-alive\r\n");
    vlc_memstream_puts(stream, "Accept: text/html,Application/json,*/*;q=8\r\n");
    vlc_memstream_puts(stream, "Accept-Language: en-US,en,q=0.5\r\n");
    vlc_memstream_puts(stream, "Accept-Encoding: " FBX_ACCEPTED_ENCODING "\r\n");
    vlc_memstream_puts(stream, "Keep-Alive: timeout=30, max=60\r\n");
}

int             fbxapi_get_http(
    const struct s_fbxapi *fbx,
    int *status_code,
    char **status
)
{
    char        *input = NULL;
    char        *iterator;

    *status = NULL;

    input = vlc_tls_GetLine(fbx->http.tls);
    if ( input == NULL )
    {
        return VLC_EGENERIC;
    }

    /*
     * Expected format : HTTP/[12].[01] GET|POST|... Some text
     */
    if (
        strncmp(input, "HTTP/", 5) != 0
        || (iterator = strchr(input, ' ')) == NULL
    )
    {
        free(input);
        return VLC_EGENERIC;
    }
   
    iterator++;
    for ( size_t i = 0; i < 3; i++ )
    {
        if ( isdigit(iterator[i]) == 0 )
        {
            free(input);
            return VLC_EGENERIC;
        }
    }
   
    if ( ! isblank(iterator[3]) && iterator[3] != '\0' )
    {
        free(input);
        return VLC_EGENERIC;
    }
    *status_code = atoi(iterator);
    if ( *status_code < 100 || *status_code > 599)
    {
        free(input);
        return VLC_EGENERIC;
    }
    if ( *status != NULL )
    {
        iterator += 3; /* Points after the third digit */
    }
    while ( isblank(*iterator) )
    {
        iterator++;
    }
    if ( iterator[0] == '\0' )
    {
        *status = NULL;
    }
    else
    {
        *status = strdup(iterator);
        if ( *status == NULL )
        {
            free(input);
            return VLC_ENOMEM;
        }
    }
    free(input);
    return VLC_SUCCESS;
}

char            **fbxapi_get_headers( const s_fbxapi *fbx )
{
    char    **tmp = NULL;
    size_t  index = 0;
    char    **headers = NULL;
    char    *input;

    headers = calloc(1, sizeof(*headers)); // Null terminated array
    if ( headers == NULL )
    {
        return NULL;
    }

    /*
     *  While we do not have an empty line, we are supposed to have http header
     */
    while (
        (input = vlc_tls_GetLine(fbx->http.tls)) != NULL
        && input[0] != '\0'
    )
    {
        tmp = realloc(headers, sizeof(* headers) * (index + 2));

        if ( tmp == NULL )
        {
            for ( size_t i = 0; i < index; i++ )
            {
                free(headers[i]);
            }
            free(headers);
            return NULL;
        }
        headers = tmp;
        headers[index] = input;
        index++;
    }
    headers[index] = NULL;
    if ( input != NULL )
    {
        free(input);
    }
    return headers;
}

char            *fbxapi_get_body( const s_fbxapi *fbx )
{
    struct VLC_VECTOR(char *)   vec = VLC_VECTOR_INITIALIZER;
    size_t                      body_length;
    char                        *body;
    size_t                      length;
    size_t                      index;
    char                        *input;

    /**
     * In order to avoid reallocation of the body everytime, store it line by
     * line in a vector and make the final allocation at once
     */
    body_length = 0;
    index = 0;
    printf("```Receive\n");
    while ( (input = vlc_tls_GetLine(fbx->http.tls)) != NULL && input[0] != '\0' )
    {
        body_length += strlen(input);
        printf("%s", input);
        if ( vlc_vector_push(&vec, input) == false )
        {
            goto eclear_and_return;
        }
    }
    if ( input != NULL )
    {
        free(input);
        input = NULL;
    }
    printf("\n```\n");

    body = malloc(sizeof(char) * (body_length + 1));
    if (body == NULL)
    {
        goto eclear_and_return;
    }

    vlc_vector_foreach(input, &vec)
    {
        length = strlen(input);
        memcpy(body + index, input, length);
        index += length;
        free(input);
    }
    body[index] = '\0';
    /* Warning : The vector is full of freed pointers not nulled */
    vlc_vector_clear(&vec);
    return body;

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
    if ( body != NULL )
    {
        free(body);
    }
    return NULL;
}

static int      fbx_get_response( const s_fbxapi *fbx, s_fbxapi_request *request )
{
    int res;

    memset(request, 0, sizeof(* request));

    if (
        (
            res = fbxapi_get_http(fbx, &request->status_code, &request->status_text)
        ) != VLC_SUCCESS
    )
    {
        return res;
    }

    if ( (request->headers = fbxapi_get_headers(fbx)) == NULL )
    {
        free(request->status_text);
        return VLC_ENOMEM;
    }

    if ( (request->body = fbxapi_get_body(fbx)) == NULL )
    {
        free(request->status_text);
        for ( size_t i = 0; request->headers[i] != NULL; i++ )
        {
            free(request->headers[i]);
        }
        free(request->headers);
        return VLC_ENOMEM;
    }
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
        vlc_memstream_printf(&stream, "Content-Length: %lu\r\n", strlen(body));
    }
    vlc_memstream_puts(&stream, "\r\n");
    if ( body != NULL )
    {
        vlc_memstream_printf(&stream, "%s\r\n", body);
    }
    if ( vlc_memstream_flush(&stream) || vlc_memstream_close(&stream) )
    {
        return VLC_EGENERIC;
    }
    vlc_tls_Write(fbx->http.tls, stream.ptr, stream.length);
    printf("```Send\n%s\n```\n", stream.ptr);
    free(stream.ptr);


    return fbx_get_response(fbx, request);
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
