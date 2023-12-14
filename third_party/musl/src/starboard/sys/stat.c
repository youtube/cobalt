#include <stdio.h>

#if SB_API_VERSION < 16
#include <fcntl.h>
#include "starboard/directory.h"

int stat(const char *restrict path, struct stat *restrict buf)
{
	return SbDirectoryCanOpen(path);
}

int mkdir(const char *path, mode_t mode)
{
    return SbDirectoryCreate(path);
}

#endif  // SB_API_VERSION < 16