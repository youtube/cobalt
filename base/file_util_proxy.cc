// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util_proxy.h"

#include "base/message_loop_proxy.h"

// TODO(jianli): Move the code from anonymous namespace to base namespace so
// that all of the base:: prefixes would be unnecessary.
namespace {

namespace {

// Performs common checks for move and copy.
// This also removes the destination directory if it's non-empty and all other
// checks are passed (so that the copy/move correctly overwrites the
// destination).
static base::PlatformFileError PerformCommonCheckAndPreparationForMoveAndCopy(
    const FilePath& src_file_path,
    const FilePath& dest_file_path) {
  // Exits earlier if the source path does not exist.
  if (!file_util::PathExists(src_file_path))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  // The parent of the |dest_file_path| does not exist.
  if (!file_util::DirectoryExists(dest_file_path.DirName()))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  // It is an error to try to copy/move an entry into its child.
  if (src_file_path.IsParent(dest_file_path))
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;

  // Now it is ok to return if the |dest_file_path| does not exist.
  if (!file_util::PathExists(dest_file_path))
    return base::PLATFORM_FILE_OK;

  // |src_file_path| exists and is a directory.
  // |dest_file_path| exists and is a file.
  bool src_is_directory = file_util::DirectoryExists(src_file_path);
  bool dest_is_directory = file_util::DirectoryExists(dest_file_path);
  if (src_is_directory && !dest_is_directory)
    return base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY;

  // |src_file_path| exists and is a file.
  // |dest_file_path| exists and is a directory.
  if (!src_is_directory && dest_is_directory)
    return base::PLATFORM_FILE_ERROR_NOT_A_FILE;

  // It is an error to copy/move an entry into the same path.
  if (src_file_path.value() == dest_file_path.value())
    return base::PLATFORM_FILE_ERROR_EXISTS;

  if (dest_is_directory) {
    // It is an error to copy/move an entry to a non-empty directory.
    // Otherwise the copy/move attempt must overwrite the destination, but
    // the file_util's Copy or Move method doesn't perform overwrite
    // on all platforms, so we delete the destination directory here.
    // TODO(kinuko): may be better to change the file_util::{Copy,Move}.
    if (!file_util::Delete(dest_file_path, false /* recursive */)) {
      if (!file_util::IsDirectoryEmpty(dest_file_path))
        return base::PLATFORM_FILE_ERROR_NOT_EMPTY;
      return base::PLATFORM_FILE_ERROR_FAILED;
    }
  }
  return base::PLATFORM_FILE_OK;
}

}  // anonymous namespace

class MessageLoopRelay
    : public base::RefCountedThreadSafe<MessageLoopRelay> {
 public:
  MessageLoopRelay()
      : origin_message_loop_proxy_(
            base::MessageLoopProxy::CreateForCurrentThread()),
        error_code_(base::PLATFORM_FILE_OK) {
  }

  bool Start(scoped_refptr<base::MessageLoopProxy> message_loop_proxy,
             const tracked_objects::Location& from_here) {
    return message_loop_proxy->PostTask(
        from_here,
        NewRunnableMethod(this, &MessageLoopRelay::ProcessOnTargetThread));
  }

 protected:
  friend class base::RefCountedThreadSafe<MessageLoopRelay>;
  virtual ~MessageLoopRelay() {}

  // Called to perform work on the FILE thread.
  virtual void RunWork() = 0;

  // Called to notify the callback on the origin thread.
  virtual void RunCallback() = 0;

  void set_error_code(base::PlatformFileError error_code) {
    error_code_ = error_code;
  }

  base::PlatformFileError error_code() const {
    return error_code_;
  }

 private:
  void ProcessOnTargetThread() {
    RunWork();
    origin_message_loop_proxy_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &MessageLoopRelay::RunCallback));
  }

  scoped_refptr<base::MessageLoopProxy> origin_message_loop_proxy_;
  base::PlatformFileError error_code_;
};

