#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include "path.h"
#include "ext2.h"

unsigned char *disk;

int main(int argc, char** argv) {
    
    if(argc != 3) {
        fprintf(stderr, "Usage: ext2_restore <image file name> <path to file> \n");
        exit(1);
    }

    int fd = open(argv[1], O_RDWR);
	if(fd == -1) {
		perror("open");
		exit(1);
    }

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    struct ext2_group_desc *bd = (struct ext2_group_desc*)(disk + (EXT2_BLOCK_SIZE) * 2);

    struct ext2_inode *inodes = 
    (struct ext2_inode*)(disk + (EXT2_BLOCK_SIZE)*bd->bg_inode_table);

    int length;
    char **path = parse_path(argv[2], &length);
    if (path == NULL) {
        fprintf(stderr, "The path to file is invalid. \n");
        return -1;
    }
    int target_directory = trace_path(path, length - 1);
    if (target_directory == -ENOENT) {
        fprintf(stderr, "The path to file is invalid. \n");
        return -ENOENT;
    }

    // Check whether the file to restore already exist
    int result = find_in_inode(target_directory, path[length-1], 'f');
    if (result > 0 || result == ERR_WRONG_TYPE) {
        fprintf(stderr, "The file you want to restor is already in directory\n");
        return -EEXIST;
    }

    struct ext2_inode *directory_inode = inodes + (target_directory - 1);
    
    // find to file to restore and restore it
    int is_over = 0;
    for (int i = 0; i < 12 && !is_over; i++) {
        if (directory_inode->i_block[i] == 0) {
            is_over = 1;
            break;
        }
        int this_block = directory_inode->i_block[i];
        int result = restore_entry_in_block(this_block, path[length-1]);
        if (result == RESTORE_SUCCESS) {
            return 0;
        } else if (result == ERR_WRONG_TYPE) {
            fprintf(stderr, "The file trying to restore is a directory\n");
            return -ENOENT;
        } else if (result == ERR_OVERWRITTEN) {
            fprintf(stderr, "The file trying to restore has been overwritten\n");
            return -ENOENT;
        }
    }
    // find in the single indirection block
    if (directory_inode->i_block[12] != 0) {
        unsigned int *indirect_block = (unsigned int*)
            (disk+EXT2_BLOCK_SIZE*directory_inode->i_block[12]);
        for (int i = 0; i < 256 && !is_over; i++) {
            if (indirect_block[i] == 0) {
                is_over = 1;
                break;
            }
            int this_block = indirect_block[i];
            int result = restore_entry_in_block(this_block, path[length-1]);
            if (result == RESTORE_SUCCESS) {
                return 0;
            } else if (result == ERR_WRONG_TYPE) {
                fprintf(stderr, "The file trying to restore is a directory\n");
                return -ENOENT;
            } else if (result == ERR_OVERWRITTEN) {
                fprintf(stderr, "The file trying to restore has been overwritten\n");
                return -ENOENT;
            }
        }
    }
    fprintf(stderr, "The file you want to restore is not found\n");
    return -ENOENT;
}
