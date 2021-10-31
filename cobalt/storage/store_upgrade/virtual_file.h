// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_STORAGE_STORE_UPGRADE_VIRTUAL_FILE_H_
#define COBALT_STORAGE_STORE_UPGRADE_VIRTUAL_FILE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"

namespace cobalt {
namespace storage {
namespace store_upgrade {

class VirtualFileSystem;

// VirtualFile implements an in-memory file system. These
// are managed by VirtualFileSystem for use by SQLite3.
// The format of a serialized VirtualFile is as follows:
// 8-byte name length
// <name length> File name string
// 8-byte file size
// <file size> raw bytes.
class VirtualFile {
 public:
  enum { kMaxVfsPathname = 512 };

  int Read(void* dest, int bytes, int offset) const;
  int Write(const void* source, int bytes, int offset);
  int Truncate(int size);
  int Size() const { return static_cast<int>(buffer_.size()); }

 private:
  explicit VirtualFile(const std::string& name);
  ~VirtualFile();

  // Returns the number of bytes written
  int Serialize(uint8* dest, const bool dry_run);
  // Deserializes a file, returning the size of the file or
  // < 0 on error.
  // |buffer_remaining| is the maximum size of |source|
  int Deserialize(const uint8* source, size_t buffer_remaining);

  std::vector<uint8> buffer_;

  std::string name_;

  friend class VirtualFileSystem;
  DISALLOW_COPY_AND_ASSIGN(VirtualFile);
};

}  // namespace store_upgrade
}  // namespace storage
}  // namespace cobalt

#endif  // COBALT_STORAGE_STORE_UPGRADE_VIRTUAL_FILE_H_
