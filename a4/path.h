extern unsigned char *disk;

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
 */ 
int trace_path(char** path, int length);