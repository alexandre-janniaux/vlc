#ifndef FBXAPI_FILEINFO_H
# define FBXAPI_FILEINFO_H

struct    s_fbxapi_fileinfo
{
    // path here are decoded
    char    *path;
    char    *name;
    char    *mimetype;
    enum
    {
        T_DIR,
        T_FILE,
    }        type;

    // File size in bytes
    int        size;

    // Modification timestamp
    int        modification;

    // Symbolic link
    int        is_link;
    char    *target;

    int        is_hidden;
    int        display_index;
};
typedef struct s_fbxapi_fileinfo s_fbxapi_fileinfo;

/*
 * Takes a json object formatted by the freebox fs api and converts it to a 
 * s_fbxapi_fileinfo ( data is dupplicated )
 */
s_fbxapi_fileinfo    *json_to_fileinfo(const json_value *as_json);

/*
 * Free ressources contained in a s_fbxapi_fileinfo structure
 * Does not free the structure itself
 */
void                fbxapi_fileinfo_destroy(s_fbxapi_fileinfo *);

#endif /* FBXAPI_FILEINFO_H */
