// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/cache_util.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_util.h"

namespace disk_cache {

bool MoveCache(const FilePath& from_path, const FilePath& to_path) {
#if defined(OS_CHROMEOS)
  // For ChromeOS, we don't actually want to rename the cache
  // directory, because if we do, then it'll get recreated through the
  // encrypted filesystem (with encrypted names), and we won't be able
  // to see these directories anymore in an unmounted encrypted
  // filesystem, so we just move each item in the cache to a new
  // directory.
  if (!file_util::CreateDirectory(to_path)) {
    LOG(ERROR) << "Unable to create destination cache directory.";
    return false;
  }
  file_util::FileEnumerator iter(
      from_path,
      /* recursive */ false,
      static_cast<file_util::FileEnumerator::FILE_TYPE>(
          file_util::FileEnumerator::DIRECTORIES |
          file_util::FileEnumerator::FILES));
  for (FilePath name = iter.Next(); !name.value().empty(); name = iter.Next()) {
    FilePath destination = to_path.Append(name.BaseName());
    if (!file_util::Move(name, destination)) {
      LOG(ERROR) << "Unable to move cache item.";
      return false;
    }
  }
  return true;
#else
  return file_util::Move(from_path, to_path);
#endif
}

void DeleteCache(const FilePath& path, bool remove_folder) {
  file_util::FileEnumerator iter(path,
                                 /* recursive */ false,
                                 file_util::FileEnumerator::FILES);
  for (FilePath file = iter.Next(); !file.value().empty(); file = iter.Next()) {
    if (!file_util::Delete(file, /* recursive */ false)) {
      LOG(WARNING) << "Unable to delete cache.";
      return;
    }
  }

  if (remove_folder) {
    if (!file_util::Delete(path, /* recursive */ false)) {
      LOG(WARNING) << "Unable to delete cache folder.";
      return;
    }
  }
}

bool DeleteCacheFile(const FilePath& name) {
  return file_util::Delete(name, false);
}

}  // namespace disk_cache
