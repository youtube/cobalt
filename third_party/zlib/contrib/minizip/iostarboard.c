// Copyright 2019 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Starboard IO base function header for compress/uncompress .zip

#include <fcntl.h>
#include <unistd.h>

#include "zlib.h"
#include "iostarboard.h"
#include "starboard/common/file_wrapper.h"

static voidpf  ZCALLBACK starboard_open_file_func  OF((voidpf opaque, const char* filename, int mode));
static voidpf  ZCALLBACK starboard_open64_file_func  OF((voidpf opaque, const void* filename, int mode));
static uLong   ZCALLBACK starboard_read_file_func  OF((voidpf opaque, voidpf stream, void* buf, uLong size));
static uLong   ZCALLBACK starboard_write_file_func OF((voidpf opaque, voidpf stream, const void* buf, uLong size));
static ZPOS64_T ZCALLBACK starboard_tell64_file_func  OF((voidpf opaque, voidpf stream));
static long    ZCALLBACK starboard_seek64_file_func  OF((voidpf opaque, voidpf stream, ZPOS64_T offset, int origin));
static int     ZCALLBACK starboard_close_file_func OF((voidpf opaque, voidpf stream));
static int     ZCALLBACK starboard_error_file_func OF((voidpf opaque, voidpf stream));

static voidpf  ZCALLBACK starboard_open_file_func (voidpf opaque, const char* filename, int mode) {
    int flags = 0;
    if ((mode & ZLIB_FILEFUNC_MODE_READWRITEFILTER)==ZLIB_FILEFUNC_MODE_READ) {
      flags = O_RDONLY;
    } else if (mode & ZLIB_FILEFUNC_MODE_EXISTING){
      flags = O_RDWR;
    } else if (mode & ZLIB_FILEFUNC_MODE_CREATE) {
      flags = O_RDWR | O_CREAT | O_TRUNC;
    }

    FilePtr file = NULL;
    if (filename != NULL) {
      file = file_open((const char*)filename, flags);
    }
    return file;
}

static voidpf  ZCALLBACK starboard_open64_file_func (voidpf opaque, const void* filename, int mode) {
    int flags = 0;
    if ((mode & ZLIB_FILEFUNC_MODE_READWRITEFILTER)==ZLIB_FILEFUNC_MODE_READ) {
      flags = O_RDONLY;
    } else if (mode & ZLIB_FILEFUNC_MODE_EXISTING){
      flags = O_RDWR;
    } else if (mode & ZLIB_FILEFUNC_MODE_CREATE) {
      flags = O_RDWR | O_CREAT | O_TRUNC;
    }

    FilePtr file = NULL;
    if (filename != NULL) {
      file = file_open((const char*)filename, flags);
    }
    return file;
}

static uLong ZCALLBACK starboard_read_file_func (voidpf opaque, voidpf stream, void* buf, uLong size) {
    uLong ret = 0;
    FilePtr file = NULL;
    if (stream != NULL) {
      file = (FilePtr)stream;
    }
    if (file != NULL) {
      int bytes_read = read(file->fd, (char*)buf, (int)size);
      ret = (uLong)(bytes_read == -1 ? 0 : bytes_read);
    }
    return ret;
}

static uLong ZCALLBACK starboard_write_file_func (voidpf opaque, voidpf stream, const void* buf, uLong size) {
    uLong ret = 0;
    FilePtr file = NULL;
    if (stream != NULL) {
      file = (FilePtr)stream;
    }
    if (file != NULL) {
      int bytes_written = write(file->fd, (const char*)buf, (int)size);
      ret = (uLong)(bytes_written == -1 ? 0 : bytes_written);
    }
    return ret;
}

static long ZCALLBACK starboard_tell_file_func (voidpf opaque, voidpf stream) {
    long ret = -1;
    FilePtr file = NULL;
    if (stream != NULL) {
      file = (FilePtr)stream;
    }
    if (file != NULL) {
      ret = lseek(file->fd, 0, SEEK_CUR);
    }
    return ret;
}

