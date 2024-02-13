// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/cache/memory_capped_directory.h"

#include <algorithm>
#include <string>

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "starboard/directory.h"

namespace cobalt {
namespace cache {

namespace {

const char* kMetadataExtension = ".json";

}  // namespace

MemoryCappedDirectory::FileInfo::FileInfo(
    base::FilePath directory_path, base::FileEnumerator::FileInfo file_info)
    : file_path_(directory_path.Append(file_info.GetName())),
      last_modified_time_(file_info.GetLastModifiedTime()),
      size_(static_cast<uint32_t>(file_info.GetSize())) {}

MemoryCappedDirectory::FileInfo::FileInfo(base::FilePath file_path,
                                          base::Time last_modified_time,
                                          uint32_t size)
    : file_path_(file_path),
      last_modified_time_(last_modified_time),
      size_(size) {}

bool MemoryCappedDirectory::FileInfo::OldestFirst::operator()(
    const MemoryCappedDirectory::FileInfo& left,
    const MemoryCappedDirectory::FileInfo& right) const {
  return left.last_modified_time_ > right.last_modified_time_;
}

// static
std::unique_ptr<MemoryCappedDirectory> MemoryCappedDirectory::Create(
    const base::FilePath& directory_path, uint32_t max_size) {
  if (!SbDirectoryCreate(directory_path.value().c_str())) {
    return nullptr;
  }
  auto memory_capped_directory = std::unique_ptr<MemoryCappedDirectory>(
      new MemoryCappedDirectory(directory_path, max_size));
  auto* heap = &memory_capped_directory->file_info_heap_;
  base::AutoLock auto_lock(memory_capped_directory->lock_);
  base::FileEnumerator file_enumerator(directory_path, false,
                                       base::FileEnumerator::FILES);
  uint32_t size = 0u;
  while (!file_enumerator.Next().empty()) {
    MemoryCappedDirectory::FileInfo file_info(directory_path,
                                              file_enumerator.GetInfo());
    // TODO: include metadata size in file sizes.
    if (file_info.file_path_.Extension() == kMetadataExtension) {
      memory_capped_directory->file_keys_with_metadata_[file_info.file_path_] =
          std::stoul(
              file_info.file_path_.BaseName().RemoveExtension().MaybeAsASCII());
      continue;
    }
    heap->push_back(file_info);
    memory_capped_directory->file_sizes_[file_info.file_path_] =
        file_info.size_;
    memory_capped_directory->size_ += file_info.size_;
  }
  if (heap->size() > 1) {
    std::make_heap(heap->begin(), heap->end(),
                   MemoryCappedDirectory::FileInfo::OldestFirst());
  }
  return memory_capped_directory;
}

bool MemoryCappedDirectory::Delete(uint32_t key) {
  base::AutoLock auto_lock(lock_);
  auto file_path = GetFilePath(key);
  if (base::PathExists(file_path)) {
    base::DeleteFile(file_path);
  }
  auto metadata_path = file_path.AddExtension(kMetadataExtension);
  if (base::PathExists(metadata_path)) {
    base::DeleteFile(metadata_path);
  }
  file_sizes_.erase(file_path);
  file_keys_with_metadata_.erase(metadata_path);
  auto* heap = &file_info_heap_;
  for (auto it = heap->begin(); it != heap->end(); ++it) {
    if (it->file_path_ == file_path) {
      size_ -= it->size_;
      heap->erase(it);
      if (heap->size() > 1) {
        std::make_heap(heap->begin(), heap->end(),
                       MemoryCappedDirectory::FileInfo::OldestFirst());
      }
      return true;
    }
  }
  return false;
}

void MemoryCappedDirectory::DeleteAll() {
  base::AutoLock auto_lock(lock_);
  // Recursively delete the contents of the directory_path_.
  base::DeleteFile(directory_path_);
  // Re-create the directory_path_ which will now be empty.
  SbDirectoryCreate(directory_path_.value().c_str());
  file_info_heap_.clear();
  file_sizes_.clear();
  file_keys_with_metadata_.clear();
  size_ = 0;
}

std::vector<uint32_t> MemoryCappedDirectory::KeysWithMetadata() {
  std::vector<uint32_t> keys;
  for (auto it = file_keys_with_metadata_.begin();
       it != file_keys_with_metadata_.end(); ++it) {
    keys.push_back(it->second);
  }
  return keys;
}

base::Optional<base::Value> MemoryCappedDirectory::Metadata(uint32_t key) {
  auto metadata_path = GetFilePath(key).AddExtension(kMetadataExtension);
  if (!base::PathExists(metadata_path)) {
    return base::nullopt;
  }
  std::string serialized_metadata;
  base::ReadFileToString(metadata_path, &serialized_metadata);

#ifndef USE_HACKY_COBALT_CHANGES
  return base::Value::FromUniquePtrValue(
      base::JSONReader::Read(serialized_metadata));
#else
  return base::Value::FromUniquePtrValue(nullptr);
#endif
}

std::unique_ptr<std::vector<uint8_t>> MemoryCappedDirectory::Retrieve(
    uint32_t key) {
  auto file_path = GetFilePath(key);
  auto it = file_sizes_.find(file_path);
  if (it == file_sizes_.end()) {
    return nullptr;
  }
  if (!base::PathExists(file_path)) {
    Delete(key);
    return nullptr;
  }
  auto size = it->second;
  auto data = std::make_unique<std::vector<uint8_t>>(static_cast<size_t>(size));
  int bytes_read = base::ReadFile(
      file_path, reinterpret_cast<char*>(data->data()), static_cast<int>(size));
  if (bytes_read != size) {
    return nullptr;
  }
  return data;
}

void MemoryCappedDirectory::Store(uint32_t key,
                                  const std::vector<uint8_t>& data,
                                  const base::Optional<base::Value>& metadata) {
  base::AutoLock auto_lock(lock_);
  auto file_path = GetFilePath(key);
  uint32_t new_entry_size = static_cast<uint32_t>(data.size());
  if (!EnsureEnoughSpace(new_entry_size)) {
    return;
  }
  if (metadata) {
    std::string serialized_metadata;
    base::JSONWriter::Write(metadata.value(), &serialized_metadata);
    base::WriteFile(file_path.AddExtension(kMetadataExtension),
                    serialized_metadata.data(), serialized_metadata.size());
  }
  int bytes_written = base::WriteFile(
      file_path, reinterpret_cast<const char*>(data.data()), data.size());
  if (bytes_written != data.size()) {
    base::DeleteFile(file_path);
    if (metadata) {
      base::DeleteFile(file_path.AddExtension(kMetadataExtension));
    }
    return;
  }
  size_ += new_entry_size;
  auto* heap = &file_info_heap_;
  heap->push_back(MemoryCappedDirectory::FileInfo(file_path, base::Time::Now(),
                                                  new_entry_size));
  std::push_heap(heap->begin(), heap->end(),
                 MemoryCappedDirectory::FileInfo::OldestFirst());
  file_sizes_[file_path] = new_entry_size;
  file_keys_with_metadata_[file_path] = key;
}

void MemoryCappedDirectory::Resize(uint32_t size) {
  if (max_size_ > size) {
    uint32_t space_to_be_freed = max_size_ - size;
    EnsureEnoughSpace(space_to_be_freed);
  }
  max_size_ = size;
}

MemoryCappedDirectory::MemoryCappedDirectory(
    const base::FilePath& directory_path, uint32_t max_size)
    : directory_path_(directory_path), max_size_(max_size), size_(0u) {}

base::FilePath MemoryCappedDirectory::GetFilePath(uint32_t key) const {
  return directory_path_.Append(std::to_string(key));
}

bool MemoryCappedDirectory::EnsureEnoughSpace(
    uint32_t additional_size_required) {
  if (additional_size_required > max_size_) {
    return false;
  }
  auto* heap = &file_info_heap_;
  while (size_ + additional_size_required > max_size_) {
    if (heap->size() == 0) {
      return false;
    }
    std::pop_heap(heap->begin(), heap->end(),
                  MemoryCappedDirectory::FileInfo::OldestFirst());
    auto removed = heap->back();
    size_ -= removed.size_;
    base::DeleteFile(removed.file_path_);
    auto metadata_path = removed.file_path_.AddExtension(kMetadataExtension);
    if (base::PathExists(metadata_path)) {
      base::DeleteFile(metadata_path);
    }
    file_sizes_.erase(removed.file_path_);
    file_keys_with_metadata_.erase(removed.file_path_);
    heap->pop_back();
  }
  return true;
}

}  // namespace cache
}  // namespace cobalt