class RelayCreateOrOpen : public MessageLoopRelay {
 public:
  RelayCreateOrOpen(
      scoped_refptr<base::MessageLoopProxy> message_loop_proxy,
      const FilePath& file_path,
      int file_flags,
      base::FileUtilProxy::CreateOrOpenCallback* callback)
      : message_loop_proxy_(message_loop_proxy),
        file_path_(file_path),
        file_flags_(file_flags),
        callback_(callback),
        file_handle_(base::kInvalidPlatformFileValue),
        created_(false) {
    DCHECK(callback);
  }

 protected:
  virtual ~RelayCreateOrOpen() {
    if (file_handle_ != base::kInvalidPlatformFileValue)
      base::FileUtilProxy::Close(message_loop_proxy_, file_handle_, NULL);
  }

  virtual void RunWork() {
    if (!file_util::DirectoryExists(file_path_.DirName())) {
      // If its parent does not exist, should return NOT_FOUND error.
      set_error_code(base::PLATFORM_FILE_ERROR_NOT_FOUND);
      return;
    }
    base::PlatformFileError error_code = base::PLATFORM_FILE_OK;
    file_handle_ = base::CreatePlatformFile(file_path_, file_flags_,
                                            &created_, &error_code);
    set_error_code(error_code);
  }

  virtual void RunCallback() {
    callback_->Run(error_code(), base::PassPlatformFile(&file_handle_),
                   created_);
    delete callback_;
  }

 private:
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
  FilePath file_path_;
  int file_flags_;
  base::FileUtilProxy::CreateOrOpenCallback* callback_;
  base::PlatformFile file_handle_;
  bool created_;
};

class RelayCreateTemporary : public MessageLoopRelay {
 public:
  RelayCreateTemporary(
      scoped_refptr<base::MessageLoopProxy> message_loop_proxy,
      base::FileUtilProxy::CreateTemporaryCallback* callback)
      : message_loop_proxy_(message_loop_proxy),
        callback_(callback),
        file_handle_(base::kInvalidPlatformFileValue) {
    DCHECK(callback);
  }

 protected:
  virtual ~RelayCreateTemporary() {
    if (file_handle_ != base::kInvalidPlatformFileValue)
      base::FileUtilProxy::Close(message_loop_proxy_, file_handle_, NULL);
  }

  virtual void RunWork() {
    // TODO(darin): file_util should have a variant of CreateTemporaryFile
    // that returns a FilePath and a PlatformFile.
    file_util::CreateTemporaryFile(&file_path_);

    // Use a fixed set of flags that are appropriate for writing to a temporary
    // file from the IO thread using a net::FileStream.
    int file_flags =
        base::PLATFORM_FILE_CREATE_ALWAYS |
        base::PLATFORM_FILE_WRITE |
        base::PLATFORM_FILE_ASYNC |
        base::PLATFORM_FILE_TEMPORARY;
    base::PlatformFileError error_code = base::PLATFORM_FILE_OK;
    file_handle_ = base::CreatePlatformFile(file_path_, file_flags,
                                            NULL, &error_code);
    set_error_code(error_code);
  }

  virtual void RunCallback() {
    callback_->Run(error_code(), base::PassPlatformFile(&file_handle_),
                   file_path_);
    delete callback_;
  }

 private:
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
  base::FileUtilProxy::CreateTemporaryCallback* callback_;
  base::PlatformFile file_handle_;
  FilePath file_path_;
};

class RelayWithStatusCallback : public MessageLoopRelay {
 public:
  explicit RelayWithStatusCallback(
      base::FileUtilProxy::StatusCallback* callback)
      : callback_(callback) {
    // It is OK for callback to be NULL.
  }

 protected:
  virtual void RunCallback() {
    // The caller may not have been interested in the result.
    if (callback_) {
      callback_->Run(error_code());
      delete callback_;
    }
  }

 private:
  base::FileUtilProxy::StatusCallback* callback_;
};

class RelayClose : public RelayWithStatusCallback {
 public:
  RelayClose(base::PlatformFile file_handle,
             base::FileUtilProxy::StatusCallback* callback)
      : RelayWithStatusCallback(callback),
        file_handle_(file_handle) {
  }

