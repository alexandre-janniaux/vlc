#ifndef FBXAPI_FS
# define FBX_FS

# include <nettle/base64.h>

# include <vlc_common.h>

# include "fbxapi_fileinfo.h"

static inline char  *path_encode( const char *path )
{
    size_t  encoded_length = BASE64_ENCODE_RAW_LENGTH(strlen(path));
    uint8_t *encoded;

    encoded = calloc(encoded_length + 1, sizeof(char));
    if ( encoded != NULL )
    {
        base64_encode_raw(path, encoded_length, encoded);
    }
    return encoded;
}

/**
 * Retrieve statisiques about a given file
 *
 * \param access
 * \param fbx
 * \param file_name Full path of a file
 */
s_fbxapi_fileinfo    *fbxapi_stat(
    stream_t *access,
    s_fbxapi *fbx,
    const char *file_name
);

/**
 * List directorie's content
 *
 * \param access
 * \param fbx
 * \param file_name Full path of a directory
 */
s_fbxapi_fileinfo    **fbxapi_ls(
    stream_t *access,
    s_fbxapi *fbx,
    const char *file_name
);

#endif /* FBX_FS */
