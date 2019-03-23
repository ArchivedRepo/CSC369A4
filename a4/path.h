#define ERR_NAME_EXIST -2
#define ERR_NOT_EXIST -1
#define ERR_NO_INODE -1
#define ERR_NO_BLOCK -1

extern unsigned char *disk;

/**
 * try find the directory with name in the given block.
 * Return ERR_NOT_EXIST if the name doesn't exist,
 * return ERR_NAME_EXIST if the name exist but is not
 * a directory.
 */ 
int find_directory(int block, char* name);
int find_file(int block, char* name);

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
 * Try find the directory with given name in the given block.
 * Return ERR_NOT_EXIST if the name doesn't exist, 
 * return ERR_NAME_EXIST if the name already exist.
 */ 
int find_directory_inode(int inode, char* name); 
int find_file_inode(int inode, char* name);

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