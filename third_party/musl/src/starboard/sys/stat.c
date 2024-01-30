#include <stdio.h>

#if SB_API_VERSION < 16
#include <fcntl.h>
#include "starboard/directory.h"

int mkdir(const char *path, mode_t mode)
{
    if (SbDirectoryCreate(path))
        return 0;
    return -1;
}

#endif  // SB_API_VERSION < 16