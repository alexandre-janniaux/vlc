#ifndef FBXAPI_H
# define FBXAPI_H

struct		s_fbxapi
{
    struct
    {
        vlc_tls_t   *tls;
        char        *domain;
        int         port;
    }               http;
    /*
     * Informations about the api
     */
    char        *api_base_uri;
    int         api_version;

    /*
     * Informations about the application
     */
    char        *app_name;
    char        *app_version;
    char        *app_token;
    char        *device_name;

    /*
     *	Session woking on
     */
    char        *session_id;
};
typedef struct s_fbxapi     s_fbxapi;

#endif /* FBXAPI_H */
