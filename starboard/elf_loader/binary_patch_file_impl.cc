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

#include "starboard/elf_loader/binary_patch_file_impl.h"

#include <vector>

#include "courgette/streams.h"
#include "courgette/third_party/bsdiff/bsdiff.h"
#include "starboard/common/log.h"
#include "starboard/common/time.h"
#include "starboard/elf_loader/evergreen_config.h"
#include "starboard/file.h"

namespace starboard {
namespace elf_loader {

BinaryPatchFileImpl::BinaryPatchFileImpl() {}

BinaryPatchFileImpl::~BinaryPatchFileImpl() {}

bool BinaryPatchFileImpl::Open(const char* name) {
  // TODO(hwarriner): look into whether we can apply the patch in a streaming
  // mode so that we can avoid loading the entire old binary into memory.

  std::vector<char> patch_contents;
  if (!LoadContents(name, patch_contents)) {
    SB_LOG(INFO) << "Failed to load " << name;
    return false;
  }
  SB_LOG(INFO) << "patch size is: " << patch_contents.size();
  courgette::SourceStream patch_source_stream;
  patch_source_stream.Init(patch_contents.data(), patch_contents.size());

  const starboard::elf_loader::EvergreenConfig* evergreen_config =
      starboard::elf_loader::EvergreenConfig::GetInstance();
  std::string system_image_path = evergreen_config->system_image_path_;
  std::string system_image_lib_path = system_image_path +=
      "/app/cobalt/lib/libcobalt.so";
  std::vector<char> old_contents;
  if (!LoadContents(system_image_lib_path.c_str(), old_contents)) {
    SB_LOG(INFO) << "Failed to load " << system_image_lib_path;
    return false;
  }
  SB_LOG(INFO) << "old binary size is: " << old_contents.size();
  courgette::SourceStream old_source_stream;
  old_source_stream.Init(old_contents.data(), old_contents.size());

  courgette::SinkStream new_sink_stream;
  int64_t start_time_us = CurrentMonotonicTime();
  bsdiff::ApplyBinaryPatch(&old_source_stream, &patch_source_stream,
                           &new_sink_stream);
  int64_t end_time_us = CurrentMonotonicTime();
  SB_LOG(INFO) << "Patch applied in: " << (end_time_us - start_time_us) / 1000
               << " milliseconds";

  SB_LOG(INFO) << "new binary size is: " << new_sink_stream.Length();
  new_binary_.assign(new_sink_stream.Buffer(),
                     new_sink_stream.Buffer() + new_sink_stream.Length());

  return true;
}

bool BinaryPatchFileImpl::LoadContents(const char* file_path,
                                       std::vector<char>& buffer) {
  SbFile file =
      SbFileOpen(file_path, kSbFileOpenOnly | kSbFileRead, NULL, NULL);
  if (!file) {
    return false;
  }

  SbFileInfo file_info;
  if (!SbFileGetInfo(file, &file_info)) {
    return false;
  }

  buffer.resize(file_info.size);
  if (file_info.size != SbFileReadAll(file, buffer.data(), file_info.size)) {
    return false;
  }
  return true;
}

bool BinaryPatchFileImpl::ReadFromOffset(int64_t offset,
                                         char* buffer,
                                         int size) {
  if ((offset < 0) || (offset + size >= new_binary_.size())) {
    return false;
  }
  memcpy(buffer, new_binary_.data() + offset, size);
  return true;
}

}  // namespace elf_loader
}  // namespace starboard
