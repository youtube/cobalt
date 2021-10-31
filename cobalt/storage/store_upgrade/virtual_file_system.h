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

#ifndef COBALT_STORAGE_STORE_UPGRADE_VIRTUAL_FILE_SYSTEM_H_
#define COBALT_STORAGE_STORE_UPGRADE_VIRTUAL_FILE_SYSTEM_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"

namespace cobalt {
namespace storage {
namespace store_upgrade {

class VirtualFile;

// Implements a simple virtual filesystem, primarily intended to
// be used for providing an in-memory filesystem that SQLite can write to,
// and allowing that filesystem to be saved out into a single memory buffer.
// Contains a mapping from paths to their corresponding memory blocks.
// VirtualFileSystem must be serialized, deserialized, and destroyed on the
// same thread it was created on.
// Open/Delete are thread-safe so that SQL can run on any thread.
class VirtualFileSystem {
 public:
  struct SerializedHeader {
    int32 version;
    int32 file_size;
    int32 file_count;
  };
  // Returns the version of the VirtualFileSystem header in buffer.
  // -1 if buffer is invalid.
  static int GetHeaderVersion(const SerializedHeader& header);
  static int GetCurrentVersion();

  VirtualFileSystem();
  ~VirtualFileSystem();

  // Serializes the file system out to a single contiguous buffer.
  // A dry run only calculates the number of bytes needed.
  // Returns the number of bytes written.
  int Serialize(uint8* buffer, bool dry_run);

  // Deserializes a file system from a memory buffer.
  // Returns false on failure.
  bool Deserialize(const uint8* buffer, int buffer_size);

  // Simple file open. Will create a file if it does not exist, and files are
  // always readable and writable.
  VirtualFile* Open(const std::string& filename);

  std::vector<std::string> ListFiles();

  void Delete(const std::string& filename);

 private:
  void ClearFileTable();
  void SQLRegister();
  void SQLUnregister();

  typedef std::map<std::string, VirtualFile*> FileTable;
  FileTable table_;
  // Certain operations (serialize, deserialize) must run on the same thread.
  THREAD_CHECKER(thread_checker_);
  // Lock to protect the file table. We want to support SQL queries on any
  // thread.
  base::Lock file_table_lock_;
};

}  // namespace store_upgrade
}  // namespace storage
}  // namespace cobalt

#endif  // COBALT_STORAGE_STORE_UPGRADE_VIRTUAL_FILE_SYSTEM_H_
