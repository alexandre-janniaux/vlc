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

static int	connect( vlc_object_t * );
static void	disconnect( vlc_object_t * );

vlc_module_begin()
	set_shortname( "FBXAPI" )
	set_description( N_( "Freebox Api" ) )
	set_capability( "access", 42 )
	set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_ACCESS )
	add_shortcut( "fbxapi", "fbx-api" )
	set_callbacks( connect, disconnect )
vlc_module_end()

static ssize_t	fbxapi_read( stream_t *, void *, size_t );
static int		fbxapi_readdir( stream_t *, input_item_node_t * );
static int		fbxapi_seek( stream_t *, uint64_t );
static int		fbxapi_control( stream_t *, int, va_list );

struct		s_fbxapi
{
	char	name[24];
};
typedef struct s_fbxapi		s_fbxapi;


static int
connect( vlc_object_t *self )
{
	stream_t		*p_access = (stream_t *)self;
	s_fbxapi		*fbx = calloc(1, sizeof(*fbx));

	if (fbx == NULL)
	{
		return VLC_ENOMEM;
	}
	p_access->p_sys = fbx;
	printf("Function %s called\n", __FUNCTION__);
	p_access->pf_readdir = fbxapi_readdir;
	p_access->pf_read = fbxapi_read;
	p_access->pf_control = access_vaDirectoryControlHelper;
	p_access->pf_seek = fbxapi_seek;
	return VLC_SUCCESS;
}

static void
disconnect( vlc_object_t *self )
{
	printf("Function %s called\n", __FUNCTION__);
	(void)self;
}

static int
fbxapi_seek( stream_t *p_access, uint64_t u)
{
	(void)p_access;
	return 1;
}

static int
fbxapi_readdir( stream_t *p_access, input_item_node_t *current_node )
{
	const char	*uri = "fbxapi://where:4545?/tmp/path";

	strcpy(((s_fbxapi *)p_access->p_sys)->name, "test");
	printf("From %s -> '%s'\n", __FUNCTION__, ((s_fbxapi *)p_access->p_sys)->name);

	struct vlc_readdir_helper	rdh;

	vlc_readdir_helper_init( &rdh, p_access, current_node );

	vlc_readdir_helper_additem( &rdh, uri, "/tmp", "/tmp/path4", 1, ITEM_NET );
	vlc_readdir_helper_additem( &rdh, uri, "/tmp", "/tmp/path5", 1, ITEM_NET );
	vlc_readdir_helper_additem( &rdh, uri, "/tmp", "/tmp/path6", 1, ITEM_NET );
	vlc_readdir_helper_additem( &rdh, uri, "/tmp", "/tmp/path7", 1, ITEM_NET );
	vlc_readdir_helper_additem( &rdh, uri, "/tmp", "/tmp/path8", 1, ITEM_NET );
	vlc_readdir_helper_finish( &rdh, 1 );
	return VLC_SUCCESS;
}

static ssize_t
fbxapi_read( stream_t *p_access, void *buffer, size_t len )
{
	const char sample[] = "gdsgjsfdlgfdgjsfdkghsfdjkghsfdjghsfdlkgjsfdlghsfdlghsfdg";
	static int i = 0;

	printf("From %s -> '%s'\n", __FUNCTION__, ((s_fbxapi *)p_access->p_sys)->name);
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
	printf(
		"item '%s' '%s' %d \n",
		item->psz_name,
		item->psz_uri,
		item->i_es
	);

	size_t		l = len >= sizeof(sample) ? sizeof(sample) : len;
	memcpy(buffer, sample, l);
	return l;
}
