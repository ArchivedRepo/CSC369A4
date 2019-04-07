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
struct ext2_super_block *sb;
struct ext2_group_desc *bd;
struct ext2_inode *inodes;
unsigned char *block_bitmap;
unsigned char *inode_bitmap;

// counter for fixes
int counter = 0;

// Count used blocks according to bitmap
int count_block();

// Count used inodes according to bitmap
int count_inode();

/* loop over the block used by a directory corresponding to an inode
 * index: index of inode (inode number - 1) 
 */
void check_directory(int index);

/* loop over the files in a block and check imode, inode bitmap and i_dtime
 * block: index of block
 */
void check_block(int block);


/* Helper function to check consistency of block bitmap
 * index : inode index in bitmap
 */
void check_data_block(int index);



int main(int argc, char** argv) {
    
    if(argc != 2) {
        fprintf(stderr, "Usage: ext2_checker <image file name>");
        exit(1);
    }

    // open image file
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

    sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
    bd = (struct ext2_group_desc *)(disk + (EXT2_BLOCK_SIZE) * 2);
    inodes = (struct ext2_inode*)(disk + (EXT2_BLOCK_SIZE)*bd->bg_inode_table);
    block_bitmap = (unsigned char *)(disk + EXT2_BLOCK_SIZE * bd->bg_block_bitmap);
    inode_bitmap = (unsigned char *)(disk + EXT2_BLOCK_SIZE * bd->bg_inode_bitmap);



    // check free blocks and inodes count
    int free_blocks_count = sb->s_blocks_count - count_block();
    int free_inodes_count = sb->s_inodes_count - count_inode();
    if(free_blocks_count != sb->s_free_blocks_count){
        int Z = abs(free_blocks_count - sb->s_free_blocks_count);
        sb->s_free_blocks_count = free_blocks_count;
        printf("Fixed: superblock's free blocks counter was off by %d compared to the bitmap\n", Z);
        counter += Z;
    }

    if(free_blocks_count != bd->bg_free_blocks_count){
        int Z = abs(free_blocks_count - bd->bg_free_blocks_count);
        bd->bg_free_blocks_count = free_blocks_count;
        printf("Fixed: block group's free blocks counter was off by %d compared to the bitmap\n", Z);
        counter += Z;
    }

    if(free_inodes_count != sb->s_free_inodes_count){
        int Z = abs(free_inodes_count - sb->s_free_inodes_count);
        sb->s_free_inodes_count = free_inodes_count;
        printf("Fixed: superblock's free inodes counter was off by %d compared to the bitmap\n", Z);
        counter += Z;
    }

    if(free_inodes_count != bd->bg_free_inodes_count){
        int Z = abs(free_inodes_count - bd->bg_free_inodes_count);
        bd->bg_free_inodes_count = free_inodes_count;
        printf("Fixed: block group's free inodes counter was off by %d compared to the bitmap\n", Z);
        counter += Z;
    }

    
    // check i_mode, i_node bitmap and i_dtime
    check_directory(EXT2_ROOT_INO - 1);


    // check consistency of block bitmap
    for(int byte = 0; byte < sb->s_inodes_count / 8; byte++){
        for (int bit = 0; bit < 8; bit++){
            unsigned char in_use = inode_bitmap[byte] & (1 << bit);
            if(in_use){
                check_data_block(byte*8 + bit);
            }
        }
    }
    
    // Summary of fixes
    if(counter == 0){
        printf("No file system inconsistencies detected!\n");
    }else{
        printf("%d file system inconsistencies repaired!\n", counter);
    }
    
    return 0;
}


// loop over the blocks used by a directory
void check_directory(int index) {
    struct ext2_inode inode = inodes[index];
    for (int i = 0; i < 12; i++) {
        if (inode.i_block[i] == 0) {
            return;
        }
        check_block(inode.i_block[i]);
    }
    
    // Single indirect block
    if (inode.i_block[12] != 0) {
        unsigned int *single_indirect_block = 
        (unsigned int *)(disk + inode.i_block[12] * EXT2_BLOCK_SIZE);
        for (int k = 0; k < 256; k++) {
            if (single_indirect_block[k] == 0) {
                return;
            }
            check_block(single_indirect_block[k]);
        }
    }
}

// loop over the files in the block to check and check the consistency of
// imode, inode bitmap and i_dtime

