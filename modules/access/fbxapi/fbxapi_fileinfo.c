#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <unistd.h>
#include <stdlib.h>

#include "../../misc/webservices/json.h"
#include "../../misc/webservices/json_helper.h"

#include "fbxapi_fileinfo.h"

s_fbxapi_fileinfo    *json_to_fileinfo(json_value *as_json)
{
    json_value    *obj;
    s_fbxapi_fileinfo    *file = NULL;

    file = calloc(1, sizeof(*file));
    if ( file == NULL )
    {
        return NULL;
    }

    if ( as_json->type != json_object )
    {
        return NULL;
    }

    obj = json_getbyname("path");
    if ( obj == NULL || obj->type != json_string )
    {
        goto release;
    }
    file->path = strdup(obj->u.string.ptr);
    if ( file->path == NULL )
    {
        goto release;
    }

    obj = json_getbyname("name");
    if ( obj == NULL || obj->type != json_string )
    {
        goto release;
    }
    file->name = strdup(obj->u.string.ptr);
    if ( file->name == NULL )
    {
        goto release;
    }

    obj = json_getbyname("mimetype");
    if ( obj == NULL || obj->type != json_string )
    {
        goto release;
    }
    file->mimetype = strdup(obj->u.string.ptr);
    if ( file->mimetype == NULL )
    {
        goto release;
    }

    obj = json_getbyname("type");
    if ( obj == NULL || obj->type != json_string )
    {
        goto release;
    }
    if ( strcmp(obj->u.string.ptr, "dir") == 0 )
    {
        file->type = T_DIR;
    }
    else if ( strcmp(obj->u.string.ptr, "file") == 0 )
    {
        file->type = T_FILE;
    }
    else
    {
        goto release;
    }

    obj = json_getbyname("size");
    if ( obj == NULL || obj->type != json_int_t )
    {
        goto release;
    }
    file->size = obj->u.integer;

    obj = json_getbyname("modification");
    if ( obj == NULL || obj->type != json_int_t )
    {
        goto release;
    }
    file->modification = obj->u.integer;

    obj = json_getbyname("link");
    if ( obj == NULL || obj->type != json_boolean )
    {
        goto release;
    }
    file->is_link = obj->u.boolean;
    if ( file->is_link != 0 )
    {
        obj = json_getbyname("target");
        if ( obj == NULL || obj->type != json_string )
        {
            goto release;
        }
        file->target = strdup(obj->u.string.ptr);
        if ( file->target == NULL )
        {
            goto release;
        }
    }

    obj = json_getbyname("hidden");
    if ( obj == NULL || obj->type != json_boolean )
    {
        goto release;
    }
    file->is_hidden = obj->u.boolean;

    obj = json_getbyname("index");
    if ( obj == NULL || obj->type != json_int_t )
    {
        goto release;
    }
    file->display_index = obj->u.integer;

    return file;

release:
    if ( file != NULL )
    {
        fbxapi_fileinfo_destroy(file);
        free(file);
    }
    return NULL;
}

void                fbxapi_fileinfo_destroy(s_fbxapi_fileinfo *fileinfo)
{
    if ( fileinfo != NULL )
    {
        if ( fileinfo->name != NULL )
        {
            free(fileinfo->name);
        }
        if ( fileinfo->path != NULL )
        {
            free(fileinfo->path);
        }
        if ( fileinfo->mimetype != NULL )
        {
            free(fileinfo->mimetype);
        }
        if ( fileinfo->target != NULL )
        {
            free(fileinfo->target);
        }
        memset(fileinfo, 0, sizeof(*fileinfo));
    }
}
