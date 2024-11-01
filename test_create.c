#include "fs.h"
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
int main() {
  printf("testing");
  char* test_name = "test";
  char* test_file = "testfile";
  assert(make_fs(test_name)!=-1);
  assert(mount_fs(test_name)!=-1);
  assert(fs_create(test_file)!=-1);
}
