// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche_platform_impl/quiche_file_utils_impl.h"

#if defined(STARBOARD)
#include <sys/stat.h>
#include <unistd.h>

#include "starboard/configuration_constants.h"
#include "starboard/directory.h"
#elif defined(_WIN32)
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif  // defined(_WIN32)

#include <fstream>
#include <ios>
#include <iostream>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "absl/types/optional.h"
#include "base/files/file.h"

namespace quiche {

#if defined(_WIN32)
std::string JoinPathImpl(absl::string_view a, absl::string_view b) {
  if (a.empty()) {
    return std::string(b);
  }
  if (b.empty()) {
    return std::string(a);
  }
  // Win32 actually provides two different APIs for combining paths; one of them
  // has issues that could potentially lead to buffer overflow, and another is
  // not supported in Windows 7, which is why we're doing it manually.
  a = absl::StripSuffix(a, "/");
  a = absl::StripSuffix(a, "\\");
  return absl::StrCat(a, "\\", b);
}
#else
std::string JoinPathImpl(absl::string_view a, absl::string_view b) {
  if (a.empty()) {
    return std::string(b);
  }
  if (b.empty()) {
    return std::string(a);
  }
  return absl::StrCat(absl::StripSuffix(a, "/"), "/", b);
}
#endif  // defined(_WIN32)

absl::optional<std::string> ReadFileContentsImpl(absl::string_view file) {
#if defined(STARBOARD)
  base::File file_handle(base::FilePath(file), base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file_handle.IsValid()) {
    return absl::nullopt;
  }

  auto max_size = std::numeric_limits<size_t>::max();
  std::string contents;
  const size_t kBufferSize = 1 << 12;
  char buf[kBufferSize];
  size_t len;
  size_t size = 0;
  bool read_status = true;

  while ((len = file_handle.ReadAtCurrentPos(buf, sizeof(buf))) > 0) {
    size_t bytes_to_add = std::min(len, max_size - size);
    if (size + bytes_to_add > contents.max_size()) {
      return absl::nullopt;
    }
    contents.append(buf, std::min(len, max_size - size));

    if ((max_size - size) < len) {
      return absl::nullopt;
    }

    size += len;
  }
  return contents;
#else
  std::ifstream input_file(std::string{file}, std::ios::binary);
  if (!input_file || !input_file.is_open()) {
    return absl::nullopt;
  }

  input_file.seekg(0, std::ios_base::end);
  auto file_size = input_file.tellg();
  if (!input_file) {
    return absl::nullopt;
  }
  input_file.seekg(0, std::ios_base::beg);

  std::string output;
  output.resize(file_size);
  input_file.read(&output[0], file_size);
  if (!input_file) {
    return absl::nullopt;
  }

  return output;
#endif
}

#if defined(STARBOARD)

class ScopedDir {
 public:
  ScopedDir(SbDirectory dir) : dir_(dir) {}
  ~ScopedDir() {
    if (SbDirectoryIsValid(dir_)) {
      SbDirectoryClose(dir_);
      dir_ = nullptr;
    }
  }

  SbDirectory get() { return dir_; }

 private:
  SbDirectory dir_;
};

bool EnumerateDirectoryImpl(absl::string_view path,
                            std::vector<std::string>& directories,
                            std::vector<std::string>& files) {
  std::string path_owned(path);
  ScopedDir dir(SbDirectoryOpen(path_owned.c_str(), nullptr));
  if (!SbDirectoryIsValid(dir.get())) {
    return false;
  }

  while (true) {
    std::vector<char> entry(kSbFileMaxName, 0);
    if (!SbDirectoryGetNext(dir.get(), entry.data(), entry.size())) {
      break;
    }
    std::string filename(entry.begin(), entry.end());
    if (filename == "." || filename == "..") {
      continue;
    }

    const std::string entry_path = JoinPathImpl(path, filename);
    struct stat stat_entry;
    if (stat(entry_path.c_str(), &stat_entry) != 0) {
      return false;
    }
    if (S_ISREG(stat_entry.st_mode)) {
      files.push_back(std::move(filename));
    } else if (S_ISDIR(stat_entry.st_mode)) {
      directories.push_back(std::move(filename));
    }
  }
  return true;
}

#elif defined(_WIN32)

class ScopedDir {
 public:
  ScopedDir(HANDLE dir) : dir_(dir) {}
  ~ScopedDir() {
    if (dir_ != INVALID_HANDLE_VALUE) {
      // The API documentation explicitly says that CloseHandle() should not be
      // used on directory search handles.
      FindClose(dir_);
      dir_ = INVALID_HANDLE_VALUE;
    }
  }

  HANDLE get() { return dir_; }

 private:
  HANDLE dir_;
};

bool EnumerateDirectoryImpl(absl::string_view path,
                            std::vector<std::string>& directories,
                            std::vector<std::string>& files) {
  std::string path_owned(path);

  // Explicitly check that the directory we are trying to search is in fact a
  // directory.
  DWORD attributes = GetFileAttributesA(path_owned.c_str());
  if (attributes == INVALID_FILE_ATTRIBUTES) {
    return false;
  }
  if ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
    return false;
  }

  std::string search_path = JoinPathImpl(path, "*");
  WIN32_FIND_DATAA file_data;
  ScopedDir dir(FindFirstFileA(search_path.c_str(), &file_data));
  if (dir.get() == INVALID_HANDLE_VALUE) {
    return GetLastError() == ERROR_FILE_NOT_FOUND;
  }
  do {
    std::string filename(file_data.cFileName);
    if (filename == "." || filename == "..") {
      continue;
    }
    if ((file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
      directories.push_back(std::move(filename));
    } else {
      files.push_back(std::move(filename));
    }
  } while (FindNextFileA(dir.get(), &file_data));
  return GetLastError() == ERROR_NO_MORE_FILES;
}

#else  // defined(_WIN32)

class ScopedDir {
 public:
  ScopedDir(DIR* dir) : dir_(dir) {}
  ~ScopedDir() {
    if (dir_ != nullptr) {
      closedir(dir_);
      dir_ = nullptr;
    }
  }

  DIR* get() { return dir_; }

 private:
  DIR* dir_;
};

bool EnumerateDirectoryImpl(absl::string_view path,
                            std::vector<std::string>& directories,
                            std::vector<std::string>& files) {
  std::string path_owned(path);
  ScopedDir dir(opendir(path_owned.c_str()));
  if (dir.get() == nullptr) {
    return false;
  }

  dirent* entry;
  while ((entry = readdir(dir.get()))) {
    const std::string filename(entry->d_name);
    if (filename == "." || filename == "..") {
      continue;
    }

    const std::string entry_path = JoinPathImpl(path, filename);
    struct stat stat_entry;
    if (stat(entry_path.c_str(), &stat_entry) != 0) {
      return false;
    }
    if (S_ISREG(stat_entry.st_mode)) {
      files.push_back(std::move(filename));
    } else if (S_ISDIR(stat_entry.st_mode)) {
      directories.push_back(std::move(filename));
    }
  }
  return true;
}

#endif  // defined(_WIN32)

}  // namespace quiche
