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

#include "starboard/elf_loader/file_impl.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>

#include "starboard/common/file.h"
#include "starboard/common/log.h"
#include "starboard/elf_loader/file_open.h"
#include "starboard/elf_loader/log.h"

namespace {
void LogLastError(const char* msg) {
  const int kErrorMessageBufferSize = 256;
  char msgbuf[kErrorMessageBufferSize];
  SbSystemError error_code = SbSystemGetLastError();
  if (SbSystemGetErrorString(error_code, msgbuf, kErrorMessageBufferSize) > 0) {
    SB_LOG(ERROR) << msg << ": " << msgbuf;
  }
}
}  // namespace

namespace elf_loader {

FileImpl::FileImpl() : file_(-1) {}

FileImpl::~FileImpl() {
  Close();
}

bool FileImpl::Open(const char* name) {
  SB_DLOG(INFO) << "Loading: " << name;
  name_ = name;
  file_ = OpenFileInPlace(name, &offset_, &size_);
  if (file_ >= 0) {
    return true;
  }
  // Don't use the 2-arg open() since _FORTIFY_SOURCE rewrites it to __open_2,
  // which bypasses -Wl,--wrap=open.
  file_ = open(name, O_RDONLY, 0);
  if (file_ < 0) {
    return false;
  }
  struct stat info;
  if (fstat(file_, &info) != 0) {
    close(file_);
    file_ = -1;
    return false;
  }
  offset_ = 0;
  size_ = info.st_size;
  return true;
}

bool FileImpl::ReadFromOffset(int64_t offset, char* buffer, int size) {
  if (file_ < 0 || offset < 0 || size < 0 || offset > size_) {
    return false;
  }
  // Stop at the end of the file. With OpenFileInPlace(), |file_| continues
  // past it into the rest of the containing file.
  size = static_cast<int>(std::min<int64_t>(size, size_ - offset));
  if (size == 0) {
    return true;
  }
  int64_t ret = lseek(file_, offset_ + offset, SEEK_SET);
  SB_DLOG(INFO) << "lseek: ret=" << ret;
  if (ret == -1) {
    LogLastError("lseek: failed");
    return false;
  }

  int count = starboard::ReadAll(file_, buffer, size);
  SB_DLOG(INFO) << "ReadAll: count=" << count;
  if (count < size) {
    LogLastError("ReadAll failed");
    return false;
  }
  return true;
}

void FileImpl::Close() {
  if (file_ >= 0) {
    close(file_);
    file_ = -1;
  }
}

const std::string& FileImpl::GetName() {
  return name_;
}

}  // namespace elf_loader
