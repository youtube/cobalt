// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_file_util.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"

namespace file_util {

namespace {

// Deny |permission| on the file |path|.
bool DenyFilePermission(const FilePath& path, mode_t permission) {
  struct stat stat_buf;
  if (stat(path.value().c_str(), &stat_buf) != 0)
    return false;
  stat_buf.st_mode &= ~permission;

  int rv = HANDLE_EINTR(chmod(path.value().c_str(), stat_buf.st_mode));
  return rv == 0;
}

// Gets a blob indicating the permission information for |path|.
// |length| is the length of the blob.  Zero on failure.
// Returns the blob pointer, or NULL on failure.
void* GetPermissionInfo(const FilePath& path, size_t* length) {
  DCHECK(length);
  *length = 0;

  struct stat stat_buf;
  if (stat(path.value().c_str(), &stat_buf) != 0)
    return NULL;

  *length = sizeof(mode_t);
  mode_t* mode = new mode_t;
  *mode = stat_buf.st_mode & ~S_IFMT;  // Filter out file/path kind.

  return mode;
}

// Restores the permission information for |path|, given the blob retrieved
// using |GetPermissionInfo()|.
// |info| is the pointer to the blob.
// |length| is the length of the blob.
// Either |info| or |length| may be NULL/0, in which case nothing happens.
bool RestorePermissionInfo(const FilePath& path, void* info, size_t length) {
  if (!info || (length == 0))
    return false;

  DCHECK_EQ(sizeof(mode_t), length);
  mode_t* mode = reinterpret_cast<mode_t*>(info);

  int rv = HANDLE_EINTR(chmod(path.value().c_str(), *mode));

  delete mode;

  return rv == 0;
}

}  // namespace

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
  FileEnumerator::FileType traverse_type =
      static_cast<FileEnumerator::FileType>(FileEnumerator::FILES |
      FileEnumerator::SHOW_SYM_LINKS | FileEnumerator::DIRECTORIES);
  FileEnumerator traversal(source_dir, true, traverse_type);

  // dest_dir may not exist yet, start the loop with dest_dir
  FileEnumerator::FindInfo info;
  FilePath current = source_dir;
  if (stat(source_dir.value().c_str(), &info.stat) < 0) {
    DLOG(ERROR) << "CopyRecursiveDirNoCache() couldn't stat source directory: "
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
        DLOG(ERROR) << "CopyRecursiveDirNoCache() couldn't create directory: "
                    << target_path.value() << " errno = " << errno;
        success = false;
      }
    } else if (S_ISREG(info.stat.st_mode)) {
      if (CopyFile(current, target_path)) {
        success = EvictFileFromSystemCache(target_path);
        DCHECK(success);
      } else {
        DLOG(ERROR) << "CopyRecursiveDirNoCache() couldn't create file: "
                    << target_path.value();
        success = false;
      }
    } else {
      DLOG(WARNING) << "CopyRecursiveDirNoCache() skipping non-regular file: "
                    << current.value();
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

bool MakeFileUnreadable(const FilePath& path) {
  return DenyFilePermission(path, S_IRUSR | S_IRGRP | S_IROTH);
}

bool MakeFileUnwritable(const FilePath& path) {
  return DenyFilePermission(path, S_IWUSR | S_IWGRP | S_IWOTH);
}

PermissionRestorer::PermissionRestorer(const FilePath& path)
    : path_(path), info_(NULL), length_(0) {
  info_ = GetPermissionInfo(path_, &length_);
  DCHECK(info_ != NULL);
  DCHECK_NE(0u, length_);
}

PermissionRestorer::~PermissionRestorer() {
  if (!RestorePermissionInfo(path_, info_, length_))
    NOTREACHED();
}

}  // namespace file_util
