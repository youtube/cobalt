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

#include <unistd.h>

extern "C" {

int __abi_wrap_ftruncate(int fildes, off_t length);

int ftruncate(int fildes, off_t length) {
  return __abi_wrap_ftruncate(fildes, length);
}

off_t __abi_wrap_lseek(int fildes, off_t offset, int whence);

off_t lseek(int fildes, off_t offset, int whence) {
  return __abi_wrap_lseek(fildes, offset, whence);
}

long __abi_wrap_sysconf(int name);

long sysconf(int name) {
  return __abi_wrap_sysconf(name);
}

long __abi_wrap_pathconf(const char* path, int name);

long pathconf(const char* path, int name) {
  return __abi_wrap_pathconf(path, name);
}

pid_t __abi_wrap_getpid();

pid_t getpid() {
  return __abi_wrap_getpid();
}

uid_t __abi_wrap_geteuid();

uid_t geteuid() {
  return __abi_wrap_geteuid();
}

int __abi_wrap_access(const char* path, int amode);

int access(const char* path, int amode) {
  return __abi_wrap_access(path, amode);
}

int __abi_wrap_fchown(int fd, uid_t owner, gid_t group);

int fchown(int fd, uid_t owner, gid_t group) {
  return __abi_wrap_fchown(fd, owner, group);
}

int __abi_wrap_unlinkat(int fildes, const char* path, int flag);

int unlinkat(int fildes, const char* path, int flag) {
  return __abi_wrap_unlinkat(fildes, path, flag);
}
}  // extern "C"