 protected:
  virtual void RunWork() {
    if (!base::ClosePlatformFile(file_handle_))
      set_error_code(base::PLATFORM_FILE_ERROR_FAILED);
  }

 private:
  base::PlatformFile file_handle_;
};

class RelayEnsureFileExists : public MessageLoopRelay {
 public:
  RelayEnsureFileExists(
      scoped_refptr<base::MessageLoopProxy> message_loop_proxy,
      const FilePath& file_path,
      base::FileUtilProxy::EnsureFileExistsCallback* callback)
      : message_loop_proxy_(message_loop_proxy),
        file_path_(file_path),
        callback_(callback),
        created_(false) {
    DCHECK(callback);
  }

 protected:
  virtual void RunWork() {
    if (!file_util::DirectoryExists(file_path_.DirName())) {
      // If its parent does not exist, should return NOT_FOUND error.
      set_error_code(base::PLATFORM_FILE_ERROR_NOT_FOUND);
      return;
    }
    base::PlatformFileError error_code = base::PLATFORM_FILE_OK;
    // Tries to create the |file_path_| exclusively.  This should fail
    // with PLATFORM_FILE_ERROR_EXISTS if the path already exists.
    base::PlatformFile handle = base::CreatePlatformFile(
        file_path_,
        base::PLATFORM_FILE_CREATE | base::PLATFORM_FILE_READ,
        &created_, &error_code);
    if (error_code == base::PLATFORM_FILE_ERROR_EXISTS) {
      // Make sure created_ is false.
      created_ = false;
      error_code = base::PLATFORM_FILE_OK;
    }
    if (handle != base::kInvalidPlatformFileValue)
      base::ClosePlatformFile(handle);
    set_error_code(error_code);
  }

  virtual void RunCallback() {
    callback_->Run(error_code(), created_);
    delete callback_;
  }

 private:
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
  FilePath file_path_;
  base::FileUtilProxy::EnsureFileExistsCallback* callback_;
  bool created_;
};

class RelayDelete : public RelayWithStatusCallback {
 public:
  RelayDelete(const FilePath& file_path,
              bool recursive,
              base::FileUtilProxy::StatusCallback* callback)
      : RelayWithStatusCallback(callback),
        file_path_(file_path),
        recursive_(recursive) {
  }

 protected:
  virtual void RunWork() {
    if (!file_util::PathExists(file_path_)) {
      set_error_code(base::PLATFORM_FILE_ERROR_NOT_FOUND);
      return;
    }
    if (!file_util::Delete(file_path_, recursive_)) {
      if (!recursive_ && !file_util::IsDirectoryEmpty(file_path_)) {
        set_error_code(base::PLATFORM_FILE_ERROR_NOT_EMPTY);
        return;
      }
      set_error_code(base::PLATFORM_FILE_ERROR_FAILED);
    }
  }

 private:
  FilePath file_path_;
  bool recursive_;
};

class RelayCopy : public RelayWithStatusCallback {
 public:
  RelayCopy(const FilePath& src_file_path,
            const FilePath& dest_file_path,
            base::FileUtilProxy::StatusCallback* callback)
      : RelayWithStatusCallback(callback),
        src_file_path_(src_file_path),
        dest_file_path_(dest_file_path) {
  }

 protected:
  virtual void RunWork() {
    set_error_code(PerformCommonCheckAndPreparationForMoveAndCopy(
        src_file_path_, dest_file_path_));
    if (error_code() != base::PLATFORM_FILE_OK)
      return;
    if (!file_util::CopyDirectory(src_file_path_, dest_file_path_,
        true /* recursive */))
      set_error_code(base::PLATFORM_FILE_ERROR_FAILED);
  }

 private:
  FilePath src_file_path_;
  FilePath dest_file_path_;
};

class RelayMove : public RelayWithStatusCallback {
 public:
  RelayMove(const FilePath& src_file_path,
            const FilePath& dest_file_path,
            base::FileUtilProxy::StatusCallback* callback)
      : RelayWithStatusCallback(callback),
        src_file_path_(src_file_path),
        dest_file_path_(dest_file_path) {
  }

