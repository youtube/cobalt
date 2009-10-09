// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_file_util.h"

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <string>

#include "base/logging.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/string_util.h"

namespace file_util {

bool DieFileDie(const FilePath& file, bool recurse) {
  // There is no need to workaround Windows problems on POSIX.
  // Just pass-through.
  return file_util::Delete(file, recurse);
}

// Mostly a verbatim copy of CopyDirectory
bool CopyRecursiveDirNoCache(const std::wstring& source_dir,
                             const std::wstring& dest_dir) {
  const FilePath from_path(FilePath::FromWStringHack(source_dir));
  const FilePath to_path(FilePath::FromWStringHack(dest_dir));

  char top_dir[PATH_MAX];
  if (base::strlcpy(top_dir, from_path.value().c_str(),
                    arraysize(top_dir)) >= arraysize(top_dir)) {
    return false;
  }

  // This function does not properly handle destinations within the source
  FilePath real_to_path = to_path;
  if (PathExists(real_to_path)) {
    if (!AbsolutePath(&real_to_path))
      return false;
  } else {
    real_to_path = real_to_path.DirName();
    if (!AbsolutePath(&real_to_path))
      return false;
  }
  if (real_to_path.value().compare(0, from_path.value().size(),
      from_path.value()) == 0)
    return false;

  bool success = true;
  FileEnumerator::FILE_TYPE traverse_type =
      static_cast<FileEnumerator::FILE_TYPE>(FileEnumerator::FILES |
      FileEnumerator::SHOW_SYM_LINKS | FileEnumerator::DIRECTORIES);
  FileEnumerator traversal(from_path, true, traverse_type);

  // to_path may not exist yet, start the loop with to_path
  FileEnumerator::FindInfo info;
  FilePath current = from_path;
  if (stat(from_path.value().c_str(), &info.stat) < 0) {
    LOG(ERROR) << "CopyRecursiveDirNoCache() couldn't stat source directory: "
        << from_path.value() << " errno = " << errno;
    success = false;
  }

  while (success && !current.empty()) {
    // current is the source path, including from_path, so paste
    // the suffix after from_path onto to_path to create the target_path.
    std::string suffix(&current.value().c_str()[from_path.value().size()]);
    // Strip the leading '/' (if any).
    if (!suffix.empty()) {
      DCHECK_EQ('/', suffix[0]);
      suffix.erase(0, 1);
    }
    const FilePath target_path = to_path.Append(suffix);

    if (S_ISDIR(info.stat.st_mode)) {
      if (mkdir(target_path.value().c_str(), info.stat.st_mode & 01777) != 0 &&
          errno != EEXIST) {
        LOG(ERROR) << "CopyRecursiveDirNoCache() couldn't create directory: " <<
            target_path.value() << " errno = " << errno;
        success = false;
      }
    } else if (S_ISREG(info.stat.st_mode)) {
      if (CopyFile(current, target_path)) {
        success = EvictFileFromSystemCache(target_path);
        DCHECK(success);
      } else {
        LOG(ERROR) << "CopyRecursiveDirNoCache() couldn't create file: " <<
            target_path.value();
        success = false;
      }
    } else {
      LOG(WARNING) << "CopyRecursiveDirNoCache() skipping non-regular file: " <<
          current.value();
    }

    current = traversal.Next();
    traversal.GetFindInfo(&info);
  }

  return success;
}

}  // namespace file_util
