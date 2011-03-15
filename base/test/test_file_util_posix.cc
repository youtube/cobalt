// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_file_util.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <string>

#include "base/logging.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"

namespace file_util {

bool DieFileDie(const FilePath& file, bool recurse) {
  // There is no need to workaround Windows problems on POSIX.
  // Just pass-through.
  return file_util::Delete(file, recurse);
}

// Mostly a verbatim copy of CopyDirectory
bool CopyRecursiveDirNoCache(const FilePath& source_dir,
                             const FilePath& dest_dir) {
  char top_dir[PATH_MAX];
  if (base::strlcpy(top_dir, source_dir.value().c_str(),
                    arraysize(top_dir)) >= arraysize(top_dir)) {
    return false;
  }

  // This function does not properly handle destinations within the source
  FilePath real_to_path = dest_dir;
  if (PathExists(real_to_path)) {
    if (!AbsolutePath(&real_to_path))
      return false;
  } else {
    real_to_path = real_to_path.DirName();
    if (!AbsolutePath(&real_to_path))
      return false;
  }
  if (real_to_path.value().compare(0, source_dir.value().size(),
      source_dir.value()) == 0)
    return false;

  bool success = true;
  FileEnumerator::FILE_TYPE traverse_type =
      static_cast<FileEnumerator::FILE_TYPE>(FileEnumerator::FILES |
      FileEnumerator::SHOW_SYM_LINKS | FileEnumerator::DIRECTORIES);
  FileEnumerator traversal(source_dir, true, traverse_type);

  // dest_dir may not exist yet, start the loop with dest_dir
  FileEnumerator::FindInfo info;
  FilePath current = source_dir;
  if (stat(source_dir.value().c_str(), &info.stat) < 0) {
    LOG(ERROR) << "CopyRecursiveDirNoCache() couldn't stat source directory: "
        << source_dir.value() << " errno = " << errno;
    success = false;
  }

  while (success && !current.empty()) {
    // |current| is the source path, including source_dir, so paste
    // the suffix after source_dir onto dest_dir to create the target_path.
    std::string suffix(&current.value().c_str()[source_dir.value().size()]);
    // Strip the leading '/' (if any).
    if (!suffix.empty()) {
      DCHECK_EQ('/', suffix[0]);
      suffix.erase(0, 1);
    }
    const FilePath target_path = dest_dir.Append(suffix);

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

#if !defined(OS_LINUX) && !defined(OS_MACOSX)
bool EvictFileFromSystemCache(const FilePath& file) {
  // There doesn't seem to be a POSIX way to cool the disk cache.
  NOTIMPLEMENTED();
  return false;
}
#endif

std::wstring FilePathAsWString(const FilePath& path) {
  return UTF8ToWide(path.value());
}
FilePath WStringAsFilePath(const std::wstring& path) {
  return FilePath(WideToUTF8(path));
}

}  // namespace file_util
