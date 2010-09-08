// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FILE_UTIL_PROXY_H_
#define BASE_FILE_UTIL_PROXY_H_

#include <vector>

#include "base/callback.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/platform_file.h"
#include "base/ref_counted.h"
#include "base/tracked_objects.h"

namespace base {

namespace file_util_proxy {

// Holds metadata for file or directory entry.
struct Entry {
  FilePath::StringType name;
  bool is_directory;
};

}  // namespace file_util_proxy

class MessageLoopProxy;
class Time;

// This class provides asynchronous access to common file routines.
class FileUtilProxy {
 public:
  // This callback is used by methods that report only an error code.  It is
  // valid to pass NULL as the callback parameter to any function that takes a
  // StatusCallback, in which case the operation will complete silently.
  typedef Callback1<base::PlatformFileError /* error code */
                    >::Type StatusCallback;

  // Creates or opens a file with the given flags.  It is invalid to pass NULL
  // for the callback.
  typedef Callback3<base::PlatformFileError /* error code */,
                    base::PassPlatformFile,
                    bool /* created */>::Type CreateOrOpenCallback;
  static bool CreateOrOpen(scoped_refptr<MessageLoopProxy> message_loop_proxy,
                           const FilePath& file_path,
                           int file_flags,
                           CreateOrOpenCallback* callback);

  // Creates a temporary file for writing.  The path and an open file handle
  // are returned.  It is invalid to pass NULL for the callback.
  typedef Callback3<base::PlatformFileError /* error code */,
                    base::PassPlatformFile,
                    FilePath>::Type CreateTemporaryCallback;
  static bool CreateTemporary(
      scoped_refptr<MessageLoopProxy> message_loop_proxy,
      CreateTemporaryCallback* callback);

  // Close the given file handle.
  static bool Close(scoped_refptr<MessageLoopProxy> message_loop_proxy,
                    base::PlatformFile,
                    StatusCallback* callback);

  // Retrieves the information about a file. It is invalid to pass NULL for the
  // callback.
  typedef Callback2<base::PlatformFileError /* error code */,
                    const base::PlatformFileInfo& /* file_info */
                    >::Type GetFileInfoCallback;
  static bool GetFileInfo(
      scoped_refptr<MessageLoopProxy> message_loop_proxy,
      const FilePath& file_path,
      GetFileInfoCallback* callback);

  typedef Callback2<base::PlatformFileError /* error code */,
      const std::vector<base::file_util_proxy::Entry>&
       >::Type ReadDirectoryCallback;
  static bool ReadDirectory(scoped_refptr<MessageLoopProxy> message_loop_proxy,
                            const FilePath& file_path,
                            ReadDirectoryCallback* callback);

  // Copies a file or a directory from |src_file_path| to |dest_file_path|
  // Error cases:
  // If destination file doesn't exist or destination's parent
  // doesn't exists.
  // If source dir exists but destination path is an existing file.
  // If source is a parent of destination.
  // If source doesn't exists.
  static bool Copy(scoped_refptr<MessageLoopProxy> message_loop_proxy,
                   const FilePath& src_file_path,
                   const FilePath& dest_file_path,
                   StatusCallback* callback);

  // Creates directory at given path. It's an error to create
  // if |exclusive| is true and dir already exists.
  static bool CreateDirectory(
      scoped_refptr<MessageLoopProxy> message_loop_proxy,
      const FilePath& file_path,
      bool exclusive,
      bool recursive,
      StatusCallback* callback);

  // Deletes a file or empty directory.
  static bool Delete(scoped_refptr<MessageLoopProxy> message_loop_proxy,
                     const FilePath& file_path,
                     StatusCallback* callback);

  // Moves a file or a directory from src_file_path to dest_file_path.
  // Error cases are similar to Copy method's error cases.
  static bool Move(
      scoped_refptr<MessageLoopProxy> message_loop_proxy,
      const FilePath& src_file_path,
      const FilePath& dest_file_path,
      StatusCallback* callback);

  // Deletes a directory and all of its contents.
  static bool RecursiveDelete(
      scoped_refptr<MessageLoopProxy> message_loop_proxy,
      const FilePath& file_path,
      StatusCallback* callback);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(FileUtilProxy);
};

}  // namespace base

#endif  // BASE_FILE_UTIL_PROXY_H_