 protected:
  virtual void RunWork() {
    set_error_code(PerformCommonCheckAndPreparationForMoveAndCopy(
        src_file_path_, dest_file_path_));
    if (error_code() != base::PLATFORM_FILE_OK)
      return;
    if (!file_util::Move(src_file_path_, dest_file_path_))
      set_error_code(base::PLATFORM_FILE_ERROR_FAILED);
  }

 private:
  FilePath src_file_path_;
  FilePath dest_file_path_;
};

class RelayCreateDirectory : public RelayWithStatusCallback {
 public:
  RelayCreateDirectory(
      const FilePath& file_path,
      bool exclusive,
      bool recursive,
      base::FileUtilProxy::StatusCallback* callback)
      : RelayWithStatusCallback(callback),
        file_path_(file_path),
        exclusive_(exclusive),
        recursive_(recursive) {
  }

 protected:
  virtual void RunWork() {
    bool path_exists = file_util::PathExists(file_path_);
    // If parent dir of file doesn't exist.
    if (!recursive_ && !file_util::PathExists(file_path_.DirName())) {
      set_error_code(base::PLATFORM_FILE_ERROR_NOT_FOUND);
      return;
    }
    if (exclusive_ && path_exists) {
      set_error_code(base::PLATFORM_FILE_ERROR_EXISTS);
      return;
    }
    // If file exists at the path.
    if (path_exists && !file_util::DirectoryExists(file_path_)) {
      set_error_code(base::PLATFORM_FILE_ERROR_EXISTS);
      return;
    }
    if (!file_util::CreateDirectory(file_path_))
      set_error_code(base::PLATFORM_FILE_ERROR_FAILED);
  }

 private:
  FilePath file_path_;
  bool exclusive_;
  bool recursive_;
};

class RelayReadDirectory : public MessageLoopRelay {
 public:
  RelayReadDirectory(const FilePath& file_path,
      base::FileUtilProxy::ReadDirectoryCallback* callback)
      : callback_(callback), file_path_(file_path) {
    DCHECK(callback);
  }

 protected:
  virtual void RunWork() {
    // TODO(kkanetkar): Implement directory read in multiple chunks.
    if (!file_util::DirectoryExists(file_path_)) {
      set_error_code(base::PLATFORM_FILE_ERROR_NOT_FOUND);
      return;
    }

    file_util::FileEnumerator file_enum(
        file_path_, false, static_cast<file_util::FileEnumerator::FILE_TYPE>(
        file_util::FileEnumerator::FILES |
        file_util::FileEnumerator::DIRECTORIES));
    FilePath current;
    while (!(current = file_enum.Next()).empty()) {
      base::FileUtilProxy::Entry entry;
      file_util::FileEnumerator::FindInfo info;
      file_enum.GetFindInfo(&info);
      entry.is_directory = file_enum.IsDirectory(info);
      // This will just give the entry's name instead of entire path
      // if we use current.value().
      entry.name = file_util::FileEnumerator::GetFilename(info).value();
      entries_.push_back(entry);
    }
  }

  virtual void RunCallback() {
    callback_->Run(error_code(), entries_);
    delete callback_;
  }

 private:
  base::FileUtilProxy::ReadDirectoryCallback* callback_;
  FilePath file_path_;
  std::vector<base::FileUtilProxy::Entry> entries_;
};

class RelayGetFileInfo : public MessageLoopRelay {
 public:
  RelayGetFileInfo(const FilePath& file_path,
                   base::FileUtilProxy::GetFileInfoCallback* callback)
      : callback_(callback),
        file_path_(file_path) {
    DCHECK(callback);
  }

 protected:
  virtual void RunWork() {
    if (!file_util::PathExists(file_path_)) {
      set_error_code(base::PLATFORM_FILE_ERROR_NOT_FOUND);
      return;
    }
    if (!file_util::GetFileInfo(file_path_, &file_info_))
      set_error_code(base::PLATFORM_FILE_ERROR_FAILED);
  }

  virtual void RunCallback() {
    callback_->Run(error_code(), file_info_);
    delete callback_;
  }

