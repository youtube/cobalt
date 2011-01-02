// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"

#import <Foundation/Foundation.h>
#include <copyfile.h>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/string_util.h"
#include "base/threading/thread_restrictions.h"

namespace file_util {

bool GetTempDir(FilePath* path) {
  NSString* tmp = NSTemporaryDirectory();
  if (tmp == nil)
    return false;
  *path = FilePath([tmp fileSystemRepresentation]);
  return true;
}

bool GetShmemTempDir(FilePath* path) {
  return GetTempDir(path);
}

bool CopyFile(const FilePath& from_path, const FilePath& to_path) {
  base::ThreadRestrictions::AssertIOAllowed();
  return (copyfile(from_path.value().c_str(),
                   to_path.value().c_str(), NULL, COPYFILE_ALL) == 0);
}

}  // namespace
