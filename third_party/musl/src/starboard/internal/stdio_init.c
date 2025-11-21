// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "stdio_impl.h"
#include "pthread_impl.h"

static pthread_once_t __std_files_once = PTHREAD_ONCE_INIT;

void __init_std_files() {
  __init_file_lock(&__stdin_FILE);
  __init_file_lock(&__stdout_FILE);
  __init_file_lock(&__stderr_FILE);
}

size_t __stdio_read_init(FILE *f, unsigned char *buf, size_t len) {
  pthread_once(&__std_files_once, __init_std_files);
  f->read = __stdio_read;
  return __stdio_read(f, buf, len);
}

size_t __stdio_write_init(FILE *f, const unsigned char *buf, size_t len) {
  pthread_once(&__std_files_once, __init_std_files);
  f->write = __stdout_write;
  return __stdout_write(f, buf, len);
}

off_t __stdio_seek_init(FILE *f, off_t off, int whence) {
  pthread_once(&__std_files_once, __init_std_files);
  f->seek = __stdio_seek;
  return __stdio_seek(f, off, whence);
}
