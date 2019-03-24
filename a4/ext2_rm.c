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

    struct ext2_inode *inodes = 
    (struct ext2_inode*)(disk + (EXT2_BLOCK_SIZE)*bd->bg_inode_table);

    int length;
    char **path = parse_path(argv[2], &length);
    if (path == NULL) {
        return -1;
    }
    int target_directory = trace_path(path, length - 1);
    if (target_directory == -ENOENT) {
        fprintf(stderr, "This path doesn't exist");
        return -ENOENT;
    }

    int find_result = find_file_inode(target_directory, path[length-1]);
    if (find_result == -1) {
        fprintf(stderr, "The file does not exist!");
        return -ENOENT;
    }

    struct ext2_inode *directory_inode = inodes + (target_directory - 1);

    
    return 0;
}