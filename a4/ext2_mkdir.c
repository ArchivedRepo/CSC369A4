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
#include "path.h"
#include "ext2.h"

//TODO: Check what to return upon no inode/block available
//TODO: Implement single indirect in path.c
unsigned char *disk;

int main(int argc, char** argv) {
    
    if(argc != 3) {
        fprintf(stderr, "Usage: ext2_mkdir <image file name> <path>");
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
        return -1;
    }
    int target_directory = trace_path(path, length - 1);
    if (target_directory == -ENOENT) {
        fprintf(stderr, "This path doesn't exist\n");
        return -ENOENT;
    }

    int find_result = find_in_inode(target_directory, path[length-1], 'd');
    if (find_result > 0) {
        fprintf(stderr, "There is a file has the name of the directory to create\n");
        return -EEXIST;
    } else if (find_result != -1) {
        //Should never reach here.
        assert(0);
    }

    struct ext2_dir_entry *new_entry = create_directory(target_directory, path[length-1]);

    int new_inode = allocate_inode();
    if (new_inode == ERR_NO_INODE) {
        fprintf(stderr, "There is no inode available\n");
        return -ENOSPC;
    }
    new_entry->inode = new_inode + 1;
    new_entry->file_type = EXT2_FT_DIR;

    struct ext2_inode *this_inode = inodes + new_inode;
    this_inode->i_mode = EXT2_S_IFDIR;
    this_inode->i_size = 1024;
    this_inode->i_links_count = 2;
    this_inode->i_blocks = 2;
    this_inode->i_dtime = 0;
    memset(this_inode->i_block, 0, sizeof(unsigned int) * 15);
    int new_block = allocate_block();
    if (new_block == ERR_NO_BLOCK) {
        fprintf(stderr, "There is no space on the disk!");
        return -ENOSPC;
    }
    this_inode->i_block[0] = new_block;
    unsigned char *this_block = disk + EXT2_BLOCK_SIZE * new_block;
    struct ext2_dir_entry *cur_entry = (struct ext2_dir_entry*)this_block;
    cur_entry[0].inode = new_inode + 1;
    cur_entry[0].name_len = 1;
    cur_entry[0].file_type = EXT2_FT_DIR;
    cur_entry[0].name[0] = '.';
    cur_entry[0].name_len = 1;
    // The actual size is 9, but this should be a multiple of 4
    cur_entry[0].rec_len = 12;

    cur_entry = (struct ext2_dir_entry*)(this_block+12);
    cur_entry[0].inode = target_directory;
    cur_entry[0].name_len = 2;
    cur_entry[0].file_type = EXT2_FT_DIR;
    cur_entry[0].name[0] = '.';
    cur_entry[0].name[1] = '.';
    //The actual size is 10, but this is currently the last entry
    // rec_len is set to be 1012
    cur_entry[0].rec_len = 1012;
    bd->bg_used_dirs_count++;
    // Increase the link count of the parent directory
    struct ext2_inode *parent = &inodes[target_directory-1];
    parent->i_links_count++;
    return 0;
}
