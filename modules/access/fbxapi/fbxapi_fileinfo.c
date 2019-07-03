#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <unistd.h>
#include <stdlib.h>

#include "../../misc/webservices/json.h"
#include "../../misc/webservices/json_helper.h"

#include "fbxapi_fileinfo.h"

s_fbxapi_fileinfo    *json_to_fileinfo(const json_value *as_json)
{
    const json_value    *obj;
    s_fbxapi_fileinfo   *file = NULL;

    file = calloc(1, sizeof(*file));
    if ( file == NULL )
    {
        return NULL;
    }

    if ( as_json->type != json_object )
    {
        return NULL;
    }

    obj = json_getbyname(as_json, "path");
    if ( obj == NULL || obj->type != json_string )
    {
        goto release;
    }
    file->path = strdup(obj->u.string.ptr);
    if ( file->path == NULL )
    {
        goto release;
    }

    obj = json_getbyname(as_json, "name");
    if ( obj == NULL || obj->type != json_string )
    {
        goto release;
    }
    file->name = strdup(obj->u.string.ptr);
    if ( file->name == NULL )
    {
        goto release;
    }

    obj = json_getbyname(as_json, "mimetype");
    if ( obj == NULL || obj->type != json_string )
    {
        goto release;
    }
    file->mimetype = strdup(obj->u.string.ptr);
    if ( file->mimetype == NULL )
    {
        goto release;
    }

    obj = json_getbyname(as_json, "type");
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

    obj = json_getbyname(as_json, "size");
    if ( obj == NULL || obj->type != json_integer )
    {
        goto release;
    }
    file->size = obj->u.integer;

    obj = json_getbyname(as_json, "modification");
    if ( obj == NULL || obj->type != json_integer )
    {
        goto release;
    }
    file->modification = obj->u.integer;

    obj = json_getbyname(as_json, "link");
    if ( obj == NULL || obj->type != json_boolean )
    {
        goto release;
    }
    file->is_link = obj->u.boolean;
    if ( file->is_link != 0 )
    {
        obj = json_getbyname(as_json, "target");
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

    obj = json_getbyname(as_json, "hidden");
    if ( obj == NULL || obj->type != json_boolean )
    {
        goto release;
    }
    file->is_hidden = obj->u.boolean;

    obj = json_getbyname(as_json, "index");
    if ( obj == NULL || obj->type != json_integer )
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
