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


unsigned char *disk;

int main(int argc, char** argv) {

    int opt;
    char mode = 1;

    if ((opt = getopt(argc, argv, "s")) != -1){
        mode = 1;
    }

    if(argc != 4 + mode) {
        fprintf(stderr, "Usage: ext2_ln <image file name> (-s) <source path> <dest path>\n");
        exit(1);
    }


    // open disk image
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

    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    struct ext2_group_desc *bd = (struct ext2_group_desc*)(disk + (EXT2_BLOCK_SIZE) * 2);

    struct ext2_inode *inodes = 
    (struct ext2_inode*)(disk + (EXT2_BLOCK_SIZE)*bd->bg_inode_table);



    // find source
    int len_s;
    char **path_s = parse_path(argv[2+mode], &len_s);
    if (path_s == NULL) {
        return -1;
    }
    int source_directory = trace_path(path_s, len_s - 1);
    if (source_directory == -ENOENT) {
        fprintf(stderr, "Source path doesn't exist\n");
        return -ENOENT;
    }


    // TODO: align with path.c
    // find the inode for source file
    int source_inode = find_file_inode(source_directory, path_s[len_s-1]); 
    if (source_inode == -1) {
        fprintf(stderr, "Source file doesn't exist\n");
        return -ENOENT;
    }else if(source_inode == -2){
        // if work on hardlink
        if(mode == 0){
            fprintf(stderr, "Source file is directory\n");
            return -EISDIR;
        }
    }


    // find destination
    int length;
    char **path = parse_path(argv[3+mode], &length);
    if (path == NULL) {
        return -1;
    }
    int target_directory = trace_path(path, length - 1);
    if (target_directory == -ENOENT) {
        fprintf(stderr, "This path doesn't exist\n");
        return -ENOENT;
    }


    // Check whether the file already exist
    int find_result = find_directory_inode(target_directory, path[length-1]);
    
    if (find_result == -2) {
        fprintf(stderr, "There is a file has the name of the directory to create\n");
        return -EEXIST;
    } else if (find_result != -1) {
        //Should never reach here.
        assert(0);
    }


    struct ext2_dir_entry *new_entry = create_directory(target_directory, path[length-1]);
    
    if(mode == 0){
        new_entry->inode = source_inode;
        new_entry->file_type = EXT2_FT_REG_FILE;

        // Increase source file link count
        struct ext2_inode *this_inode = inodes + source_inode - 1; //-1 for the index in bitmap
        this_inode->i_links_count ++;

    }else{
        int new_inode = allocate_inode();
        if (new_inode == -1) {
            fprintf(stderr, "There is no inode available\n");
            return -ENOSPC;
        }
        new_entry->inode = new_inode + 1;
        new_entry->file_type = EXT2_FT_SYMLINK;

        // setting inode fields
        struct ext2_inode *this_inode = inodes + new_inode;
        this_inode->i_mode = EXT2_S_IFLNK;
        this_inode->i_uid = 0;
        this_inode->i_gid = 0;
        // this_inode->i_ctime = ;
        // this_inode->i_dtime = ;
        this_inode->i_links_count = 1;
        this_inode->osd1 = 0;
        this_inode->i_generation = 0;
        this_inode->i_size = strlen(argv[4]); 
        this_inode->i_blocks = 0;   
        memset(this_inode->i_block, 0, sizeof(unsigned int) * 15);

        // allocate new block to store link
        int new_block = allocate_block();
        if (new_block == -1) {
            fprintf(stderr, "There is no space on the disk!");
            return -ENOSPC;
        }
        this_inode->i_block[0] = new_block;
        this_inode->i_blocks += 2;

        // copying path into data block
        char *this_block = (char*)disk + EXT2_BLOCK_SIZE * new_block;
        strncpy(this_block, argv[3], strlen(argv[3]) + 1); // TODO: null terminate?
       
    }
    
    

    close(fd);
    return 0;
}