 private:
  base::FileUtilProxy::GetFileInfoCallback* callback_;
  FilePath file_path_;
  base::PlatformFileInfo file_info_;
};

class RelayGetFileInfoFromPlatformFile : public MessageLoopRelay {
 public:
  RelayGetFileInfoFromPlatformFile(
      base::PlatformFile file,
      base::FileUtilProxy::GetFileInfoCallback* callback)
      : callback_(callback),
        file_(file) {
    DCHECK(callback);
  }

 protected:
  virtual void RunWork() {
    if (!base::GetPlatformFileInfo(file_, &file_info_))
      set_error_code(base::PLATFORM_FILE_ERROR_FAILED);
  }

  virtual void RunCallback() {
    callback_->Run(error_code(), file_info_);
    delete callback_;
  }

 private:
  base::FileUtilProxy::GetFileInfoCallback* callback_;
  base::PlatformFile file_;
  base::PlatformFileInfo file_info_;
};

class RelayRead : public MessageLoopRelay {
 public:
  RelayRead(base::PlatformFile file,
            int64 offset,
            int bytes_to_read,
            base::FileUtilProxy::ReadCallback* callback)
      : file_(file),
        offset_(offset),
        buffer_(new char[bytes_to_read]),
        bytes_to_read_(bytes_to_read),
        callback_(callback),
        bytes_read_(0) {
  }

 protected:
  virtual void RunWork() {
    bytes_read_ = base::ReadPlatformFile(file_, offset_, buffer_.get(),
                                         bytes_to_read_);
    if (bytes_read_ < 0)
      set_error_code(base::PLATFORM_FILE_ERROR_FAILED);
  }

  virtual void RunCallback() {
    if (callback_) {
      callback_->Run(error_code(), buffer_.get(), bytes_read_);
      delete callback_;
    }
  }

 private:
  base::PlatformFile file_;
  int64 offset_;
  scoped_array<char> buffer_;
  int bytes_to_read_;
  base::FileUtilProxy::ReadCallback* callback_;
  int bytes_read_;
};

class RelayWrite : public MessageLoopRelay {
 public:
  RelayWrite(base::PlatformFile file,
             int64 offset,
             const char* buffer,
             int bytes_to_write,
             base::FileUtilProxy::WriteCallback* callback)
      : file_(file),
        offset_(offset),
        buffer_(new char[bytes_to_write]),
        bytes_to_write_(bytes_to_write),
        callback_(callback),
        bytes_written_(0) {
    memcpy(buffer_.get(), buffer, bytes_to_write);
  }

 protected:
  virtual void RunWork() {
    bytes_written_ = base::WritePlatformFile(file_, offset_, buffer_.get(),
                                             bytes_to_write_);
    if (bytes_written_ < 0)
      set_error_code(base::PLATFORM_FILE_ERROR_FAILED);
  }

  virtual void RunCallback() {
    if (callback_) {
      callback_->Run(error_code(), bytes_written_);
      delete callback_;
    }
  }

 private:
  base::PlatformFile file_;
  int64 offset_;
  scoped_array<char> buffer_;
  int bytes_to_write_;
  base::FileUtilProxy::WriteCallback* callback_;
  int bytes_written_;
};

class RelayTouch : public RelayWithStatusCallback {
 public:
  RelayTouch(base::PlatformFile file,
             const base::Time& last_access_time,
             const base::Time& last_modified_time,
             base::FileUtilProxy::StatusCallback* callback)
      : RelayWithStatusCallback(callback),
        file_(file),
        last_access_time_(last_access_time),
        last_modified_time_(last_modified_time) {
  }

 protected:
  virtual void RunWork() {
    if (!base::TouchPlatformFile(file_, last_access_time_, last_modified_time_))
      set_error_code(base::PLATFORM_FILE_ERROR_FAILED);
  }

 private:
  base::PlatformFile file_;
  base::Time last_access_time_;
  base::Time last_modified_time_;
};

