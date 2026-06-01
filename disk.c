#include "disk.h"
#include <fcntl.h>
#include <stddef.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int disk_init(Disk *disk) {
    if (disk == NULL) {
        return -1;
    }
    disk->fd = -1;
    return 0;
}

int disk_create(Disk *disk, const char *path) {
    if (disk == NULL || path == NULL) {
        return -1;
    }
    if (disk->fd > -1) { // already opened
        return -1;
    }
    disk->fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (disk->fd == -1) {
        return -1;
    }
    uint8_t empty_block[BLOCK_SIZE];
    memset(empty_block, 0, sizeof(empty_block));
    for (size_t i = 0; i < MAXIMUM_BLOCKS; i++) {
        size_t written_so_far = 0;
        while (written_so_far < BLOCK_SIZE) {
            ssize_t written_bytes = write(disk->fd, empty_block + written_so_far, BLOCK_SIZE - written_so_far);
            if (written_bytes <= 0) {
                disk_close(disk);
                return -1;
            }
            written_so_far += written_bytes;
        }
    }
    if (lseek(disk->fd, 0, SEEK_SET) == -1) {
        disk_close(disk);
        return -1;
    }
    return 0;
}
int disk_open(Disk *disk, const char *path) {
    if (disk == NULL || path == NULL) {
        return -1;
    }
    if (disk->fd > -1) { // already opened
        return -1;
    }
    struct stat file_stats;
    if (stat(path, &file_stats) == -1) {
        return -1;
    }
    if (file_stats.st_size != DISK_SIZE) {
        return -1;
    }
    disk->fd = open(path, O_RDWR);
    if (disk->fd == -1) {
        return -1;
    }
    return 0;
}
int disk_close(Disk *disk) {
    if (disk == NULL || disk->fd == -1) {
        return -1;
    }
    if (close(disk->fd) == -1) {
        return -1;
    }
    disk->fd = -1;
    return 0;
}
int disk_read_block(Disk *disk, size_t block_index, void *buffer) {
    if (disk == NULL || disk->fd == -1 || block_index >= MAXIMUM_BLOCKS || buffer == NULL) {
        return -1;
    }
    if (lseek(disk->fd, block_index * BLOCK_SIZE, SEEK_SET) == -1) {
        return -1;
    }
    uint8_t *buffer_bytes = buffer;

    size_t read_so_far = 0;
    while (read_so_far < BLOCK_SIZE) {
        ssize_t read_bytes = read(disk->fd, buffer_bytes + read_so_far, BLOCK_SIZE - read_so_far);
        if (read_bytes <= 0) {
            return -1;
        }
        read_so_far += read_bytes;
    }
    return 0;
}

int disk_write_block(Disk *disk, size_t block_index, const void *buffer) {
    if (disk == NULL || disk->fd == -1 || block_index >= MAXIMUM_BLOCKS || buffer == NULL) {
        return -1;
    }
    if (lseek(disk->fd, block_index * BLOCK_SIZE, SEEK_SET) == -1) {
        return -1;
    }
    const uint8_t *buffer_bytes = buffer;
    size_t written_so_far = 0;
    while (written_so_far < BLOCK_SIZE) {
        ssize_t written_bytes = write(disk->fd, buffer_bytes + written_so_far, BLOCK_SIZE - written_so_far);
        if (written_bytes <= 0) {
            return -1;
        }
        written_so_far += written_bytes;
    }

    return 0;
}