#include <stdio.h>
#include <sys/stat.h>

#if SB_API_VERSION < 16
#include <fcntl.h>
#include <time.h>
#include "starboard/directory.h"

int mkdir(const char *path, mode_t mode)
{
    if (SbDirectoryCreate(path)){
        return 0;
    }
    return -1;
}

static SB_C_FORCE_INLINE time_t WindowsUsecToTimeT(int64_t time) {
    int64_t posix_time = time - 11644473600000000ULL;
    posix_time = posix_time / 1000000;
    return posix_time;
}

// SbDirectoryCanOpen, SbFileGetPathInfo, SbFileExists, SbFileCanOpen
int stat(const char *path, struct stat *file_info)
{
    SbFileInfo out_info;
    if (!SbFileGetPathInfo(path, &out_info)){
        return -1;
    }

    file_info -> st_mode = (unsigned int) 0;
    if (out_info.is_directory){
        file_info->st_mode = (unsigned int) 16877;
    } else if (out_info.is_symbolic_link){
        file_info->st_mode = (unsigned int) 40960;
    }

    file_info->st_ctime = WindowsUsecToTimeT(out_info.creation_time);
    file_info->st_atime = WindowsUsecToTimeT(out_info.last_accessed);
    file_info->st_mtime = WindowsUsecToTimeT(out_info.last_modified);
    file_info->st_size = out_info.size;

    return 0;
}
#endif  // SB_API_VERSION < 16