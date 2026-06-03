// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_COMMON_FILE_WRAPPER_H_
#define STARBOARD_COMMON_FILE_WRAPPER_H_

typedef struct FileStruct {
  int fd;
} FileStruct;

typedef FileStruct* FilePtr;

#ifdef __cplusplus
extern "C" {
#endif

// Converts an ISO |fopen()| mode string into flags that can be equivalently
// passed into POSIX open().
//
// |mode|: The mode string to be converted into flags.
int FileModeStringToFlags(const char* mode);

// Wrapper for close() that takes in a FilePtr instead of a file descriptor.
int file_close(FilePtr file);

// Wrapper for open() that returns a FilePtr instead of a file descriptor.
FilePtr file_open(const char* path, int mode);

#ifdef __cplusplus
}
#endif

#endif  // STARBOARD_COMMON_FILE_WRAPPER_H_
