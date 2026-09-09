#include "path.h"
#include "directory.h"
#include "inode.h"
#include "inode_table.h"
#include <string.h>

static int verify_directory(Disk *disk, uint32_t inode_number) {
    Inode inode;
    int res = inode_table_read(disk, inode_number, &inode);
    if (res != FS_OK) {
        return res;
    }
    if (inode.type != INODE_TYPE_DIRECTORY) {
        return FS_ERR_ENTRY_IS_NOT_DIRECTORY;
    }
    return FS_OK;
}

int resolve_path(Disk *disk, const char *path, uint32_t *inode_number) {
    if (disk == NULL || path == NULL || inode_number == NULL) {
        return FS_ERR_NULL;
    }
    size_t path_length = strlen(path);
    if (path_length == 0 || path[0] != '/') {
        return FS_ERR_INVALID_PATH;
    }
    if (path_length == 1) {
        *inode_number = ROOT_INODE;
        return FS_OK;
    }
    uint32_t current_inode_number = ROOT_INODE;
    char current_entry[DIRECTORY_NAME_SIZE];
    size_t j = 0;
    for (size_t i = 1; i <= path_length; i++) {
        if (path[i] == '/' || path[i] == '\0') {
            if (j == 0) {
                return FS_ERR_INVALID_PATH;
            }
            uint32_t entry_inode;
            current_entry[j] = '\0';
            int res = directory_find_entry(disk, current_inode_number, current_entry, &entry_inode);
            if (res == FS_ERR_ENTRY_NOT_FOUND) {
                return FS_ERR_INVALID_PATH;
            }
            if (res != FS_OK) {
                return res;
            }
            current_inode_number = entry_inode;
            j = 0;
        } else {
            if (j >= MAX_FILENAME_LENGTH) {
                return FS_ERR_INVALID_PATH;
            }
            current_entry[j] = path[i];
            j++;
        }
    }
    *inode_number = current_inode_number;
    return FS_OK;
}

int resolve_parent_path(Disk *disk, const char *path, uint32_t *inode_number, char *name) {
    if (disk == NULL || path == NULL || inode_number == NULL || name == NULL) {
        return FS_ERR_NULL;
    }
    size_t path_length = strlen(path);
    if (path_length == 0 || path[0] != '/') {
        return FS_ERR_INVALID_PATH;
    }
    if (path_length == 1) {
        return FS_ERR_INVALID_PATH;
    }
    if (path[path_length - 1] == '/') {
        return FS_ERR_INVALID_PATH;
    }
    size_t last_slash_in_path = path_length - 1;
    while (path[last_slash_in_path] != '/') {
        last_slash_in_path--;
    }
    size_t name_length = path_length - 1 - last_slash_in_path;
    if (name_length == 0 || name_length > MAX_FILENAME_LENGTH) {
        return FS_ERR_INVALID_PATH;
    }
    int res;
    uint32_t parent_inode_number;
    if (last_slash_in_path == 0) {
        parent_inode_number = ROOT_INODE;
    } else {
        char parent_path[last_slash_in_path + 1];
        memcpy(parent_path, path, last_slash_in_path);
        parent_path[last_slash_in_path] = '\0';
        res = resolve_path(disk, parent_path, &parent_inode_number);
        if (res != FS_OK) {
            return res;
        }
    }

    res = verify_directory(disk, parent_inode_number);
    if (res != FS_OK) {
        return res;
    }

    memcpy(name, path + last_slash_in_path + 1, name_length);
    name[name_length] = '\0';
    *inode_number = parent_inode_number;
    return FS_OK;
}
