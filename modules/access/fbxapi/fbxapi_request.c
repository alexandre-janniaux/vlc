#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdint.h>
#include <stdlib.h>

#include <vlc_common.h>
#include <vlc_stream.h>
#include <vlc_tls.h>
#include <vlc_memstream.h>

#include "fbxapi.h"
#include "fbxapi_request.h"

static void		fbxapi_set_headers(
	struct vlc_memstream *stream,
	const char **headers
)
{
	size_t		i;

	for ( i = 0; headers[i] != NULL; i++ )
	{
		vlc_memstream_printf(stream, "%s\r\n", headers[i]);
	}
}

static void		fbxapi_set_bases(
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

static int		fbx_get_response(const s_fbxapi *fbx)
{
	char	*input;
	s_fbxapi_request		request;

	memset(&request, 0, sizeof(request));
	input = vlc_tls_GetLine(fbx->http.tls);
	if (input == NULL)
	{
		return VLC_EGENERIC;
	}
	if ( sscanf(
			input,
			"HTTP/1.1 %d %s",
			&request.status_code,
			&request.status_text
		) == -1
	)
	{
		free(input);
		return VLC_EGENERIC;
	}
	free(input);

	index = 0;
	while (
		(input = vlc_tls_GetLine(fbx->http.tls)) != NULL
		&& input[0] != '\0'
	)
	{
		char **tmp = realloc(request.headers, sizeof(* request.headers) * (index + 2));

		if ( tmp == NULL )
		{
			for ( size_t i = 0; i < index; i++ )
			{
				free(request.headers[i]);
			}
			free(request.headers);
			free(request.status_text);
			return VLC_ENOMEM;
		}
		request.headers = tmp;
		request.headers[index] = input;
		index++;
	}
	request.headers[index + 1] = NULL;
	if ( input != NULL )
	{
		free(input);
		input = NULL;
	}
}

int				fbxapi_request(
	const s_fbxapi *fbx,
	const char *verb,
	const char *endpoint,
	const char **headers,
	const char *body
)
{
	struct vlc_memstream	stream;
	char					*input;
	size_t					index;

	vlc_memstream_open(&stream);
	fbxapi_set_bases(&stream, fbx, verb, endpoint);
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

	fbx_get_response(fbx);

	return VLC_SUCCESS;
}
