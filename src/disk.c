#include "disk.h"
#include "common.h"
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int disk_init(Disk *disk) {
    if (disk == NULL) {
        return FS_ERR_NULL;
    }
    disk->fd = -1;
    return FS_OK;
}

int disk_create(Disk *disk, const char *path) {
    if (disk == NULL || path == NULL) {
        return FS_ERR_NULL;
    }
    if (disk->fd > -1) {
        return FS_ERR_ALREADY_OPEN;
    }
    disk->fd = open(path, O_RDWR | O_CREAT | O_TRUNC | O_BINARY, 0644);
    if (disk->fd == -1) {
        return FS_ERR_CREATE;
    }
    uint8_t empty_block[BLOCK_SIZE];
    memset(empty_block, 0, sizeof(empty_block));
    for (size_t i = 0; i < MAXIMUM_BLOCKS; i++) {
        size_t total_bytes_written = 0;
        while (total_bytes_written < BLOCK_SIZE) {
            ssize_t bytes_written = write(disk->fd, empty_block + total_bytes_written, BLOCK_SIZE - total_bytes_written);
            if (bytes_written <= 0) {
                close(disk->fd);
                disk->fd = -1;
                return FS_ERR_WRITE;
            }
            total_bytes_written += bytes_written;
        }
    }

    return FS_OK;
}

int disk_open(Disk *disk, const char *path) {
    if (disk == NULL || path == NULL) {
        return FS_ERR_NULL;
    }
    if (disk->fd > -1) {
        return FS_ERR_ALREADY_OPEN;
    }
    disk->fd = open(path, O_RDWR | O_BINARY);
    if (disk->fd == -1) {
        return FS_ERR_OPEN;
    }
    struct stat file_stat;
    if (fstat(disk->fd, &file_stat) == -1) {
        close(disk->fd);
        disk->fd = -1;
        return FS_ERR_STAT;
    }
    if (file_stat.st_size != DISK_SIZE) {
        close(disk->fd);
        disk->fd = -1;
        return FS_ERR_INVALID_DISK;
    }
    return FS_OK;
}

int disk_close(Disk *disk) {
    if (disk == NULL) {
        return FS_ERR_NULL;
    }
    if (disk->fd == -1) {
        return FS_ERR_NOT_OPEN;
    }
    if (close(disk->fd) == -1) {
        disk->fd = -1;
        return FS_ERR_CLOSE;
    }
    disk->fd = -1;
    return FS_OK;
}

int disk_read(Disk *disk, void *buffer, size_t block_index) {
    if (disk == NULL || buffer == NULL) {
        return FS_ERR_NULL;
    }
    if (disk->fd == -1) {
        return FS_ERR_NOT_OPEN;
    }
    if (block_index >= MAXIMUM_BLOCKS) {
        return FS_ERR_INVALID_BLOCK;
    }
    if (lseek(disk->fd, block_index * BLOCK_SIZE, SEEK_SET) == -1) {
        return FS_ERR_SEEK;
    }
    uint8_t *bytes = buffer;
    size_t total_bytes_read = 0;
    while (total_bytes_read < BLOCK_SIZE) {
        ssize_t bytes_read = read(disk->fd, bytes + total_bytes_read, BLOCK_SIZE - total_bytes_read);
        if (bytes_read <= 0) {
            return FS_ERR_READ;
        }
        total_bytes_read += bytes_read;
    }
    return FS_OK;
}

int disk_write(Disk *disk, const void *buffer, size_t block_index) {
    if (disk == NULL || buffer == NULL) {
        return FS_ERR_NULL;
    }
    if (disk->fd == -1) {
        return FS_ERR_NOT_OPEN;
    }
    if (block_index >= MAXIMUM_BLOCKS) {
        return FS_ERR_INVALID_BLOCK;
    }
    if (lseek(disk->fd, block_index * BLOCK_SIZE, SEEK_SET) == -1) {
        return FS_ERR_SEEK;
    }
    const uint8_t *bytes = buffer;
    size_t total_bytes_written = 0;
    while (total_bytes_written < BLOCK_SIZE) {
        ssize_t bytes_written = write(disk->fd, bytes + total_bytes_written, BLOCK_SIZE - total_bytes_written);
        if (bytes_written <= 0) {
            return FS_ERR_WRITE;
        }
        total_bytes_written += bytes_written;
    }
    return FS_OK;
}