class RelayTouchFilePath : public RelayWithStatusCallback {
 public:
  RelayTouchFilePath(const FilePath& file_path,
                     const base::Time& last_access_time,
                     const base::Time& last_modified_time,
                     base::FileUtilProxy::StatusCallback* callback)
      : RelayWithStatusCallback(callback),
        file_path_(file_path),
        last_access_time_(last_access_time),
        last_modified_time_(last_modified_time) {
  }

 protected:
  virtual void RunWork() {
    if (!file_util::TouchFile(
            file_path_, last_access_time_, last_modified_time_))
      set_error_code(base::PLATFORM_FILE_ERROR_FAILED);
  }

 private:
  FilePath file_path_;
  base::Time last_access_time_;
  base::Time last_modified_time_;
};

class RelayTruncatePlatformFile : public RelayWithStatusCallback {
 public:
  RelayTruncatePlatformFile(base::PlatformFile file,
                            int64 length,
                            base::FileUtilProxy::StatusCallback* callback)
      : RelayWithStatusCallback(callback),
        file_(file),
        length_(length) {
  }

 protected:
  virtual void RunWork() {
    if (!base::TruncatePlatformFile(file_, length_))
      set_error_code(base::PLATFORM_FILE_ERROR_FAILED);
  }

 private:
  base::PlatformFile file_;
  int64 length_;
};

class RelayTruncate : public RelayWithStatusCallback {
 public:
  RelayTruncate(const FilePath& path,
                int64 length,
                base::FileUtilProxy::StatusCallback* callback)
      : RelayWithStatusCallback(callback),
        path_(path),
        length_(length) {
  }

 protected:
  virtual void RunWork() {
    base::PlatformFileError error_code(base::PLATFORM_FILE_ERROR_FAILED);
    base::PlatformFile file =
        base::CreatePlatformFile(
            path_,
            base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_WRITE,
            NULL,
            &error_code);
    if (error_code != base::PLATFORM_FILE_OK) {
      set_error_code(error_code);
      return;
    }
    if (!base::TruncatePlatformFile(file, length_))
      set_error_code(base::PLATFORM_FILE_ERROR_FAILED);
    base::ClosePlatformFile(file);
  }

 private:
  FilePath path_;
  int64 length_;
};

class RelayFlush : public RelayWithStatusCallback {
 public:
  RelayFlush(base::PlatformFile file,
             base::FileUtilProxy::StatusCallback* callback)
      : RelayWithStatusCallback(callback),
        file_(file) {
  }

 protected:
  virtual void RunWork() {
    if (!base::FlushPlatformFile(file_))
      set_error_code(base::PLATFORM_FILE_ERROR_FAILED);
  }

 private:
  base::PlatformFile file_;
};

bool Start(const tracked_objects::Location& from_here,
           scoped_refptr<base::MessageLoopProxy> message_loop_proxy,
           scoped_refptr<MessageLoopRelay> relay) {
  return relay->Start(message_loop_proxy, from_here);
}

}  // namespace

