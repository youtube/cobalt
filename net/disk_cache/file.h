// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See net/disk_cache/disk_cache.h for the public interface of the cache.

#ifndef NET_DISK_CACHE_FILE_H_
#define NET_DISK_CACHE_FILE_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "base/platform_file.h"

class FilePath;

namespace disk_cache {

// This interface is used to support asynchronous ReadData and WriteData calls.
class FileIOCallback {
 public:
  virtual ~FileIOCallback() {}

  // Notified of the actual number of bytes read or written. This value is
  // negative if an error occurred.
  virtual void OnFileIOComplete(int bytes_copied) = 0;
};

// Simple wrapper around a file that allows asynchronous operations.
class File : public base::RefCounted<File> {
  friend class base::RefCounted<File>;
 public:
  File();
  // mixed_mode set to true enables regular synchronous operations for the file.
  explicit File(bool mixed_mode);

  // Initializes the object to use the passed in file instead of opening it with
  // the Init() call. No asynchronous operations can be performed with this
  // object.
  explicit File(base::PlatformFile file);

  // Initializes the object to point to a given file. The file must aready exist
  // on disk, and allow shared read and write.
  bool Init(const FilePath& name);

  // Returns the handle or file descriptor.
  base::PlatformFile platform_file() const;

  // Returns true if the file was opened properly.
  bool IsValid() const;

  // Performs synchronous IO.
  bool Read(void* buffer, size_t buffer_len, size_t offset);
  bool Write(const void* buffer, size_t buffer_len, size_t offset);

  // Performs asynchronous IO. callback will be called when the IO completes,
  // as an APC on the thread that queued the operation.
  bool Read(void* buffer, size_t buffer_len, size_t offset,
            FileIOCallback* callback, bool* completed);
  bool Write(const void* buffer, size_t buffer_len, size_t offset,
             FileIOCallback* callback, bool* completed);

  // Sets the file's length. The file is truncated or extended with zeros to
  // the new length.
  bool SetLength(size_t length);
  size_t GetLength();

  // Blocks until |num_pending_io| IO operations complete.
  static void WaitForPendingIO(int* num_pending_io);

 protected:
  virtual ~File();

  // Performs the actual asynchronous write. If notify is set and there is no
  // callback, the call will be re-synchronized.
  bool AsyncWrite(const void* buffer, size_t buffer_len, size_t offset,
                  FileIOCallback* callback, bool* completed);

 private:
  bool init_;
  bool mixed_;
  base::PlatformFile platform_file_;  // Regular, asynchronous IO handle.
  base::PlatformFile sync_platform_file_;  // Synchronous IO handle.

  DISALLOW_COPY_AND_ASSIGN(File);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_FILE_H_
