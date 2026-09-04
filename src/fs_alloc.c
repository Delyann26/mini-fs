#include "fs_alloc.h"
#include "bitmap.h"
#include "common.h"
#include <stdint.h>

int fs_allocate_inode(Disk *disk, uint32_t *inode_number) {
    if (disk == NULL || inode_number == NULL) {
        return FS_ERR_NULL;
    }
    uint8_t inode_bitmap[BLOCK_SIZE];
    int res = disk_read(disk, inode_bitmap, INODE_BITMAP_BLOCK);
    if (res != FS_OK) {
        return res;
    }

    size_t first_free_bit = bitmap_find_first_free_bit(inode_bitmap, MAXIMUM_INODES);
    if (first_free_bit == SIZE_MAX) {
        return FS_ERR_NO_FREE_INODES;
    }

    res = bitmap_set_bit(inode_bitmap, MAXIMUM_INODES, first_free_bit);
    if (res != FS_OK) {
        return res;
    }

    res = disk_write(disk, inode_bitmap, INODE_BITMAP_BLOCK);
    if (res != FS_OK) {
        return res;
    }
    *inode_number = first_free_bit;
    return FS_OK;
}

int fs_free_inode(Disk *disk, uint32_t inode_number) {
    if (disk == NULL) {
        return FS_ERR_NULL;
    }
    if (inode_number >= MAXIMUM_INODES) {
        return FS_ERR_INDEX_OUT_OF_BOUNDS;
    }
    if (inode_number == ROOT_INODE) {
        return FS_ERR_PROTECTED_INODE;
    }
    uint8_t inode_bitmap[BLOCK_SIZE];
    int res = disk_read(disk, inode_bitmap, INODE_BITMAP_BLOCK);
    if (res != FS_OK) {
        return res;
    }
    if (bitmap_is_free(inode_bitmap, MAXIMUM_INODES, inode_number)) {
        return FS_ERR_INODE_IS_ALREADY_FREE;
    }
    res = bitmap_clear_bit(inode_bitmap, MAXIMUM_INODES, inode_number);
    if (res != FS_OK) {
        return res;
    }

    res = disk_write(disk, inode_bitmap, INODE_BITMAP_BLOCK);
    if (res != FS_OK) {
        return res;
    }

    return FS_OK;
}

int fs_allocate_data_block(Disk *disk, uint32_t *block_number) {
    if (disk == NULL || block_number == NULL) {
        return FS_ERR_NULL;
    }

    uint8_t data_bitmap[BLOCK_SIZE];
    int res = disk_read(disk, data_bitmap, DATA_BITMAP_BLOCK);
    if (res != FS_OK) {
        return res;
    }
    size_t first_free_bit = bitmap_find_first_free_bit(data_bitmap, TOTAL_USER_DATA_BLOCKS);
    if (first_free_bit == SIZE_MAX) {
        return FS_ERR_NO_FREE_DATA_BLOCKS;
    }
    res = bitmap_set_bit(data_bitmap, TOTAL_USER_DATA_BLOCKS, first_free_bit);
    if (res != FS_OK) {
        return res;
    }
    res = disk_write(disk, data_bitmap, DATA_BITMAP_BLOCK);
    if (res != FS_OK) {
        return res;
    }
    *block_number = first_free_bit + USER_DATA_BLOCK_START;
    return FS_OK;
}

int fs_free_data_block(Disk *disk, uint32_t block_number) {
    if (disk == NULL) {
        return FS_ERR_NULL;
    }
    if (block_number < USER_DATA_BLOCK_START) {
        return FS_ERR_PROTECTED_BLOCK;
    }
    if (block_number >= MAXIMUM_BLOCKS) {
        return FS_ERR_INDEX_OUT_OF_BOUNDS;
    }
    uint8_t data_bitmap[BLOCK_SIZE];
    int res = disk_read(disk, data_bitmap, DATA_BITMAP_BLOCK);
    if (res != FS_OK) {
        return res;
    }

    size_t bitmap_index = block_number - USER_DATA_BLOCK_START;
    if (bitmap_is_free(data_bitmap, TOTAL_USER_DATA_BLOCKS, bitmap_index)) {
        return FS_ERR_DATA_BLOCK_IS_ALREADY_FREE;
    }
    res = bitmap_clear_bit(data_bitmap, TOTAL_USER_DATA_BLOCKS, bitmap_index);
    if (res != FS_OK) {
        return res;
    }
    res = disk_write(disk, data_bitmap, DATA_BITMAP_BLOCK);
    if (res != FS_OK) {
        return res;
    }
    return FS_OK;
}