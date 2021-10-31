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

#include "starboard/common/log.h"
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

namespace starboard {
namespace elf_loader {

FileImpl::FileImpl() : file_(NULL) {}

FileImpl::~FileImpl() {
  Close();
}

bool FileImpl::Open(const char* name) {
  SB_DLOG(INFO) << "Loading: " << name;
  name_ = name;
  file_ = SbFileOpen(name, kSbFileOpenOnly | kSbFileRead, NULL, NULL);
  if (!file_) {
    return false;
  }
  return true;
}

bool FileImpl::ReadFromOffset(int64_t offset, char* buffer, int size) {
  if (!file_) {
    return false;
  }
  int64_t ret = SbFileSeek(file_, kSbFileFromBegin, offset);
  SB_DLOG(INFO) << "SbFileSeek: ret=" << ret;
  if (ret == -1) {
    LogLastError("SbFileSeek: failed");
    return false;
  }

  int count = SbFileReadAll(file_, buffer, size);
  SB_DLOG(INFO) << "SbFileReadAll: count=" << count;
  if (count == -1) {
    LogLastError("SbFileReadAll failed");
    return false;
  }
  return true;
}

void FileImpl::Close() {
  if (file_) {
    SbFileClose(file_);
  }
}

const std::string& FileImpl::GetName() {
  return name_;
}

}  // namespace elf_loader
}  // namespace starboard