void check_block(int block) {
    
    int size = 0; //record total rec_len of blocks accessed
    unsigned char *dir = disk + EXT2_BLOCK_SIZE * block;
    struct ext2_dir_entry *this_dir = (struct ext2_dir_entry*)dir;

    while (size != EXT2_BLOCK_SIZE) {

        // check consistency of file type
        char type = 0;
        int type_fixed = 0;
        struct ext2_inode *this_inode = &inodes[this_dir->inode - 1];
        if ((this_inode->i_mode & EXT2_S_IFLNK) == EXT2_S_IFLNK) {
            type = 'l';
            if ((this_dir->file_type & EXT2_FT_SYMLINK) != EXT2_FT_SYMLINK) {
                this_dir->file_type = EXT2_FT_SYMLINK;
                type_fixed = 1;
            }
        } else if ((this_inode->i_mode & EXT2_S_IFREG) == EXT2_S_IFREG) {
            type = 'f';
            if ((this_dir->file_type & EXT2_FT_SYMLINK) != EXT2_FT_REG_FILE) {
                this_dir->file_type = EXT2_FT_REG_FILE;
                type_fixed = 1;
            }
        } else if ((this_inode->i_mode & EXT2_S_IFDIR) == EXT2_S_IFDIR) {
            type = 'd';
            if ((this_dir->file_type & EXT2_FT_SYMLINK) != EXT2_FT_DIR) {
                this_dir->file_type = EXT2_FT_DIR;
                type_fixed = 1;
            }
        }

        // update total fixes counter for file type fix
        if(type_fixed == 1){
            counter++;
            printf("Fixed: Entry type vs inode mismatch: inode [%d]\n", this_dir->inode);
        }


        // if is regular file, directory or symlink
        if(type != 0){
           
            // check whether inode is marked as in user in inode bitmap
            int byte = (this_dir->inode - 1)/8; 
            int bit = (this_dir->inode - 1) % 8;
            if( (inode_bitmap[byte] & (1 << bit)) == 0){
                *(inode_bitmap + byte) |= 1 << bit;
                sb->s_free_inodes_count--;
                bd->bg_free_inodes_count--;
                counter++;
                printf("Fixed: inode [%d] not marked as in-use\n", this_dir->inode);
            }


            // check deletion time
            if(this_inode->i_dtime != 0){
                this_inode->i_dtime = 0;
                counter++;
                printf("Fixed: valid inode marked for deletion: [%d]\n", this_dir->inode);
            }

            // check subdirectory
            // avoid checking current and parent dir and lost-and-found to avoid infinite loop and seg fault
            if(type == 'd' && this_dir->name[0]!='.' && this_dir->inode != 11){ 
                check_directory(this_dir->inode - 1);
            }
        }
        
        // go to the next file in this directory
        size += this_dir->rec_len;
        dir += this_dir->rec_len;
        this_dir = (struct ext2_dir_entry*)dir;
    }
    
}


// check the consistency of block bitmap
void check_data_block(int index) {
    char type = 0;
    struct ext2_inode this_inode = inodes[index];
    if ((this_inode.i_mode & EXT2_S_IFLNK) == EXT2_S_IFLNK) {
        type = 'l';
    } else if ((this_inode.i_mode & EXT2_S_IFREG) == EXT2_S_IFREG) {
        type = 'f';
    } else if ((this_inode.i_mode & EXT2_S_IFDIR) == EXT2_S_IFDIR) {
        type = 'd';
    }


    // if file is dir, regular file or symlink
    if(type != 0){
        int fixed = 0;

        // check whether the corresponding bit is set to one in bitmap for block in use 
        for (int k = 0; k < 12; k++) {
            if (this_inode.i_block[k] == 0) {
                break;
            } else {
                int block = this_inode.i_block[k] - 1;
                if(!( block_bitmap[block / 8] & (1 << (block % 8)) )){
                    block_bitmap[block/8] |= 1 << (block % 8);
                    sb->s_free_blocks_count--;
                    bd->bg_free_blocks_count--;
                    fixed++;
                }
            }
        }

        // check for single indirect block
        if (this_inode.i_block[12] != 0) {
            unsigned int *single_indirect_block = 
            (unsigned int *)(disk + this_inode.i_block[12] * EXT2_BLOCK_SIZE);
            for (int k = 0; k < 256; k++) {
                if (single_indirect_block[k] == 0) {
                    break;
                } else {
                   int block = single_indirect_block[k] - 1;
                    if(!( block_bitmap[block / 8] & (1 << (block % 8)) )){
                        block_bitmap[block/8] |= 1 << (block % 8);
                        sb->s_free_blocks_count--;
                        bd->bg_free_blocks_count--;
                        fixed++;
                    }
                }
            }
        }

        // update total fixes counter
        if(fixed > 0){
            counter+= fixed;
            printf("Fixed: %d in-use data blocks not marked in data bitmap for inode: [%d]\n", fixed, index+1);
        }
        
    } 
}

// count used block number
int count_block(){
    int block_counter = 0;   
    for(int byte = 0; byte < sb->s_blocks_count / 8; byte++){
        for (int bit = 0; bit < 8; bit++){
            unsigned char in_use = block_bitmap[byte] & (1 << bit);
            if(in_use){
                block_counter++;
            }
        }
    }
    return block_counter;
}

// count used inode number
int count_inode(){
    int inode_counter = 0;
    for(int byte = 0; byte < sb->s_inodes_count / 8; byte++){
        for (int bit = 0; bit < 8; bit++){
            unsigned char in_use = inode_bitmap[byte] & (1 << bit);
            if(in_use){
                inode_counter++;
            }
        }
    }
    
    return inode_counter;
}