static ZPOS64_T ZCALLBACK starboard_tell64_file_func (voidpf opaque, voidpf stream) {
    ZPOS64_T ret = -1;
    FilePtr file = NULL;
    if (stream != NULL) {
      file = (FilePtr)stream;
    }
    if (file != NULL) {
      ret = (ZPOS64_T)lseek(file->fd, 0, SEEK_CUR);
    }
    return ret;
}

static long ZCALLBACK starboard_seek_file_func (voidpf opaque, voidpf stream, uLong offset, int origin) {
    long ret = -1;
    FilePtr file = NULL;
    int file_whence = 0;
    if (stream != NULL) {
      file = (FilePtr)stream;
    }
    switch (origin) {
      case ZLIB_FILEFUNC_SEEK_CUR :
          file_whence = SEEK_CUR;
          break;
      case ZLIB_FILEFUNC_SEEK_END :
          file_whence = SEEK_END;
          break;
      case ZLIB_FILEFUNC_SEEK_SET :
          file_whence = SEEK_SET;
          break;
      default:
          return -1;
    }

    if (file != NULL) {
      if (lseek(file->fd, (int64_t)offset, file_whence) != -1) {
        ret = 0;
      }
    }
    return ret;
}

static long ZCALLBACK starboard_seek64_file_func (voidpf opaque, voidpf stream, ZPOS64_T offset, int origin) {
    long ret = -1;
    FilePtr file = NULL;
    int file_whence = 0;
    if (stream != NULL) {
      file = (FilePtr)stream;
    }
    switch (origin) {
      case ZLIB_FILEFUNC_SEEK_CUR :
          file_whence = SEEK_CUR;
          break;
      case ZLIB_FILEFUNC_SEEK_END :
          file_whence = SEEK_END;
          break;
      case ZLIB_FILEFUNC_SEEK_SET :
          file_whence = SEEK_SET;
          break;
      default:
          return -1;
    }

    if (file != NULL) {
      if (lseek(file->fd, (int64_t)offset, file_whence) != -1) {
        ret = 0;
      }
    }
    return ret;
}

static int ZCALLBACK starboard_close_file_func (voidpf opaque, voidpf stream) {
    int ret = -1;
    FilePtr file = NULL;
    if (stream != NULL) {
      file = (FilePtr)stream;
    }
    if (file != NULL && !file_close(file)) {
      ret = 0;
    }
    return ret;
}

static int ZCALLBACK starboard_error_file_func (voidpf opaque, voidpf stream) {
    // This function is NOOP because Starboard doesn't have a counterpart of ferror. Starboard sets
    // the file error code in SbFileOpen, but zlib uses this function to get the file error code
    // only after reading a file (SbFileRead), which doesn't set the error code anyways.
    int ret = -1;
    FilePtr file = NULL;
    if (stream != NULL) {
      file = (FilePtr)stream;
    }
    if (file != NULL) {
      ret = 0;
    }
    return ret;
}

void fill_starboard_filefunc (zlib_filefunc_def* pzlib_filefunc_def) {
    pzlib_filefunc_def->zopen_file = starboard_open_file_func;
    pzlib_filefunc_def->zread_file = starboard_read_file_func;
    pzlib_filefunc_def->zwrite_file = starboard_write_file_func;
    pzlib_filefunc_def->ztell_file = starboard_tell_file_func;
    pzlib_filefunc_def->zseek_file = starboard_seek_file_func;
    pzlib_filefunc_def->zclose_file = starboard_close_file_func;
    pzlib_filefunc_def->zerror_file = starboard_error_file_func;
    pzlib_filefunc_def->opaque = NULL;
}

void fill_starboard_filefunc64(zlib_filefunc64_def* pzlib_filefunc_def) {
    pzlib_filefunc_def->zopen64_file = starboard_open64_file_func;
    pzlib_filefunc_def->zread_file = starboard_read_file_func;
    pzlib_filefunc_def->zwrite_file = starboard_write_file_func;
    pzlib_filefunc_def->ztell64_file = starboard_tell64_file_func;
    pzlib_filefunc_def->zseek64_file = starboard_seek64_file_func;
    pzlib_filefunc_def->zclose_file = starboard_close_file_func;
    pzlib_filefunc_def->zerror_file = starboard_error_file_func;
    pzlib_filefunc_def->opaque = NULL;
}
