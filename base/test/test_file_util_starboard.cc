// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "base/test/test_file_util.h"

#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "starboard/directory.h"
#include "starboard/file.h"
#include "starboard/system.h"

namespace file_util {

bool DieFileDie(const FilePath& file, bool recurse) {
  // There is no need to workaround Windows problems on Starboard.  Just
  // pass-through.
  return file_util::Delete(file, recurse);
}

// Mostly a verbatim copy of CopyDirectory
bool CopyRecursiveDirNoCache(const FilePath& source_dir,
                             const FilePath& dest_dir) {
  char top_dir[PATH_MAX];
  if (base::strlcpy(top_dir, source_dir.value().c_str(), arraysize(top_dir)) >=
      arraysize(top_dir)) {
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
  int traverse_type = FileEnumerator::FILES | FileEnumerator::DIRECTORIES;
  FileEnumerator traversal(source_dir, true, traverse_type);

  // dest_dir may not exist yet, start the loop with dest_dir
  FileEnumerator::FindInfo info;
  FilePath current = source_dir;
  if (!SbFileGetPathInfo(source_dir.value().c_str(), &info.sb_info)) {
    DLOG(ERROR) << "CopyRecursiveDirNoCache() couldn't stat source directory: "
                << source_dir.value() << " error = " << SbSystemGetLastError();
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

    if (info.sb_info.is_directory) {
      if (SbDirectoryCreate(target_path.value().c_str())) {
        DLOG(ERROR) << "CopyRecursiveDirNoCache() couldn't create directory: "
                    << target_path.value()
                    << " error = " << SbSystemGetLastError();
        success = false;
      }
    } else {
      if (CopyFile(current, target_path)) {
        success = true;
      } else {
        DLOG(ERROR) << "CopyRecursiveDirNoCache() couldn't create file: "
                    << target_path.value();
        success = false;
      }
    }

    current = traversal.Next();
    traversal.GetFindInfo(&info);
  }

  return success;
}

std::wstring FilePathAsWString(const FilePath& path) {
  return UTF8ToWide(path.value());
}

FilePath WStringAsFilePath(const std::wstring& path) {
  return FilePath(WideToUTF8(path));
}

bool MakeFileUnreadable(const FilePath& path) {
  NOTIMPLEMENTED();
  return true;
}

bool MakeFileUnwritable(const FilePath& path) {
  NOTIMPLEMENTED();
  return true;
}

PermissionRestorer::PermissionRestorer(const FilePath& path)
    : path_(path), info_(NULL), length_(0) {
  NOTIMPLEMENTED();
}

PermissionRestorer::~PermissionRestorer() {
  NOTIMPLEMENTED();
}

}  // namespace file_util
