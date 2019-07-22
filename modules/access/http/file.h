/*****************************************************************************
 * file.h: HTTP read-only file
 *****************************************************************************
 * Copyright (C) 2015 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include <stdint.h>

/**
 * \defgroup http_file Files
 * HTTP read-only files
 * \ingroup http_res
 * @{
 */

struct vlc_http_mgr;
struct vlc_http_resource;
struct block_t;

/**
 * \see vlc_http_resource_cbs */
struct vlc_http_file_cbs
{
    /* Request the vlc_http_file user to finish the formatting of the request.
     *
     * During the execution of this callback, the user can finish adding
     * headers before the request is sent.
     *
     * \param res the file resource being processed
     * \param req the request that the user must complete
     * \param opaque userdata filled by the user
     * \ret TODO
     */
    int (*request_format)(const struct vlc_http_file *file,
                          struct vlc_http_msg *req,
                          void *opaque);
};


/**
 * Creates an HTTP file.
 *
 * Allocates a structure for a remote HTTP-served read-only file.
 *
 * @param url URL of the file to read
 * @param ua user agent string (or NULL to ignore)
 * @param ref referral URL (or NULL to ignore)
 *
 * @return an HTTP file resource object pointer, or NULL on error
 */
struct vlc_http_file *vlc_http_file_create(struct vlc_http_mgr *mgr,
                                           const char *url, const char *ua,
                                           const char *ref);
/**
 * Creates an HTTP file with additional callbacks.
 *
 * Allocates a structure for a remote HTTP-served read-only file.
 *
 * @param url URL of the file to read
 * @param ua user agent string (or NULL to ignore)
 * @param ref referral URL (or NULL to ignore)
 * @param opaque user data to be given to callbacks
 * @param cbs callbacks called when request is being written
 *
 * @return an HTTP file resource object pointer, or NULL on error
 */

struct vlc_http_file *vlc_http_file_extend(struct vlc_http_mgr *mgr,
                                           const char *uri, const char *ua,
                                           const char *ref, void *opaque,
                                           const struct vlc_http_file_cbs *cbs);

/**
 * Gets file size.
 *
 * Determines the file size in bytes.
 *
 * @return Bytes count or (uintmax_t)-1 if unknown.
 */
uintmax_t vlc_http_file_get_size(struct vlc_http_file *);

/**
 * Checks seeking support.
 *
 * @retval true if file supports seeking
 * @retval false if file does not support seeking
 */
bool vlc_http_file_can_seek(struct vlc_http_file *);

/**
 * Sets the read offset.
 *
 * @param offset byte offset of next read
 * @retval 0 if seek succeeded
 * @retval -1 if seek failed
 */
int vlc_http_file_seek(struct vlc_http_file *, uintmax_t offset);

/**
 * Reads data.
 *
 * Reads data from a file and update the file offset.
 */
struct block_t *vlc_http_file_read(struct vlc_http_file *);

/**
 * Get the underlying resource object.
 */
struct vlc_http_resource *vlc_http_file_resource(struct vlc_http_file *);

void vlc_http_file_destroy(struct vlc_http_file *);

#define vlc_http_file_get_status(f) \
    vlc_http_res_get_status(vlc_http_file_resource(f))

#define vlc_http_file_get_redirect(f) \
    vlc_http_res_get_redirect(vlc_http_file_resource(f))

#define vlc_http_file_get_type(f) \
    vlc_http_res_get_type(vlc_http_file_resource(f))

/** @} */
