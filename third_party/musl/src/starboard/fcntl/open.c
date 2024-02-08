// #if SB_API_VERSION < 16

// #include <fcntl.h>
// #include "starboard/file.h"
// #include "starboard/shared/posix/file_internal.h"

// int open(const char *path, int oflag, ... ) {
//   int flags;
//   if (oflag & O_CREAT && oflag & O_EXCL) {
//     flags = kSbFileCreateOnly;
//   } else if (oflag & O_CREAT && oflag & O_TRUNC) {
//     flags = kSbFileCreateAlways;
//   } else if (oflag & O_CREAT) {
//     flags = kSbFileOpenAlways;
//   } else if (oflag & O_TRUNC) {
//     flags = kSbFileOpenTruncated;
//   } else if (oflag & O_RDONLY) {
//     flags = kSbFileOpenOnly;
//   }

//   if (oflag & O_RDWR) {
//     flags |= kSbFileRead | kSbFileWrite;
//   } else if (oflag & O_WRONLY) {
//     flags |= kSbFileWrite;
//   }

//   SbFile file = SbFileOpen(path, oflag, NULL, NULL);
//   return file->descriptor;
// }

// #endif   // SB_API_VERSION < 16
