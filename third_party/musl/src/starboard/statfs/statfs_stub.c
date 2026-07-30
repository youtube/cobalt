#include <sys/statfs.h>

#include "starboard/common/log.h"

// This stub is to replace its musl implementation
// in //third_party/musl/src/stat/statvfs.c.
int fstatfs(int fd, struct statfs *buf) {
  *buf = (struct statfs){0};
  SB_NOTIMPLEMENTED();
  return 0;
}
