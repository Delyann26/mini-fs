#include "fs_alloc.h"
#include "bitmap.h"

uint32_t fs_allocate_inode(Disk *disk) {
    if (disk == NULL) {
        return UINT32_MAX;
    }

    uint8_t inode_bitmap[BLOCK_SIZE];
    if (disk_read_block(disk, INODE_BITMAP_BLOCK, inode_bitmap) == -1) {
        // reading inode bitmap block failed
        return UINT32_MAX;
    }
    uint32_t free_index = bitmap_find_free(inode_bitmap, MAXIMUM_INODES);
    if (free_index == UINT32_MAX) {
        // no free inode bit
        return UINT32_MAX;
    }
    if (bitmap_set(inode_bitmap, free_index) == -1) {
        // inode bitmap is null
        return UINT32_MAX;
    }
    if (disk_write_block(disk, INODE_BITMAP_BLOCK, inode_bitmap) == -1) {
        // writing inode bitmap to disk failed
        return UINT32_MAX;
    }
    return free_index;
}

int fs_free_inode(Disk *disk, uint32_t inode_index) {
    if (disk == NULL || inode_index >= MAXIMUM_INODES) {
        return -1;
    }
    uint8_t inode_bitmap[BLOCK_SIZE];
    if (disk_read_block(disk, INODE_BITMAP_BLOCK, inode_bitmap) == -1) {
        // reading inode bitmap block failed
        return -1;
    }
    if (bitmap_clear(inode_bitmap, inode_index) == -1) {
        // inode bitmap is null
        return -1;
    }
    if (disk_write_block(disk, INODE_BITMAP_BLOCK, inode_bitmap) == -1) {
        // writing inode bitmap to disk failed
        return -1;
    }
    return 0;
}

uint32_t fs_allocate_data_block(Disk *disk) {
    if (disk == NULL) {
        return UINT32_MAX;
    }
    uint8_t data_bitmap[BLOCK_SIZE];
    if (disk_read_block(disk, DATA_BITMAP_BLOCK, data_bitmap) == -1) {
        // reading data bitmap block failed
        return UINT32_MAX;
    }
    uint32_t free_index = bitmap_find_free(data_bitmap, MAXIMUM_DATA_BLOCKS);
    if (free_index == UINT32_MAX) {
        // no free data bit
        return UINT32_MAX;
    }
    if (bitmap_set(data_bitmap, free_index) == -1) {
        // data bitmap is null
        return UINT32_MAX;
    }
    if (disk_write_block(disk, DATA_BITMAP_BLOCK, data_bitmap) == -1) {
        // writing inode bitmap to disk failed
        return UINT32_MAX;
    }
    return free_index + DATA_BLOCK_START;
}

int fs_free_data_block(Disk *disk, uint32_t block_index) {
    if (disk == NULL || block_index < DATA_BLOCK_START || block_index >= MAXIMUM_BLOCKS) {
        return -1;
    }
    uint8_t data_bitmap[BLOCK_SIZE];
    if (disk_read_block(disk, DATA_BITMAP_BLOCK, data_bitmap) == -1) {
        // reading data bitmap block failed
        return -1;
    }
    if (bitmap_clear(data_bitmap, block_index - DATA_BLOCK_START) == -1) {
        // data bitmap is null
        return -1;
    }
    if (disk_write_block(disk, DATA_BITMAP_BLOCK, data_bitmap) == -1) {
        // writing data bitmap to disk failed
        return -1;
    }
    return 0;
}