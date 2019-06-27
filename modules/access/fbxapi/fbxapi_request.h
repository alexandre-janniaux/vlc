#ifndef FBXAPI_REQUEST_H
# define FBXAPI_REQUEST_H

# include "fbxapi.h"

# define FBX_ACCEPTED_ENCODING "deflate"

struct        s_fbxapi_request
{
    int     status_code;
    char    *status_text; // Ok, Not Found, etc
    char    **headers; // NULL terminated array, can be NULL
    char    *body; // Can be NULL
};
typedef struct s_fbxapi_request     s_fbxapi_request;

/**
 * Create a request and give back a response
 *
 * \param fbx A pointer to a valid AND connected fbxapi struture
 * \param request A pointer to a valid structure to fill. Sort of 2nd return
 * \param verb an http verb like GET or PUT
 * \param endpoint api's endpoint without destination/port part
 * \param headers an array of formed headers like {
 *                      "Authorization: gfdsgsfgv"
 *                  } or NULL
 *     \param body full formated request's body or NULL
 */
int        fbxapi_request(
    const s_fbxapi *fbx,
    s_fbxapi_request *request,
    const char *verb,
    const char *endpoint,
    const char **headers,
    const char *body
);

/**
 * Fill http's basic header in a memstream 
 *
 * \param stream The stream to fill
 * \param fbx
 * \param verb One of existing http verb ( GET, PUT, etc... )
 * \param http's request destination ( /api/version )
*/
void                fbxapi_set_request_bases(
    struct vlc_memstream *stream,
    const struct s_fbxapi *fbx,
    const char *verb,
    const char *endpoint
);

/**
 * Free all internal data ( all ), and memset it
 *
 * \param request A valid pointer to a valid request structure to be freed
 */
void    fbxapi_request_destroy(struct s_fbxapi_request *request);


#endif /* FBXAPI_REQUEST_H */
