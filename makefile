override CFLAGS := -Wall -Werror -std=gnu99 -pedantic -O0 -g -pthread $(CFLAGS)
override LDLIBS := -pthread $(LDLIBS)
CC = gcc

all: check

fs.o : fs.c fs.h disk.h
disk.o: disk.c disk.h

test_make.o: test_make.c
test_create.o :test_create.c
test_listfiles.o: test_listfiles.c
test_open_close.o: test_open_close.c
test_fs_write.o: test_fs_write.c
test_get_filesize.o : test_get_filesize.c
test_fs_read.o: test_fs_read.c
test_big_writes.o: test_big_writes.c

test_make: test_make.o fs.o disk.o
test_create: test_create.o fs.o disk.o
test_listfiles: test_listfiles.o fs.o disk.o
test_open_close: test_open_close.o fs.o disk.o
test_fs_write: test_fs_write.o fs.o disk.o
test_get_filesize: test_get_filesize.o fs.o disk.o
test_fs_read: test_fs_read.o fs.o disk.o
test_big_writes: test_big_writes.o fs.o disk.o

test_files=./test_make ./test_create ./test_listfiles ./test_open_close ./test_fs_write ./test_get_filesize ./test_fs_read ./test_big_writes

.PHONY: clean check checkprogs all

checkprogs: $(test_files)

check: checkprogs
	/bin/bash run_tests.sh $(test_files)

clean:
	rm -f disk.o fs fs.o
