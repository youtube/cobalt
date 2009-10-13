// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/cache_util.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_util.h"

namespace disk_cache {

bool MoveCache(const FilePath& from_path, const FilePath& to_path) {
  // Just use the version from base.
  return file_util::Move(from_path, to_path);
}

void DeleteCache(const FilePath& path, bool remove_folder) {
  file_util::FileEnumerator iter(path,
                                 /* recursive */ false,
                                 file_util::FileEnumerator::FILES);
  for (FilePath file = iter.Next(); !file.value().empty(); file = iter.Next()) {
    if (!file_util::Delete(file, /* recursive */ false))
      NOTREACHED();
  }

  if (remove_folder) {
    if (!file_util::Delete(path, /* recursive */ false))
      NOTREACHED();
  }
}

bool DeleteCacheFile(const FilePath& name) {
  return file_util::Delete(name, false);
}

}  // namespace disk_cache
