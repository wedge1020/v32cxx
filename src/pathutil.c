#include <stdlib.h>
#include <string.h>
#include "pathutil.h"

char *replace_extension(const char *path, const char *new_ext) {
    const char *dot = strrchr(path, '.');
    const char *slash = strrchr(path, '/');
    size_t base_len = (dot != NULL && (slash == NULL || dot > slash))
                     ? (size_t)(dot - path)
                     : strlen(path);
    size_t ext_len = strlen(new_ext);
    char *result = malloc(base_len + ext_len + 1);
    memcpy(result, path, base_len);
    memcpy(result + base_len, new_ext, ext_len);
    result[base_len + ext_len] = '\0';
    return result;
}
