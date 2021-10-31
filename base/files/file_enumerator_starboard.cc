// Copyright 2018 Google Inc. All Rights Reserved.
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

#include "base/files/file_enumerator.h"

#include "base/files/file_util.h"
#include "base/threading/thread_restrictions.h"
#include "starboard/common/string.h"
#include "starboard/configuration_constants.h"
#include "starboard/directory.h"
#include "starboard/file.h"
#include "starboard/memory.h"

namespace base {

// FileEnumerator::FileInfo ----------------------------------------------------

FileEnumerator::FileInfo::FileInfo() {
  memset(&sb_info_, 0, sizeof(sb_info_));
}

bool FileEnumerator::FileInfo::IsDirectory() const {
  return sb_info_.is_directory;
}

FilePath FileEnumerator::FileInfo::GetName() const {
  return filename_;
}

int64_t FileEnumerator::FileInfo::GetSize() const {
  return sb_info_.size;
}

base::Time FileEnumerator::FileInfo::GetLastModifiedTime() const {
  return base::Time::FromSbTime(sb_info_.last_modified);
}

// FileEnumerator --------------------------------------------------------------

FileEnumerator::FileEnumerator(const FilePath &root_path,
                               bool recursive,
                               int file_type)
    : FileEnumerator(root_path, recursive, file_type, FilePath::StringType(),
                     FolderSearchPolicy::MATCH_ONLY) {}

FileEnumerator::FileEnumerator(const FilePath &root_path,
                               bool recursive,
                               int file_type,
                               const FilePath::StringType &pattern)
    : FileEnumerator(root_path, recursive, file_type, pattern,
                     FolderSearchPolicy::MATCH_ONLY) {}

FileEnumerator::FileEnumerator(const FilePath& root_path,
                               bool recursive,
                               int file_type,
                               const FilePath::StringType& pattern,
                               FolderSearchPolicy folder_search_policy)
    : current_directory_entry_(0),
      root_path_(root_path),
      recursive_(recursive),
      file_type_(file_type),
      pattern_(pattern),
      folder_search_policy_(folder_search_policy) {
  // INCLUDE_DOT_DOT must not be specified if recursive.
  DCHECK(!(recursive && (INCLUDE_DOT_DOT & file_type_)));
  // The Windows version of this code appends the pattern to the root_path,
  // potentially only matching against items in the top-most directory.
  // Do the same here.
  if (pattern.empty()) {
    pattern_ = FilePath::StringType();
  }
  pending_paths_.push(root_path);
}

FileEnumerator::~FileEnumerator() = default;

//static
std::vector<FileEnumerator::FileInfo> FileEnumerator::ReadDirectory(
    const FilePath& source) {
  AssertBlockingAllowed();
  SbDirectory dir = SbDirectoryOpen(source.value().c_str(), NULL);
  if (!SbDirectoryIsValid(dir)) {
    return std::vector<FileEnumerator::FileInfo>();
  }

  auto GenerateEntry = [source](std::string filename) {
    FileEnumerator::FileInfo info;
    info.filename_ = FilePath(filename);

    FilePath full_name = source.Append(filename);
    // TODO: Make sure this follows symlinks on relevant platforms.
    if (!SbFileGetPathInfo(full_name.value().c_str(), &info.sb_info_)) {
      DPLOG(ERROR) << "Couldn't SbFileGetInfo on " << full_name.value();
      memset(&info.sb_info_, 0, sizeof(info.sb_info_));
    }
    return info;
  };

  std::vector<FileEnumerator::FileInfo> ret;
  // We test if SbDirectoryGetNext returns the parent directory, i.e. |..|,
  // because whether or not it is returned is platform-dependent and we need to
  // be able to guarantee it is returned when the INCLUDE_DOT_DOT bitflag is
  // set.
  bool found_dot_dot = false;

#if SB_API_VERSION >= 12
  std::vector<char> entry(kSbFileMaxName);

  while (SbDirectoryGetNext(dir, entry.data(), entry.size())) {
    const char dot_dot_str[] = "..";
    if (!strncmp(entry.data(), dot_dot_str, sizeof(dot_dot_str))) {
      found_dot_dot = true;
    }
    ret.push_back(GenerateEntry(entry.data()));
  }
#else   // SB_API_VERSION >= 12
  SbDirectoryEntry entry;

  while (SbDirectoryGetNext(dir, &entry)) {
    const char dot_dot_str[] = "..";
    if (!strncmp(entry.name, dot_dot_str, sizeof(dot_dot_str))) {
      found_dot_dot = true;
    }
    ret.push_back(GenerateEntry(entry.name));
  }
#endif  // SB_API_VERSION >= 12

  if ((INCLUDE_DOT_DOT & file_type_) && !found_dot_dot) {
    ret.push_back(GenerateEntry(".."));
  }

  ignore_result(SbDirectoryClose(dir));
  return ret;
}

FilePath FileEnumerator::Next() {
  AssertBlockingAllowed();

  ++current_directory_entry_;

  // While we've exhausted the entries in the current directory, do the next
  while (current_directory_entry_ >= directory_entries_.size()) {
    if (pending_paths_.empty()) {
      return FilePath();
    }

    root_path_ = pending_paths_.top();
    root_path_ = root_path_.StripTrailingSeparators();
    pending_paths_.pop();

    std::vector<FileInfo> entries = ReadDirectory(root_path_);

    directory_entries_.clear();
    current_directory_entry_ = 0;
    for (const auto& file_info : entries) {
      FilePath full_path = root_path_.Append(file_info.filename_);
      if (ShouldSkip(full_path)) {
        continue;
      }

      if (pattern_.size()) {
        NOTREACHED() << "Patterns not supported in Starboard.";
        continue;
      }

      if (recursive_ && file_info.sb_info_.is_directory) {
        pending_paths_.push(full_path);
      }

      if ((file_info.sb_info_.is_directory && (file_type_ & DIRECTORIES)) ||
          (!file_info.sb_info_.is_directory && (file_type_ & FILES))) {
        directory_entries_.push_back(file_info);
      }
    }
  }

  return
      root_path_.Append(directory_entries_[current_directory_entry_].filename_);
}

FileEnumerator::FileInfo FileEnumerator::GetInfo() const {
  return directory_entries_[current_directory_entry_];
}

}  // namespace base
