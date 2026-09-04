#ifndef INODE_TABLE_H
#define INODE_TABLE_H

#include "disk.h"
#include "inode.h"

int inode_table_read(Disk *disk, uint32_t inode_number, Inode *inode);
int inode_table_write(Disk *disk, uint32_t inode_number, const Inode *inode);

#endif