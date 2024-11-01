#include "fs.h"
#include <stdio.h>
#include <assert.h>
int main() {
  printf("testing");
  char* test_name = "test";
  assert(make_fs(test_name)!=-1);
  assert(mount_fs(test_name)!=-1);
}
