#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include "path.h"
#include "ext2.h"

/**
 * Find the last non-zero entry in the i_block.
 */
static int find_last_nonzero(unsigned int *i_block) {
    //The fisr entry should never be 0!
    assert(i_block[0] != 0);
    for (int i = 1; i < 15; i++) {
        if (i_block[i] == 0) {
            return i-1;
        }
    }
    return -1;
}

/**
 * Find the last non-zero entry in a 1024 length array
 */ 
static int find_last_nonzero_1024(unsigned int* arr) {
    assert(arr[0] != 0);
    for (int i = 1; i < 1024 - 1; i++) {
        if (arr[i] == 0) {
            return i -1;
        }
    }
    return 1023;
}


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
        return EXT2_ROOT_INO;
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
                return ERR_NAME_EXIST;
            }
        }
        this_dir = (struct ext2_dir_entry*)(this_block + size);
    }
    return ERR_NOT_EXIST;
}

int find_directory_inode(int inode, char* name) {
    struct ext2_group_desc *bd = (struct ext2_group_desc*)(disk + (EXT2_BLOCK_SIZE) * 2);
    struct ext2_inode *inodes = 
    (struct ext2_inode*)(disk + (EXT2_BLOCK_SIZE)*bd->bg_inode_table);
    struct ext2_inode this_inode = inodes[inode - 1];
    int is_over = 0;
    for (int i = 0; i < 12 && !is_over; i++) {
        if (this_inode.i_block[i] == 0) {
            is_over = 1;
            break;
        }
        int result = find_directory(this_inode.i_block[i], name);
        if (result > 0 || result == -2) {
            return ERR_NAME_EXIST;
        }
    }
    return ERR_NOT_EXIST;
}


int find_file(int block, char* name) {
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
            if (type != 'd') {
                return this_dir->inode;
            } else {
                return -2;
            }
        }
        this_dir = (struct ext2_dir_entry*)(this_block + size);
    }
    return -1;
}

int find_file_inode(int inode, char* name) {
    struct ext2_group_desc *bd = (struct ext2_group_desc*)(disk + (EXT2_BLOCK_SIZE) * 2);
    struct ext2_inode *inodes = 
    (struct ext2_inode*)(disk + (EXT2_BLOCK_SIZE)*bd->bg_inode_table);
    struct ext2_inode this_inode = inodes[inode - 1];
    int is_over = 0;
    for (int i = 0; i < 12 && !is_over; i++) {
        if (this_inode.i_block[i] == 0) {
            is_over = 1;
            break;
        }
        int result = find_file(this_inode.i_block[i], name);
        if (result > 0 || result == -2) {
            return result;
        }
    }
    return -1;
}

int allocate_inode() {
    struct ext2_group_desc *bd = (struct ext2_group_desc*)(disk + EXT2_BLOCK_SIZE * 2);
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    char *inode_bitmap = (char*)(disk + EXT2_BLOCK_SIZE * bd->bg_inode_bitmap);
    int inode_amount = ((struct ext2_super_block *)(disk + 1024))->s_inodes_count;
    for (int i = 0; i < inode_amount; i++) {
        if (!(*(inode_bitmap + i / 8) & (1 << (i % 8)))) {
            *(inode_bitmap + i/8) |= 1 << (i % 8);
            sb->s_free_inodes_count--;
            bd->bg_free_inodes_count--;
            return i;
        }
    }
    return ERR_NO_INODE;
}

int allocate_block() {
    struct ext2_group_desc *bd = (struct ext2_group_desc*)(disk + EXT2_BLOCK_SIZE * 2);
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    char *block_bitmap = (char*)(disk+ EXT2_BLOCK_SIZE * bd->bg_block_bitmap);
    int block_count = ((struct ext2_super_block *)(disk + 1024))->s_blocks_count;
    for (int i = 0; i < block_count; i++) {
        if (!(*(block_bitmap + i / 8) & (1 << (i % 8)))) {
            *(block_bitmap + i/8) |= 1 << (i % 8);
            sb->s_free_blocks_count--;
            bd->bg_free_blocks_count--;
            return i+1;
        }
    }
    return ERR_NO_BLOCK;
}

/**
 * Try find space and allocate an ext2_dir_entry in the given block.
 * Return the pointer to the struct on success, return NULL on Fail
 */ 
static struct ext2_dir_entry* find_space_in_block(unsigned char *block, char *name) {
    int size = 0;
    struct ext2_dir_entry *cur_entry = (struct ext2_dir_entry*)block;
    size += cur_entry->rec_len;
    while (size != EXT2_BLOCK_SIZE) {
        cur_entry = (struct ext2_dir_entry*)(block + size);
        size += cur_entry->rec_len;
    }
    int length_needed = strlen(name) + 8;
    int actual_length = 8 + cur_entry->name_len;
    if (actual_length % 4 != 0) {
        actual_length += 4 - actual_length % 4;
    }
    printf("%d\n", actual_length);
    printf("%d\n", cur_entry->rec_len);
    size -= cur_entry->rec_len;
    int length_left = cur_entry ->rec_len - actual_length;
    if (length_left > length_needed) {
        cur_entry->rec_len = actual_length;
        cur_entry = (struct ext2_dir_entry*)(block + size + actual_length);
        size += actual_length;
        printf("%d\n", size);
        cur_entry->rec_len = EXT2_BLOCK_SIZE - size;
        cur_entry->name_len = strlen(name);
        for (int i = 0; i < strlen(name); i++) {
            cur_entry->name[i] = name[i];
        }
        return cur_entry;
    }
    return NULL;
}


