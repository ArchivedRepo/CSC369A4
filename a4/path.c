#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "path.h"

/**
 * parse the path provided and return an array of all the folder tokens in 
 * the path, change length to the length of the path. Note that the first 
 * token in the returned array is always "/"
 * If the path is invalid, return NULL and left length unset.
 * The returned array and the strings in it is dynamically allocated and need to
 * be freed.
 */
char** parse_path(char *path, int *length) {
    if (path[0] == '\0' || path[0] != '/') {
        fprintf(stderr, "Wrong format");
        return NULL;
    }
    //count the number of token in the first pass
    int str_len = strlen(path);
    int count = 1;
    int last_slash = 1;
    for (int i = 1; i < str_len; i++) {
        if (!last_slash && path[i] == '/') {
            count++;
            last_slash = 1;
        } else if (last_slash && path[i] != '/') {
            last_slash = 0;
        }
    }
    if (path[str_len - 1] != '/') {
        count++;
    }
    *length = count;

    //second pass to find the length of each token
    int token_lens[count];
    token_lens[0] = 1;

    int this_count = 0;
    last_slash = 1;
    int k = 1;
    for (int i = 1; i < str_len; i++) {
        if (!last_slash && path[i] == '/') {
            token_lens[k] = this_count;
            k++;
            this_count = 0;
            last_slash = 1;
        } else if (last_slash && path[i] != '/') {
            last_slash = 0;
            this_count++;
        } else {
            this_count++;
        }
    }
    if (path[str_len - 1] != '/') {
        token_lens[k] = this_count;
    }
    
    // Third pass to construct the return string
    char **result = malloc(sizeof(char*) * count);
    for (int i = 0; i < count; i++) {
        result[i] = malloc(sizeof(char) * (token_lens[i] + 1));
    }
    strcpy(result[0], "/");

    // for (int i = 0; i < count; i++) {
    //     printf(" %d", token_lens[i]);
    // }
    // printf("\n");

    k = 1;
    last_slash = 1;
    this_count = 0;
    for (int i = 1; i < str_len; i++) {
        if (!last_slash && path[i] == '/') {
            result[k][this_count] = '\0';
            k++;
            last_slash = 1;
            this_count = 0;
        } else if (last_slash && path[i] != '/') {
            last_slash = 0;
            result[k][this_count] = path[i];
            this_count++;
        } else if (path[i] != '/') {
            result[k][this_count] = path[i];
            this_count++;
        }
    }
    

    for (int i = 0; i < count; i++) {
        printf("%s\n", result[i]);
    }
    return NULL;
    
}