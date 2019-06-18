#ifndef FBXAPI_REQUEST_H
# define FBXAPI_REQUEST_H

# include "fbxapi.h"

# define FBX_ACCEPTED_ENCODING "deflate"

struct		s_fbxapi_request
{
	int			status_code;
	char		*status_text; // Ok, Not Found, etc
	char		**headers;
	char		*body;
};
typedef struct s_fbxapi_request	s_fbxapi_request;

/**
 * Create a request and give back a response
 *
 * \param verb an http verb like GET or PUT
 * \param endpoint api's endpoint without destination/port part
 * \param headers an array of formed headers like {
 * 						"Authorization: gfdsgsfgv"
 * 					} or NULL
 * 	\param body full formated request's body or NULL
 */
int		fbxapi_request(
	const s_fbxapi*fbx,
	const char *verb,
	const char *endpoint,
	const char **headers,
	const char *body
);


#endif /* FBXAPI_REQUEST_H */
