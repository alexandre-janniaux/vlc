#ifndef FBXAPI_APP_REGISTER
# define FBX_APP_REGISTER

# include <vlc_common.h>

# define FBXAPI_APP_ID "vlc_fbxapi"
# define FBXAPI_APP_NAME "VLC fbxapi"
# define FBXAPI_APP_VERSION "1.0.0"
# define FBXAPI_DEVICE_NAME "device"


char      *fbxapi_get_app_token( vlc_object_t *self );

#endif /* FBX_APP_REGISTER */
