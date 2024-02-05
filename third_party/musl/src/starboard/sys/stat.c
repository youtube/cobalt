#include <stdio.h>

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

// SbDirectoryCanOpen, SbFileGetPathInfo, SbFileExists, SbFileCanOpen
int stat(const char *path, struct stat *file_info)
{
    //SbDirectoryCanOpen calls FileGetPathInto and returns a single property
    SbFileInfo* out_info;
    if (!SbFileGetPathInfo(path, &out_info)){
        return -1;
    }

    // out_info->is_symbolic_link = S_ISLNK(file_info.st_mode); ((((file_info.st_mode)) & 0170000) == (0120000))
    // out_info->is_directory = S_ISDIR(file_info.st_mode); ((((file_info.st_mode)) & 0170000) == (0040000))
    if (out_info -> is_directory){
        file_info.st_mode = 16877;
    } else if (out_info -> is_symbolic_link){
        file_info.st_mode = 40960;
    } else {
        file_info.st_mode = 0;
    }
    // out_info->creation_time = TimeTToWindowsUsec(file_info.st_ctime);
    file_info.st_ctime = WindowsUsecToTimeT(out_info->creation_time);
    // out_info->last_accessed = TimeTToWindowsUsec(file_info.st_atime);
    file_info.st_atime = WindowsUsecToTimeT(out_info->last_accessed);
    // out_info->last_modified = TimeTToWindowsUsec(file_info.st_mtime);
    file_info.st_mtime = WindowsUsecToTimeT(out_info->last_modified);
    // out_info->size = file_info.st_size;
    file_info.st_size = out_info->size;

    return 0;
}

inline time_t WindowsUsecToTimeT(int64_t time) {
    return WindowsTimeToPosixTime(time) / 1000000;
}

// Converts a Windows microseconds timestamp to a Posix microseconds timestamp.
inline int64_t WindowsTimeToPosixTime(int64_t posix_time) {
  // Subtract number of microseconds since Jan 1, 1601 (UTC) until Jan 1, 1970 (UTC).
  return posix_time - 11644473600000000ULL;
}
#endif  // SB_API_VERSION < 16