namespace base {

// static
bool FileUtilProxy::CreateOrOpen(
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    const FilePath& file_path, int file_flags,
    CreateOrOpenCallback* callback) {
  return Start(FROM_HERE, message_loop_proxy, new RelayCreateOrOpen(
      message_loop_proxy, file_path, file_flags, callback));
}

// static
bool FileUtilProxy::CreateTemporary(
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    CreateTemporaryCallback* callback) {
  return Start(FROM_HERE, message_loop_proxy,
               new RelayCreateTemporary(message_loop_proxy, callback));
}

// static
bool FileUtilProxy::Close(scoped_refptr<MessageLoopProxy> message_loop_proxy,
                          base::PlatformFile file_handle,
                          StatusCallback* callback) {
  return Start(FROM_HERE, message_loop_proxy,
               new RelayClose(file_handle, callback));
}

// static
bool FileUtilProxy::EnsureFileExists(
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    const FilePath& file_path,
    EnsureFileExistsCallback* callback) {
  return Start(FROM_HERE, message_loop_proxy, new RelayEnsureFileExists(
      message_loop_proxy, file_path, callback));
}

// Retrieves the information about a file. It is invalid to pass NULL for the
// callback.
bool FileUtilProxy::GetFileInfo(
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    const FilePath& file_path,
    GetFileInfoCallback* callback) {
  return Start(FROM_HERE, message_loop_proxy, new RelayGetFileInfo(
               file_path, callback));
}

// static
bool FileUtilProxy::GetFileInfoFromPlatformFile(
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    PlatformFile file,
    GetFileInfoCallback* callback) {
  return Start(FROM_HERE, message_loop_proxy,
               new RelayGetFileInfoFromPlatformFile(file, callback));
}

// static
bool FileUtilProxy::ReadDirectory(
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    const FilePath& file_path,
    ReadDirectoryCallback* callback) {
  return Start(FROM_HERE, message_loop_proxy, new RelayReadDirectory(
               file_path, callback));
}

// static
bool FileUtilProxy::CreateDirectory(
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    const FilePath& file_path,
    bool exclusive,
    bool recursive,
    StatusCallback* callback) {
  return Start(FROM_HERE, message_loop_proxy, new RelayCreateDirectory(
      file_path, exclusive, recursive, callback));
}

// static
bool FileUtilProxy::Copy(scoped_refptr<MessageLoopProxy> message_loop_proxy,
                         const FilePath& src_file_path,
                         const FilePath& dest_file_path,
                         StatusCallback* callback) {
  return Start(FROM_HERE, message_loop_proxy,
               new RelayCopy(src_file_path, dest_file_path, callback));
}

// static
bool FileUtilProxy::Move(scoped_refptr<MessageLoopProxy> message_loop_proxy,
                         const FilePath& src_file_path,
                         const FilePath& dest_file_path,
                         StatusCallback* callback) {
  return Start(FROM_HERE, message_loop_proxy,
               new RelayMove(src_file_path, dest_file_path, callback));
}

// static
bool FileUtilProxy::Delete(scoped_refptr<MessageLoopProxy> message_loop_proxy,
                           const FilePath& file_path,
                           bool recursive,
                           StatusCallback* callback) {
  return Start(FROM_HERE, message_loop_proxy,
               new RelayDelete(file_path, recursive, callback));
}

// static
bool FileUtilProxy::RecursiveDelete(
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    const FilePath& file_path,
    StatusCallback* callback) {
  return Start(FROM_HERE, message_loop_proxy,
               new RelayDelete(file_path, true, callback));
}

// static
bool FileUtilProxy::Read(
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    PlatformFile file,
    int64 offset,
    int bytes_to_read,
    ReadCallback* callback) {
  return Start(FROM_HERE, message_loop_proxy,
               new RelayRead(file, offset, bytes_to_read, callback));
}

// static
bool FileUtilProxy::Write(
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    PlatformFile file,
    int64 offset,
    const char* buffer,
    int bytes_to_write,
    WriteCallback* callback) {
  return Start(FROM_HERE, message_loop_proxy,
               new RelayWrite(file, offset, buffer, bytes_to_write, callback));
}

// static
bool FileUtilProxy::Touch(
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    PlatformFile file,
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    StatusCallback* callback) {
  return Start(FROM_HERE, message_loop_proxy,
               new RelayTouch(file, last_access_time, last_modified_time,
                              callback));
}

// static
bool FileUtilProxy::Touch(
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    const FilePath& file_path,
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    StatusCallback* callback) {
  return Start(FROM_HERE, message_loop_proxy,
               new RelayTouchFilePath(file_path, last_access_time,
                                      last_modified_time, callback));
}

// static
bool FileUtilProxy::Truncate(
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    PlatformFile file,
    int64 length,
    StatusCallback* callback) {
  return Start(FROM_HERE, message_loop_proxy,
               new RelayTruncatePlatformFile(file, length, callback));
}

// static
bool FileUtilProxy::Truncate(
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    const FilePath& path,
    int64 length,
    StatusCallback* callback) {
  return Start(FROM_HERE, message_loop_proxy,
               new RelayTruncate(path, length, callback));
}

// static
bool FileUtilProxy::Flush(
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    PlatformFile file,
    StatusCallback* callback) {
  return Start(FROM_HERE, message_loop_proxy, new RelayFlush(file, callback));
}

}  // namespace base