/**
 * Create a new directory entry in the given inode with provided name.
 * This function simple find the space, but left inode and file_typr unset.
 * Note: 1. the inode number provided must be an entry
 *       2. the inode number provided will be minus one to index the inode
 * Return a pointer to the new ext2_dir_entry on success, return NULL on failure.
 */ 
struct ext2_dir_entry* create_directory(int inode, char *name) {
    struct ext2_group_desc *bd = (struct ext2_group_desc*)(disk + (EXT2_BLOCK_SIZE) * 2);

    struct ext2_inode *inodes = 
    (struct ext2_inode*)(disk + (EXT2_BLOCK_SIZE)*bd->bg_inode_table);
    struct ext2_inode *this_inode = inodes + (inode - 1);
    int last_nonzero = find_last_nonzero(this_inode->i_block);
    // We don't deal with double/triple indirect
    assert(last_nonzero < 14);

    if (last_nonzero <= 10) {
        unsigned char *this_block = disk + EXT2_BLOCK_SIZE * (this_inode->i_block)[last_nonzero];
        struct ext2_dir_entry *result = find_space_in_block(this_block, name);
        if (result != NULL) {
            return result;
        }
        //need a new block
        last_nonzero++;
        int new_block = allocate_block();
        if (new_block == -1) {
            fprintf(stderr, "There is no space left on disk");
            exit(-ENOSPC);
        }
        (this_inode->i_block)[last_nonzero] = new_block;
        // Clear the disk block
        memset(disk+EXT2_BLOCK_SIZE*new_block, 0, EXT2_BLOCK_SIZE);
        struct ext2_dir_entry *this_dir = (struct ext2_dir_entry*)
            (disk + EXT2_BLOCK_SIZE * new_block);
        this_dir->inode = inode;
        this_dir->name_len = strlen(name);
        for (int i = 0; i < this_dir->name_len; i++) {
            this_dir->name[i] = name[i];
        }
        // This is the only directory, set length to 1024
        this_dir->rec_len = 1024;
        return this_dir;
    } else if (last_nonzero == 11) {// the nonzero is the last direct block {
        unsigned char *this_block = disk + EXT2_BLOCK_SIZE * (this_inode->i_block)[last_nonzero];
        struct ext2_dir_entry *result = find_space_in_block(this_block, name);
        if (result != NULL) {
            return result;
        }
        //Need a new block with single indirecton
        int new_indirect_block = allocate_block();
        if (new_indirect_block == -1) {
            fprintf(stderr, "There is no space on the disk!");
            exit(-ENOSPC);
        }
        //Clear the new block
        memset(disk+EXT2_BLOCK_SIZE*new_indirect_block, 0, 1024);
        (this_inode->i_block)[12] = new_indirect_block;
        unsigned int *indirect_blocks = 
        (unsigned int *)(disk + EXT2_BLOCK_SIZE * new_indirect_block);
        int new_block = allocate_block();
        if (new_block == -1) {
            fprintf(stderr, "There is no space on the disk");
            exit(-ENOSPC);
        }
        indirect_blocks[0] = new_block;
        memset(disk+EXT2_BLOCK_SIZE*new_block, 0, 1024);
        struct ext2_dir_entry *this_dir = (struct ext2_dir_entry*)
            (disk + EXT2_BLOCK_SIZE * new_block);
        this_dir->inode = inode;
        this_dir->name_len = strlen(name);
        for (int i = 0; i < this_dir->name_len; i++) {
            this_dir->name[i] = name[i];
        }
        // This is the only directory, set length to 1024
        this_dir->rec_len = 1024;
        return this_dir;
    } else if (last_nonzero == 12) {
        unsigned int *blocks = (unsigned int*)(disk + EXT2_BLOCK_SIZE*(this_inode->i_block)[12]);
        int last_nonzero = find_last_nonzero_1024(blocks);
        unsigned char *this_block = 
        disk + EXT2_BLOCK_SIZE * blocks[last_nonzero];
        struct ext2_dir_entry *result = find_space_in_block(this_block, name);
        if (result != NULL) {
            return result;
        } else if (result == NULL && last_nonzero == 1023) {
            fprintf(stderr, "Unable to handle the case that need double indirect");
            exit(-ENOSPC);
        }
        last_nonzero++;
        int new_block = allocate_block();
        if (new_block == -1) {
            fprintf(stderr, "There is no space on the disk\n");
            exit(-ENOSPC);
        }
        blocks[last_nonzero] = new_block;
        memset(disk+EXT2_BLOCK_SIZE*new_block, 0, 1024);
        struct ext2_dir_entry *new_entry = (struct ext2_dir_entry*)
        (disk + EXT2_BLOCK_SIZE * new_block);
        new_entry->inode = inode;
        new_entry->rec_len = 1024;
        new_entry->name_len = strlen(name);
        for (int i = 0; i < new_entry->name_len; i++) {
            new_entry->name[i] = name[i];
        }
        return new_entry;
    } else {
        //should not reach here
        assert(0);
    }
    return NULL;
}