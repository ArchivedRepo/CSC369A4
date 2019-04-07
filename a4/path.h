#define ERR_NOT_EXIST -1
#define ERR_NO_INODE -1
#define ERR_NO_BLOCK -1
#define ERR_WRONG_TYPE -3
#define DELETE_SUCCESS 0
#define RESTORE_SUCCESS 0
#define ERR_OVERWRITTEN -4

extern unsigned char *disk;

/**
 * try find the directory entry with name and given type in the given block.
 * Return the inode number of the file on found.
 * Return ERR_NOT_EXIST if the name doesn't exist,
 * Return ERR_WRONG_TYPE if the name exist but no as given type
 */ 
int find_in_block(int block, char* name, char type);

/**
 * parse the path provided and return an array of all the folder tokens in 
 * the path, change length to the length of the path. Note that the first 
 * token in the returned array is always "/"
 * If the path is invalid, return NULL and left length unset.
 * The returned array and the strings in it is dynamically allocated and need to
 * be freed.
 */
char** parse_path(char *path, int *length);

/**
 * Trace the path and return the inode number of the target directory.
 * Input: path is the path want to trace, length is the number of token in 
 * the path, include the "/"
 * Example: input path: /, usr, local, return the inode number of local.
 * Return ENOENT on incorrect path. 
 * Note: The inode number returned need to be minus 1 when used to find the 
 * inode in the array.
 */ 
int trace_path(char** path, int length);

/**
 * Try find the directory with given name and type in the given inode.
 * Return the inode number of the file on found.
 * Return ERR_NOT_EXIST if the name doesn't exist, 
 * Return ERR_WRONG_TYPE if the name exist but no as given type
 */ 
int find_in_inode(int inode, char* name, char type);

/**
 * Allocate an inode and mark the inode to be in use in the bitmap.
 * Return the inode index on success, return ERR_NO_BLOCK if no inode is available
 * Note: the number returned in this function is the actual index in the 
 * inodes array.
 */
int allocate_inode(); 

/**
 * Allocate an block and mark the inode to be in use in the bitmap. 
 * Return the block index on success, return ERR_NO_INODE if no block is available.
 */ 
int allocate_block();

/**
 * Create a new directory entry in the given inode with provided name.
 * This function simple find the space, but left inode and file_type unset.
 * Note: 1. the inode number provided must be an entry
 *       2. the inode number provided should be index(i.e. don't need to minus 1)
 * Return a pointer to the new ext2_dir_entry on success, return NULL on failure.
 */ 
struct ext2_dir_entry* create_directory(int inode, char *name);

/**
 * Try delete the file in the block;
 * Return DELETE_SUCCESS on success, return ERR_NOT_EXIST on not found
 */ 
int delete_entry_in_block(int block, char *name);

/**
 * Try restore the file with name in the given block;
 * Return RESTORE_SUCCESS on success, return ERR_NOT_EXIST on not found
 * Return ERR_WRONG_TYPE if the entry try to restore is a directory
 * Return ERR_OVERWRITTEN if the entry inode or the datablock in the inode
 * has been reallocated
 */
int restore_entry_in_block(int block, char *name);