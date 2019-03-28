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
        fprintf(stderr, "Usage: ext2_rm <image file name> <path to delete>");
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
    struct ext2_super_block *sb = (struct ext2_super_block*)(disk + EXT2_BLOCK_SIZE);

    struct ext2_inode *inodes = 
    (struct ext2_inode*)(disk + (EXT2_BLOCK_SIZE)*bd->bg_inode_table);

    int length;
    char **path = parse_path(argv[2], &length);
    if (path == NULL) {
        fprintf(stderr, "Invalid Path\n");
        return -1;
    }
    int target_directory = trace_path(path, length - 1);
    if (target_directory == -ENOENT) {
        fprintf(stderr, "This path doesn't exist");
        return -ENOENT;
    }

    int find_result = find_in_inode(target_directory, path[length-1], 'f');
    if (find_result == ERR_WRONG_TYPE) {
        // Search again to see if this is a symbolic link;
        find_result = find_in_inode(target_directory, path[length-1], 'l');
        if (find_result == ERR_WRONG_TYPE) {
            fprintf(stderr, "%s is a directory\n", argv[2]);
            return -ENOENT;
        }
    } else if (find_result == ERR_NOT_EXIST) {
        fprintf(stderr, "File not Exists\n");
        return -ENOENT;
    }

    struct ext2_inode *directory_inode = inodes + (target_directory - 1);

    //find the directory entry of the file
    int is_over = 0;
    for (int i = 0; i < 12 && !is_over; i++) {
        if (directory_inode->i_block[i] == 0) {
            is_over = 0;
            break;
        }
        int block_num = directory_inode->i_block[i];
        int result = delete_entry_in_block(block_num, path[length-1]);
        if (result == DELETE_SUCCESS) {
            is_over = 1;
        }
    }
    if (!is_over && directory_inode->i_block[12] != 0) {
        unsigned int *indirect_block = (unsigned int*)(disk+EXT2_BLOCK_SIZE*directory_inode->i_block[12]);
        for (int i = 0; i < 256 && !is_over; i++) {
            if (indirect_block[i] == 0) {
                is_over = 0;
                break;
            }
            int block_num = indirect_block[i];
            int result = delete_entry_in_block(block_num, path[length-1]);
            if (result == DELETE_SUCCESS) {
                is_over = 1;
            }
        }
    }

    // update delete time, inode bitmap, block bitmap, group descriptor
    // and super block;
    
    struct ext2_inode *delete_file = inodes + (find_result - 1);
    unsigned char *inode_bitmap = disk + EXT2_BLOCK_SIZE * bd->bg_inode_bitmap;
    unsigned char *block_bitmap = disk + EXT2_BLOCK_SIZE * bd->bg_block_bitmap;
    time_t delete_time;
    time(&delete_time);
    delete_file->i_dtime = delete_time;
    *(inode_bitmap + (find_result-1) / 8) &= ~(1 << ((find_result - 1) % 8));
    bd->bg_free_inodes_count++;
    sb->s_free_inodes_count++;
    is_over = 0;
    for (int i = 0; i < 12 && !is_over; i++) {
        if (delete_file->i_block[i] == 0) {
            is_over = 1;
            break;
        }
        int this_block = delete_file->i_block[i];
        *(block_bitmap + this_block / 8) &= ~(1 << (this_block % 8));
        bd->bg_free_blocks_count++;
        sb->s_free_blocks_count++;
    }

    if (!is_over && delete_file->i_block[12] != 0) {
        unsigned int *indirect_block = (unsigned int*)(disk + EXT2_BLOCK_SIZE * delete_file->i_block[12]);
        for (int i = 0; i < 256 && !is_over; i++) {
            if (indirect_block[i] == 0) {
                is_over = 1;
                break;
            }
            int this_block = indirect_block[i];
            *(block_bitmap + this_block / 8) &= ~(1 << (this_block % 8));
            bd->bg_free_blocks_count++;
            sb->s_free_blocks_count++;
        }
        int this_block = delete_file->i_block[12];
        *(block_bitmap + this_block / 8) &= ~(1 << (this_block % 8));
        bd->bg_free_blocks_count++;
    }
    return 0;
}