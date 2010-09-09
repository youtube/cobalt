// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util_proxy.h"

#include "base/message_loop_proxy.h"

// TODO(jianli): Move the code from anonymous namespace to base namespace so
// that all of the base:: prefixes would be unnecessary.
namespace {

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
        set_error_code(base::PLATFORM_FILE_ERROR_INVALID_OPERATION);
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
    bool dest_path_exists = file_util::PathExists(dest_file_path_);
    if (!dest_path_exists &&
        !file_util::DirectoryExists(dest_file_path_.DirName())) {
      set_error_code(base::PLATFORM_FILE_ERROR_NOT_FOUND);
      return;
    }
    // |src_file_path| exists and is a directory.
    // |dest_file_path| exists and is a file.
    if (file_util::DirectoryExists(src_file_path_) &&
        dest_path_exists && !file_util::DirectoryExists(dest_file_path_)) {
      set_error_code(base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY);
      return;
    }
    if (file_util::ContainsPath(src_file_path_, dest_file_path_)) {
      set_error_code(base::PLATFORM_FILE_ERROR_FAILED);
      return;
    }
    if (!file_util::CopyDirectory(src_file_path_, dest_file_path_,
        true /* recursive */)) {
      if (!file_util::PathExists(src_file_path_)) {
        set_error_code(base::PLATFORM_FILE_ERROR_NOT_FOUND);
        return;
      }
      if (src_file_path_.value() == dest_file_path_.value()) {
        set_error_code(base::PLATFORM_FILE_ERROR_EXISTS);
        return;
      }
      // Something else went wrong.
      set_error_code(base::PLATFORM_FILE_ERROR_FAILED);
    }
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
    bool dest_path_exists = file_util::PathExists(dest_file_path_);
    if (!dest_path_exists &&
        !file_util::DirectoryExists(dest_file_path_.DirName())) {
      set_error_code(base::PLATFORM_FILE_ERROR_NOT_FOUND);
      return;
    }
    // |src_file_path| exists and is a directory.
    // |dest_file_path| exists and is a file.
    if (file_util::DirectoryExists(src_file_path_) &&
          dest_path_exists &&
          !file_util::DirectoryExists(dest_file_path_)) {
      set_error_code(base::PLATFORM_FILE_ERROR_EXISTS);
      return;
    }
    if (file_util::ContainsPath(src_file_path_, dest_file_path_)) {
      set_error_code(base::PLATFORM_FILE_ERROR_INVALID_OPERATION);
      return;
    }
    if (!file_util::Move(src_file_path_, dest_file_path_)) {
      if (!file_util::PathExists(src_file_path_)) {
        set_error_code(base::PLATFORM_FILE_ERROR_NOT_FOUND);
        return;
      }
      if (src_file_path_.value() == dest_file_path_.value()) {
        set_error_code(base::PLATFORM_FILE_ERROR_EXISTS);
        return;
      }
      // Something else went wrong.
      set_error_code(base::PLATFORM_FILE_ERROR_FAILED);
    }
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
      base::file_util_proxy::Entry entry;
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
  std::vector<base::file_util_proxy::Entry> entries_;
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
            char* buffer,
            int bytes_to_read,
            base::FileUtilProxy::ReadWriteCallback* callback)
      : file_(file),
        offset_(offset),
        buffer_(buffer),
        bytes_to_read_(bytes_to_read),
        callback_(callback),
        bytes_read_(0) {
  }

 protected:
  virtual void RunWork() {
    bytes_read_ = base::ReadPlatformFile(file_, offset_, buffer_,
                                         bytes_to_read_);
    if (bytes_read_ < 0)
      set_error_code(base::PLATFORM_FILE_ERROR_FAILED);
  }

  virtual void RunCallback() {
    if (callback_) {
      callback_->Run(error_code(), bytes_read_);
      delete callback_;
    }
  }

 private:
  base::PlatformFile file_;
  int64 offset_;
  char* buffer_;
  int bytes_to_read_;
  base::FileUtilProxy::ReadWriteCallback* callback_;
  int bytes_read_;
};

class RelayWrite : public MessageLoopRelay {
 public:
  RelayWrite(base::PlatformFile file,
             long long offset,
             const char* buffer,
             int bytes_to_write,
             base::FileUtilProxy::ReadWriteCallback* callback)
      : file_(file),
        offset_(offset),
        buffer_(buffer),
        bytes_to_write_(bytes_to_write),
        callback_(callback) {
  }

 protected:
  virtual void RunWork() {
    bytes_written_ = base::WritePlatformFile(file_, offset_, buffer_,
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
  const char* buffer_;
  int bytes_to_write_;
  base::FileUtilProxy::ReadWriteCallback* callback_;
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

class RelayTruncate : public RelayWithStatusCallback {
 public:
  RelayTruncate(base::PlatformFile file,
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
bool FileUtilProxy::Close(scoped_refptr<MessageLoopProxy> message_loop_proxy,
                          base::PlatformFile file_handle,
                          StatusCallback* callback) {
  return Start(FROM_HERE, message_loop_proxy,
               new RelayClose(file_handle, callback));
}

// static
bool FileUtilProxy::Delete(scoped_refptr<MessageLoopProxy> message_loop_proxy,
                           const FilePath& file_path,
                           StatusCallback* callback) {
  return Start(FROM_HERE, message_loop_proxy,
               new RelayDelete(file_path, false, callback));
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
bool FileUtilProxy::ReadDirectory(
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    const FilePath& file_path,
    ReadDirectoryCallback* callback) {
  return Start(FROM_HERE, message_loop_proxy, new RelayReadDirectory(
               file_path, callback));
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
bool FileUtilProxy::RecursiveDelete(
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    const FilePath& file_path,
    StatusCallback* callback) {
  return Start(FROM_HERE, message_loop_proxy,
               new RelayDelete(file_path, true, callback));
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
bool FileUtilProxy::Read(
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    PlatformFile file,
    int64 offset,
    char* buffer,
    int bytes_to_read,
    ReadWriteCallback* callback) {
  return Start(FROM_HERE, message_loop_proxy,
               new RelayRead(file, offset, buffer, bytes_to_read, callback));
}

// static
bool FileUtilProxy::Write(
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    PlatformFile file,
    int64 offset,
    const char* buffer,
    int bytes_to_write,
    ReadWriteCallback* callback) {
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
bool FileUtilProxy::Truncate(
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    PlatformFile file,
    long long length,
    StatusCallback* callback) {
  return Start(FROM_HERE, message_loop_proxy,
               new RelayTruncate(file, length, callback));
}

// static
bool FileUtilProxy::Flush(
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    PlatformFile file,
    StatusCallback* callback) {
  return Start(FROM_HERE, message_loop_proxy, new RelayFlush(file, callback));
}

}  // namespace base
