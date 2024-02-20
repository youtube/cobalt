// #if SB_API_VERSION < 16

// #include <unistd.h>
// #include "starboard/file.h"
// #include "starboard/shared/posix/file_internal.h"

// int close(int fildes) {
//   SbFile file;
//   file->descriptor = fildes;
//   return SbFileClose(file);
// }

// #endif   // SB_API_VERSION < 16
