#include <unistd.h>
#include <errno.h>

#include "starboard/system.h"

// This function is to replace its musl implementation
// in //third_party/musl/src/unistd/getcwd.c.
char *getcwd(char *buf, size_t size) {
  if (!size) {
    errno = EINVAL;
    return 0;
  }
  bool got_path =
      SbSystemGetPath(kSbSystemPathTempDirectory, buf, size);
  if (!got_path) {
    errno = ENOENT;
    return 0;
  }
  return buf;
}
