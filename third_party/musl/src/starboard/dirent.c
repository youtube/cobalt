#if SB_API_VERSION < 16

#include <stdio.h>

#include <dirent.h>
#include <fcntl.h>
#include <time.h>
#include "starboard/common/log.h"
#include "starboard/configuration_constants.h"
#include "starboard/directory.h"
#include "starboard/file.h"

// copied from starboard/common/string.h
int strlcpy_dirent(char* dst, const char* src, int dst_size) {
  for (int i = 0; i < dst_size; ++i) {
    if ((dst[i] = src[i]) == 0)  // We hit and copied the terminating NULL.
      return i;
  }

  // We were left off at dst_size.  We over copied 1 byte.  Null terminate.
  if (dst_size != 0)
    dst[dst_size - 1] = 0;

  // Count the rest of the |src|, and return its length in characters.
  while (src[dst_size])
    ++dst_size;
  return dst_size;
}

struct __dirstream {
  SbDirectory dir;
};

typedef struct __dirstream DIR;

DIR* opendir(const char *path){
    SbDirectory result = SbDirectoryOpen(path, NULL);
    if (result == kSbDirectoryInvalid){
        return NULL;
    }
    DIR* ret = (DIR*)(malloc(sizeof(DIR)));
    ret->dir = result;
    return ret;
}

int closedir(DIR* dir){
    if (!dir || !dir->dir){
      return -1;
    }
    return SbDirectoryClose(dir->dir) ? 0 : -1;
}

int readdir_r(DIR* dir, struct dirent* dirent_buf, struct dirent** dirent){
    if (!dir || !dir->dir || !dirent_buf || !dirent){
      dirent = NULL;
      return -1;
    }

    *dirent = dirent_buf;
    if (SbDirectoryGetNext(dir->dir, (*dirent)->d_name, kSbFileMaxName)){   
        return 0;
    }
    dirent = NULL;
    return -1;
}

#endif  // SB_API_VERSION < 16
