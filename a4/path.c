#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include "path.h"
#include "ext2.h"

/**
 * try find the directory with name in the given block.
 * Return -1 if the name doesn't exist, return -2 if the name exist but is a
 * regular file
 */ 
static int find_directory(int block, char* name);

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
    
    return result;
    
}

int trace_path(char** path, int length) {
    struct ext2_group_desc *bd = (struct ext2_group_desc*)(disk + (EXT2_BLOCK_SIZE) * 2);
    struct ext2_inode *inodes = (struct ext2_inode*)
            (disk + EXT2_BLOCK_SIZE * bd->bg_inode_table);
    struct ext2_inode root_inode = inodes[EXT2_ROOT_INO - 1];
    if (length == 1) {
        return EXT2_ROOT_INO - 1;
    }
    
    struct ext2_inode cur_inode = root_inode;

    for (int i = 0; i < length - 1; i++) {
        char* target = path[i+1];
        int is_over = 0;
        int has_find = 0;
        int result = 0;
        for (int k = 0; k < 12 && !is_over; k++) {
            if (cur_inode.i_block[k] == 0) {
                is_over = 0;
                break;
            }
            result = find_directory(cur_inode.i_block[k], target);
            if (result > 0) {
                has_find = 1;
                break;
            } else if (result == -2) {
                return -ENOENT;
            }
        }
        if (has_find) {
            cur_inode = inodes[result - 1];
        }
        if (has_find && i == length - 2) {
            return result;
        }    
    }
    return -ENOENT;
}

/**
 * try find the directory with name in the given block.
 * Return -1 if the name doesn't exist, return -2 if the name exist but is not a
 * directory
 */ 
int find_directory(int block, char* name) {
    unsigned char *this_block = disk + block * EXT2_BLOCK_SIZE;
    struct ext2_dir_entry *this_dir = (struct ext2_dir_entry*)this_block;
    int size = 0;
    while (size != EXT2_BLOCK_SIZE) {
        size += this_dir->rec_len;
        char type = 0;
        if ((this_dir->file_type & EXT2_FT_SYMLINK) == EXT2_FT_SYMLINK) {
            type = 'l';
        } else if ((this_dir->file_type & EXT2_FT_REG_FILE) == EXT2_FT_REG_FILE) {
            type = 'f';
        } else if ((this_dir->file_type & EXT2_FT_DIR) == EXT2_FT_DIR) {
            type = 'd';
        } else {
            fprintf(stderr, "Unexpected type!\n");
        }
        char this_name[EXT2_NAME_LEN];
        strncpy(this_name, this_dir->name, EXT2_NAME_LEN);
        this_name[this_dir->name_len] = '\0';
        if (strcmp(this_name, name) == 0) {
            if (type == 'd') {
                return this_dir->inode;
            } else {
                return -2;
            }
        }
        this_dir = (struct ext2_dir_entry*)(this_block + size);
    }
    return -1;
}