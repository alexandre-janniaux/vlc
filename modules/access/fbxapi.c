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

/*
 * freebox's base uri is given by freebox itself
 */
#define BASE_URI_SIZE	16
/*
 * freebox's domain is given by freebox itself with format "garbage.fbxos.fr"
 */
#define DOMAIN_SIZE		32

#define APP_NAME_SIZE	16

static int	fbxapi_connect( vlc_object_t * );
static void	fbxapi_disconnect( vlc_object_t * );

vlc_module_begin()
	set_shortname( "FBXAPI" )
	set_description( N_( "Freebox Api" ) )
	set_capability( "access", 42 )
	set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )
	add_shortcut( "fbxapi", "fbx-api" )
	set_callbacks( fbxapi_connect, fbxapi_disconnect )
vlc_module_end()

static ssize_t	fbxapi_read( stream_t *, void *, size_t );
static int		fbxapi_readdir( stream_t *, input_item_node_t * );
static int		fbxapi_seek( stream_t *, uint64_t );
static int		fbxapi_control( stream_t *, int, va_list );

struct		s_fbxapi
{
	struct
	{
		vlc_tls_t	*tls;
	}				http;
	/*
	 * Informations about the api
	 */
	char			*api_base_uri;
	char			*api_domain;
	unsigned int	https_port;
	int				api_version;

	/*
	 * Informations about the application
	 */
	char			*app_name;
	char			*app_version;
	char			*app_token;
	char			*device_name;

	/*
	 *	Session woking on
	 */
	char			*session_id;
};
typedef struct s_fbxapi		s_fbxapi;

/*static int
fbxapi_rest_connect( stream_t *p_access )
{
	const char	base_get_challenge[] = "/login";
	const char	base_get_login[] = "/login/session";
}*/

static int		fbxapi_open_tls(stream_t *access)
{
	vlc_url_t		url;
	s_fbxapi		*fbx;

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
	if (fbx->http.tls == NULL)
	{
		free(fbx);
		access->p_sys = fbx = NULL;
		return VLC_ENOMEM;
	}
	return VLC_SUCCESS;
}

static int		fbxapi_connect( vlc_object_t *self )
{
	stream_t		*access = (stream_t *)self;
	s_fbxapi		*fbx;
	int				res;

 	access->p_sys = fbx = calloc(1, sizeof(*fbx));
	if (fbx == NULL)
	{
		return VLC_ENOMEM;
	}
	res = fbx_open_tls(access);
	if (res != VLC_SUCCESS)
	{
		return res;
	}

	access->pf_control = access_vaDirectoryControlHelper;
	access->pf_seek = fbxapi_seek;

/*	if (dummy++ == 0)
	{
		p_access->pf_readdir = fbxapi_readdir;
	}
	else
	{
		p_access->pf_read = fbxapi_read;
	}
*/
	return VLC_SUCCESS;
}

static void		fbxapi_disconnect( vlc_object_t *self )
{
	stream_t		*p_access = (stream_t *)self;
	s_fbxapi		*fbx = p_access->p_sys;

	vlc_tls_SessionDelete(fbx->http.tls);
	free(p_access->p_sys);
	p_access->p_sys = fbx = NULL;
	(void)self;
}

static int		fbxapi_seek( stream_t *p_access, uint64_t u )
{
	(void)p_access;
	return 1;
}

static int		fbxapi_readdir(
	stream_t *p_access,
	input_item_node_t *current_node
)
{
	char	uri[] = "fbxapi://where:4545?/tmp/path9";
	char	file_name[] = "/tmp/path4";

	printf(
		"From %s -> '%s' %s\n",
		__FUNCTION__,
		p_access->psz_name,
		p_access->psz_url
	);

	struct vlc_readdir_helper	rdh;

	vlc_readdir_helper_init( &rdh, p_access, current_node );

	for (int i = 0; i < 5; i++)
	{
		uri[sizeof(uri) - 2] = (char)('0' + i);
		file_name[sizeof(file_name) - 2] = (char)('0' + i);
		vlc_readdir_helper_additem( &rdh, (uri), "/tmp", (file_name), 1, ITEM_NET );
	}
	vlc_readdir_helper_finish( &rdh, 1 );
	return VLC_SUCCESS;
}

static ssize_t		fbxapi_read( stream_t *p_access, void *buffer, size_t len )
{
	const char sample[] = "gdsgjsfdlgfdgjsfdkghsfdjkghsfdjghsfdlkgjsfdlghsfdlghsfdg";
	static int i = 0;

	printf("From %s\n", __FUNCTION__);
	if (i != 0)
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

	size_t		l = len >= sizeof(sample) ? sizeof(sample) : len;
	memcpy(buffer, sample, l);
	return l;
}
