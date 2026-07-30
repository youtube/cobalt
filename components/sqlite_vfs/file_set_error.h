// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SQLITE_VFS_FILE_SET_ERROR_H_
#define COMPONENTS_SQLITE_VFS_FILE_SET_ERROR_H_

namespace sqlite_vfs {

enum class FileSetError {
  // A transient error occurred. A retry might succeed. Examples: disk full, too
  // many open files, temporary lock contention, shared memory resource limits,
  // or OS handle exhaustion.
  kTransient,

  // A persistent error occurred. A retry will fail. Examples: target directory
  // does not exist, lack of permissions, or backing files were deleted.
  kPermanent,

  // A persistent error occurred where the recommended recovery is to delete the
  // files and try again. Example: a directory exists where the database file
  // should be.
  kPermanentRequireDeletion,
};

}  // namespace sqlite_vfs

#endif  // COMPONENTS_SQLITE_VFS_FILE_SET_ERROR_H_
