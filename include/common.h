#ifndef COMMON_H
#define COMMON_H
typedef enum Error {
    FS_OK = 0,                         // success
    FS_ERR_NULL,                       // a required pointer argument is NULL
    FS_ERR_INVALID_BLOCK,              // block index is outside [0, MAXIMUM_BLOCKS - 1]
    FS_ERR_NOT_OPEN,                   // trying to read/write/close a disk that is not open
    FS_ERR_ALREADY_OPEN,               // file descriptor is already opened
    FS_ERR_CREATE,                     // creating the disk image failed
    FS_ERR_OPEN,                       // opening an existing disk image failed
    FS_ERR_SEEK,                       // moving to the requested block offset failed
    FS_ERR_READ,                       // disk read failed
    FS_ERR_WRITE,                      // disk write failed
    FS_ERR_CLOSE,                      // closing the disk file failed
    FS_ERR_STAT,                       // retrieving file information failed
    FS_ERR_INVALID_DISK,               // invalid disk characteristics
    FS_ERR_INDEX_OUT_OF_BOUNDS,        // invalid index bounds
    FS_ERR_BITMAP_IS_FULL,             // bitmap has not free space
    FS_ERR_USED_BIT,                   // bit is already used
    FS_ERR_INODE_IS_ALREADY_FREE,      // inode is already clear
    FS_ERR_PROTECTED_INODE,            // inode cannot be freed
    FS_ERR_NO_FREE_INODES,             // no free space in inode bitmap
    FS_ERR_NO_FREE_DATA_BLOCKS,        // no free space in data bitmap
    FS_ERR_DATA_BLOCK_IS_ALREADY_FREE, // data block is already clear
    FS_ERR_PROTECTED_BLOCK,            // data block cannot be freed
    FS_ERR_ALREADY_MOUNTED,            // filesystem is already mounted
    FS_ERR_NOT_MOUNTED,                // filesystem is not mounted
    FS_ERR_ENTRY_NOT_FOUND,            // directory entry not found
    FS_ERR_INVALID_FILE_NAME,          // invalid file name
    FS_ERR_ENTRY_IS_NOT_DIRECTORY,     // entry type is not directory
    FS_ERR_ENTRY_IS_NOT_FILE,          // entry type is not file
    FS_ERR_ENTRY_ALREADY_EXISTS,       // entry exists
    FS_ERR_DIRECTORY_IS_FULL,          // full directory
    FS_ERR_INVALID_PATH,               // invalid path
} Error;

#define FS_MAGIC 0x12345678

#define BLOCK_SIZE 4096
#define MAXIMUM_BLOCKS 64
#define DISK_SIZE (BLOCK_SIZE * MAXIMUM_BLOCKS)

#define SUPERBLOCK_BLOCK 0
#define INODE_BITMAP_BLOCK 1
#define DATA_BITMAP_BLOCK 2

#define INODE_SIZE 256
#define INODE_TABLE_BLOCK_START 3
#define TOTAL_INODE_TABLE_BLOCKS 5
#define MAXIMUM_INODES ((BLOCK_SIZE / INODE_SIZE) * TOTAL_INODE_TABLE_BLOCKS)
#define INODES_PER_BLOCK (BLOCK_SIZE / INODE_SIZE)
#define INODE_DIRECT_POINTERS_COUNT 12
#define INDIRECT_POINTERS_PER_BLOCK (BLOCK_SIZE / sizeof(uint32_t))
#define INVALID_BLOCK 0

#define USER_DATA_BLOCK_START 8
#define TOTAL_USER_DATA_BLOCKS (MAXIMUM_BLOCKS - USER_DATA_BLOCK_START)

#define ROOT_INODE 0

#define DIRECTORY_ENTRY_SIZE 64
#define DIRECTORY_ENTRIES_PER_BLOCK (BLOCK_SIZE / DIRECTORY_ENTRY_SIZE)
#define DIRECTORY_NAME_SIZE 60
#define MAX_FILENAME_LENGTH (DIRECTORY_NAME_SIZE - 1)

#endif