CC = gcc

CFLAGS = -Wall -Wextra -g -Iinclude -Itests
LDLIBS = -lm


main: build/src/main.o build/src/disk.o build/src/superblock.o
	$(CC) $(CFLAGS) build/src/main.o build/src/disk.o build/src/superblock.o -o main


build/src/main.o: src/main.c include/disk.h include/superblock.h include/common.h | build/src
	$(CC) $(CFLAGS) -c src/main.c -o build/src/main.o


build/src/disk.o: src/disk.c include/disk.h include/common.h | build/src
	$(CC) $(CFLAGS) -c src/disk.c -o build/src/disk.o


build/src/superblock.o: src/superblock.c include/superblock.h include/disk.h include/common.h | build/src
	$(CC) $(CFLAGS) -c src/superblock.c -o build/src/superblock.o


build/src/bitmap.o: src/bitmap.c include/bitmap.h include/common.h | build/src
	$(CC) $(CFLAGS) -c src/bitmap.c -o build/src/bitmap.o


build/src/inode.o: src/inode.c include/inode.h include/common.h | build/src
	$(CC) $(CFLAGS) -c src/inode.c -o build/src/inode.o


build/src/inode_table.o: src/inode_table.c \
                         include/inode_table.h \
                         include/disk.h \
                         include/inode.h \
                         include/common.h \
                         | build/src
	$(CC) $(CFLAGS) -c src/inode_table.c -o build/src/inode_table.o


build/src/fs_alloc.o: src/fs_alloc.c \
                      include/fs_alloc.h \
                      include/disk.h \
                      include/bitmap.h \
                      include/common.h \
                      | build/src
	$(CC) $(CFLAGS) -c src/fs_alloc.c -o build/src/fs_alloc.o


build/src/filesystem.o: src/filesystem.c \
                        include/filesystem.h \
                        include/bitmap.h \
                        include/common.h \
                        include/fs_alloc.h \
                        include/inode.h \
                        include/inode_table.h \
                        include/disk.h \
                        include/superblock.h \
                        | build/src
	$(CC) $(CFLAGS) -c src/filesystem.c -o build/src/filesystem.o


build/src/directory.o: src/directory.c \
                       include/directory.h \
                       include/inode_table.h \
                       include/inode.h \
                       include/disk.h \
                       include/common.h \
                       | build/src
	$(CC) $(CFLAGS) -c src/directory.c -o build/src/directory.o



test_all: build/tests/test_main.o \
          build/tests/test_disk.o \
          build/src/disk.o \
          build/tests/test_superblock.o \
          build/src/superblock.o \
          build/tests/test_bitmap.o \
          build/src/bitmap.o \
          build/tests/test_inode.o \
          build/src/inode.o \
          build/tests/test_inode_table.o \
          build/src/inode_table.o \
          build/tests/test_fs_alloc.o \
          build/src/fs_alloc.o \
          build/tests/test_filesystem.o \
          build/src/filesystem.o \
          build/tests/test_directory.o \
          build/src/directory.o
	$(CC) $(CFLAGS) build/tests/test_main.o \
	                    build/tests/test_disk.o \
	                    build/src/disk.o \
	                    build/tests/test_superblock.o \
	                    build/src/superblock.o \
	                    build/tests/test_bitmap.o \
	                    build/src/bitmap.o \
	                    build/tests/test_inode.o \
	                    build/src/inode.o \
	                    build/tests/test_inode_table.o \
	                    build/src/inode_table.o \
	                    build/tests/test_fs_alloc.o \
	                    build/src/fs_alloc.o \
	                    build/tests/test_filesystem.o \
	                    build/src/filesystem.o \
	                    build/tests/test_directory.o \
	                    build/src/directory.o \
	                    -o test_all $(LDLIBS)


build/tests/test_main.o: tests/test_main.c \
                         tests/test_disk.h \
                         tests/test_superblock.h \
                         tests/test_bitmap.h \
                         tests/test_inode.h \
                         tests/test_inode_table.h \
                         tests/test_fs_alloc.h \
                         tests/test_filesystem.h \
                         tests/test_directory.h \
                         | build/tests
	$(CC) $(CFLAGS) -c tests/test_main.c -o build/tests/test_main.o


build/tests/test_disk.o: tests/test_disk.c \
                         tests/test_disk.h \
                         include/disk.h \
                         include/common.h \
                         | build/tests
	$(CC) $(CFLAGS) -c tests/test_disk.c -o build/tests/test_disk.o


build/tests/test_superblock.o: tests/test_superblock.c \
                               tests/test_superblock.h \
                               include/superblock.h \
                               include/disk.h \
                               include/common.h \
                               | build/tests
	$(CC) $(CFLAGS) -c tests/test_superblock.c -o build/tests/test_superblock.o


build/tests/test_bitmap.o: tests/test_bitmap.c \
                           tests/test_bitmap.h \
                           include/bitmap.h \
                           include/common.h \
                           | build/tests
	$(CC) $(CFLAGS) -c tests/test_bitmap.c -o build/tests/test_bitmap.o


build/tests/test_inode.o: tests/test_inode.c \
                          tests/test_inode.h \
                          include/inode.h \
                          include/common.h \
                          | build/tests
	$(CC) $(CFLAGS) -c tests/test_inode.c -o build/tests/test_inode.o


build/tests/test_inode_table.o: tests/test_inode_table.c \
                                tests/test_inode_table.h \
                                include/inode_table.h \
                                include/disk.h \
                                include/inode.h \
                                include/common.h \
                                | build/tests
	$(CC) $(CFLAGS) -c tests/test_inode_table.c -o build/tests/test_inode_table.o


build/tests/test_fs_alloc.o: tests/test_fs_alloc.c \
                             tests/test_fs_alloc.h \
                             include/fs_alloc.h \
                             include/disk.h \
                             include/bitmap.h \
                             include/common.h \
                             | build/tests
	$(CC) $(CFLAGS) -c tests/test_fs_alloc.c -o build/tests/test_fs_alloc.o


build/tests/test_filesystem.o: tests/test_filesystem.c \
                               tests/test_filesystem.h \
                               include/bitmap.h \
                               include/common.h \
                               include/disk.h \
                               include/filesystem.h \
                               include/inode.h \
                               include/inode_table.h \
                               include/superblock.h \
                               | build/tests
	$(CC) $(CFLAGS) -c tests/test_filesystem.c -o build/tests/test_filesystem.o


build/tests/test_directory.o: tests/test_directory.c \
                              tests/test_directory.h \
                              include/directory.h \
                              include/filesystem.h \
                              include/fs_alloc.h \
                              include/inode.h \
                              include/inode_table.h \
                              include/disk.h \
                              include/common.h \
                              | build/tests
	$(CC) $(CFLAGS) -c tests/test_directory.c -o build/tests/test_directory.o


test: test_all
	./test_all


build/src:
	mkdir -p build/src


build/tests:
	mkdir -p build/tests


clean:
	rm -rf build main test_all


.PHONY: test clean