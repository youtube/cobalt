#include <sys/stat.h>
#include <fcntl.h>

int stat(const char *restrict path, struct stat *restrict buf)
{
	return fstatat(AT_FDCWD, path, buf, 0);
}

int stat64(const char* path, struct stat* info) {
  return stat(path, info);
}
