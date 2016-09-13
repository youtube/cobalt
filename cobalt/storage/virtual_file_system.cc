/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include "cobalt/storage/virtual_file_system.h"

#include "base/logging.h"
#include "base/synchronization/lock.h"
#include "cobalt/storage/virtual_file.h"

#include "starboard/client_porting/poem/string_poem.h"

namespace cobalt {
namespace storage {

namespace {
// Update this any time the serialization format changes.
const char kVersion[] = "SAV0";
}  // namespace

// static
int VirtualFileSystem::GetHeaderVersion(const SerializedHeader& header) {
  // Copy the version int to a char buffer to avoid endian issues.
  char version[4];
  memcpy(version, &header.version, sizeof(version));

  if (memcmp(version, kVersion, 3) != 0) {
    return -1;
  } else if (header.file_size < static_cast<int>(sizeof(SerializedHeader))) {
    return -1;
  } else {
    return version[3] - '0';
  }
}

// static
int VirtualFileSystem::GetCurrentVersion() {
  COMPILE_ASSERT(sizeof(kVersion) - 1 == 4, Unexpected_version_size);
  int version;
  memcpy(&version, kVersion, sizeof(version));
  return version;
}

VirtualFileSystem::VirtualFileSystem() {}

VirtualFileSystem::~VirtualFileSystem() {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock lock(file_table_lock_);
  ClearFileTable();
}

VirtualFile* VirtualFileSystem::Open(const std::string& filename) {
  base::AutoLock lock(file_table_lock_);
  VirtualFile* result = NULL;
  FileTable::iterator it = table_.find(filename);
  if (it != table_.end()) {
    result = it->second;
  } else {
    result = new VirtualFile(filename);
    table_.insert(it, FileTable::value_type(filename, result));
  }
  return result;
}

std::vector<std::string> VirtualFileSystem::ListFiles() {
  base::AutoLock lock(file_table_lock_);
  std::vector<std::string> files;
  for (FileTable::iterator it = table_.begin(); it != table_.end(); ++it) {
    files.push_back(it->first);
  }
  return files;
}

void VirtualFileSystem::Delete(const std::string& filename) {
  base::AutoLock lock(file_table_lock_);
  FileTable::iterator it = table_.find(filename);
  if (it != table_.end()) {
    delete it->second;
    table_.erase(it);
  }
}

int VirtualFileSystem::Serialize(uint8* buffer, bool dry_run) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock lock(file_table_lock_);
  uint8* original = buffer;

  // We don't know the total size of the file yet, so defer writing the header.
  buffer += sizeof(SerializedHeader);
  int valid_file_count = 0;

  // Serialize each file
  for (FileTable::iterator it = table_.begin(); it != table_.end(); ++it) {
    if (it->second->Size() == 0) {
      continue;
    }
    int file_bytes = it->second->Serialize(buffer, dry_run);
    buffer += file_bytes;
    valid_file_count++;
  }
  const int bytes_written = static_cast<int>(buffer - original);
  if (!dry_run) {
    // Now we can write the header to the beginning of the buffer.
    SerializedHeader header;
    header.version = GetCurrentVersion();
    header.file_count = valid_file_count;
    header.file_size = bytes_written;
    memcpy(original, &header, sizeof(SerializedHeader));
  }

  return bytes_written;
}

void VirtualFileSystem::Deserialize(const uint8* buffer, int buffer_size) {
  base::AutoLock lock(file_table_lock_);
  ClearFileTable();

  if (buffer_size < 0) {
    DLOG(ERROR) << "Buffer size must be positive: "
                << buffer_size << " < 0.";
    return;
  }

  // The size of the buffer must be checked before copying the beginning of it
  // into a SerializedHeader.
  if (static_cast<size_t>(buffer_size) < sizeof(SerializedHeader)) {
    DLOG(ERROR) << "Buffer size " << buffer_size
                << " is too small to contain a SerializedHeader of size "
                << sizeof(SerializedHeader) << "; operation aborted.";
    return;
  }

  // Read in expected number of files
  SerializedHeader header;
  memcpy(&header, buffer, sizeof(SerializedHeader));
  buffer += sizeof(SerializedHeader);

  int buffer_version = GetHeaderVersion(header);
  if (buffer_version != 0) {
    // Note: We would handle old versions here, if necessary.
    DLOG(ERROR) << "Attempted to load a different version; operation aborted.";
    return;
  } else if (header.file_size != buffer_size) {
    DLOG(ERROR) << "Buffer size mismatch: " << header.file_size
                << " != " << buffer_size;
    return;
  }

  for (int i = 0; i < header.file_count; ++i) {
    VirtualFile* file = new VirtualFile("");
    int bytes = file->Deserialize(buffer);
    if (bytes > 0) {
      buffer += bytes;
      table_[file->name_] = file;
    } else {
      DLOG(WARNING) << "Failed to deserialize virtual file system.";
    }
  }
}

void VirtualFileSystem::ClearFileTable() {
  DCHECK(thread_checker_.CalledOnValidThread());
  file_table_lock_.AssertAcquired();
  for (FileTable::iterator it = table_.begin(); it != table_.end(); ++it) {
    delete it->second;
  }
  table_.clear();
}

}  // namespace storage
}  // namespace cobalt
