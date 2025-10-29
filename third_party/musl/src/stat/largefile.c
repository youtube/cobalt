#if defined(_LARGEFILE64_SOURCE)
#include <sys/stat.h>

int stat64(const char* path, struct stat* info) {
  return stat(path, info);
}

int fstat64(int fildes, struct stat* info) {
  return fstat(fildes, info);
}
#endif
