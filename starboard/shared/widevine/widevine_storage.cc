// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/widevine/widevine_storage.h"

#include "starboard/common/file.h"
#include "starboard/common/log.h"
#include "starboard/shared/widevine/widevine_keybox_hash.h"
#include "starboard/types.h"

namespace starboard {
namespace shared {
namespace widevine {

// Reserved key name for referring to the Widevine Keybox checksum value.
const char WidevineStorage::kCobaltWidevineKeyboxChecksumKey[] =
    "cobalt_widevine_keybox_checksum";

namespace {

void ReadFile(const std::string& path_name, std::vector<uint8_t>* content) {
  SB_DCHECK(content);

  ScopedFile file(path_name.c_str(), kSbFileOpenOnly | kSbFileRead);

  if (!file.IsValid()) {
    content->clear();
    SB_LOG(INFO) << "Failed to open " << path_name
                 << ", returning empty content.";
    return;
  }

  auto size = file.GetSize();
  if (size < 0) {
    content->clear();
    SB_LOG(INFO) << "Failed to get size of " << path_name
                 << ", returning empty content.";
    return;
  }

  content->resize(size);
  if (file.ReadAll(reinterpret_cast<char*>(content->data()), size) != size) {
    content->clear();
    SB_LOG(INFO) << "Failed to read content of " << path_name
                 << ", returning empty content.";
    return;
  }
}

bool WriteFile(const std::string& path_name,
               const std::vector<uint8_t>& content) {
  ScopedFile file(path_name.c_str(), kSbFileCreateAlways | kSbFileWrite);

  if (!file.IsValid()) {
    SB_LOG(INFO) << "Failed to create " << path_name << " for writing.";
    return false;
  }

  if (file.WriteAll(reinterpret_cast<const char*>(content.data()),
                    static_cast<int>(content.size())) != content.size()) {
    SB_LOG(INFO) << "Failed to write content to " << path_name << '.';
    return false;
  }

  file.Flush();

  return true;
}

bool ReadString(const std::vector<uint8_t>& data,
                size_t* offset,
                std::string* str) {
  if (*offset + sizeof(int) > data.size()) {
    SB_LOG(ERROR) << "Failed to read the size of string from |data|.";
    return false;
  }
  int size = *reinterpret_cast<const int*>(data.data() + *offset);
  *offset += sizeof(int);
  if (*offset + size > data.size()) {
    SB_LOG(ERROR) << "Failed to read " << size << " bytes from |data|.";
    return false;
  }
  str->assign(reinterpret_cast<const char*>(data.data() + *offset),
              reinterpret_cast<const char*>(data.data() + *offset + size));
  *offset += size;
  return true;
}

void WriteString(const std::string& str, std::vector<uint8_t>* content) {
  content->reserve(content->size() + sizeof(int) + str.size());
  content->resize(content->size() + sizeof(int));
  *reinterpret_cast<int*>(content->data() + content->size() - sizeof(int)) =
      static_cast<int>(str.size());
  content->insert(content->end(), reinterpret_cast<const uint8_t*>(str.c_str()),
                  reinterpret_cast<const uint8_t*>(str.c_str()) + str.size());
}

}  // namespace

WidevineStorage::WidevineStorage(const std::string& path_name)
    : path_name_(path_name) {
  std::vector<uint8_t> content;
  ReadFile(path_name_, &content);
  size_t offset = 0;

  while (offset != content.size()) {
    std::string name, value;
    if (!ReadString(content, &offset, &name) ||
        !ReadString(content, &offset, &value)) {
      cache_.clear();
      SB_LOG(WARNING) << path_name_ << " is corrupt, returns empty content.";
      return;
    }
    cache_[name] = value;
  }

  SB_LOG(INFO) << "Loaded " << cache_.size() << " records from " << path_name_;

  // Not a cryptographic hash but is sufficient for this problem space.
  std::string keybox_checksum = GetWidevineKeyboxHash();
  if (existsInternal(kCobaltWidevineKeyboxChecksumKey)) {
    std::string cached_checksum;
    readInternal(kCobaltWidevineKeyboxChecksumKey, &cached_checksum);
    if (keybox_checksum == cached_checksum) {
      return;
    }
    SB_LOG(INFO) << "Cobalt widevine keybox checksums do not match. Clearing "
                    "to force re-provisioning.";
    cache_.clear();
    // Save the new keybox checksum to be used after provisioning completes.
    writeInternal(kCobaltWidevineKeyboxChecksumKey, keybox_checksum);
    return;
  }
  SB_LOG(INFO) << "Widevine checksum is not stored on disk. Writing the "
                  "computed checksum to disk.";
  writeInternal(kCobaltWidevineKeyboxChecksumKey, keybox_checksum);
}

bool WidevineStorage::read(const std::string& name, std::string* data) {
  SB_DCHECK(name != kCobaltWidevineKeyboxChecksumKey);
  return readInternal(name, data);
}

bool WidevineStorage::write(const std::string& name, const std::string& data) {
  SB_DCHECK(name != kCobaltWidevineKeyboxChecksumKey);
  return writeInternal(name, data);
}

bool WidevineStorage::exists(const std::string& name) {
  SB_DCHECK(name != kCobaltWidevineKeyboxChecksumKey);
  return existsInternal(name);
}

bool WidevineStorage::remove(const std::string& name) {
  SB_DCHECK(name != kCobaltWidevineKeyboxChecksumKey);
  return removeInternal(name);
}

int32_t WidevineStorage::size(const std::string& name) {
  SB_DCHECK(name != kCobaltWidevineKeyboxChecksumKey);
  ScopedLock scoped_lock(lock_);
  auto iter = cache_.find(name);
  return iter == cache_.end() ? -1 : static_cast<int32_t>(iter->second.size());
}

bool WidevineStorage::list(std::vector<std::string>* records) {
  SB_DCHECK(records);
  ScopedLock scoped_lock(lock_);
  records->clear();
  for (auto item : cache_) {
    if (item.first == kCobaltWidevineKeyboxChecksumKey) {
      continue;
    }
    records->push_back(item.first);
  }
  return !records->empty();
}

bool WidevineStorage::readInternal(const std::string& name,
                                   std::string* data) const {
  SB_DCHECK(data);
  ScopedLock scoped_lock(lock_);
  auto iter = cache_.find(name);
  if (iter == cache_.end()) {
    return false;
  }
  *data = iter->second;
  return true;
}

bool WidevineStorage::writeInternal(const std::string& name,
                                    const std::string& data) {
  ScopedLock scoped_lock(lock_);
  cache_[name] = data;

  std::vector<uint8_t> content;
  for (auto iter : cache_) {
    WriteString(iter.first, &content);
    WriteString(iter.second, &content);
  }
  return WriteFile(path_name_, content);
}

bool WidevineStorage::existsInternal(const std::string& name) const {
  ScopedLock scoped_lock(lock_);
  return cache_.find(name) != cache_.end();
}

bool WidevineStorage::removeInternal(const std::string& name) {
  ScopedLock scoped_lock(lock_);
  auto iter = cache_.find(name);
  if (iter == cache_.end()) {
    return false;
  }
  cache_.erase(iter);
  return true;
}

}  // namespace widevine
}  // namespace shared
}  // namespace starboard
