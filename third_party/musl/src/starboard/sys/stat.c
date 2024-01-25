#include <stdio.h>

#if SB_API_VERSION < 16
#include <fcntl.h>
#include "starboard/directory.h"

int mkdir(const char *path, mode_t mode)
{
    return SbDirectoryCreate(path);
}

#endif  // SB_API_VERSION < 16