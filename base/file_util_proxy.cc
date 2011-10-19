// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util_proxy.h"

#include "base/bind.h"
#include "base/message_loop_proxy.h"

namespace base {

namespace {

class MessageLoopRelay
    : public RefCountedThreadSafe<MessageLoopRelay> {
 public:
  MessageLoopRelay()
      : origin_message_loop_proxy_(
            MessageLoopProxy::current()),
        error_code_(PLATFORM_FILE_OK) {
  }

  bool Start(scoped_refptr<MessageLoopProxy> message_loop_proxy,
             const tracked_objects::Location& from_here) {
    return message_loop_proxy->PostTask(
        from_here, Bind(&MessageLoopRelay::ProcessOnTargetThread, this));
  }

 protected:
  friend class RefCountedThreadSafe<MessageLoopRelay>;
  virtual ~MessageLoopRelay() {}

  // Called to perform work on the FILE thread.
  virtual void RunWork() = 0;

  // Called to notify the callback on the origin thread.
  virtual void RunCallback() = 0;

  void set_error_code(PlatformFileError error_code) {
    error_code_ = error_code;
  }

  PlatformFileError error_code() const {
    return error_code_;
  }

 private:
  void ProcessOnTargetThread() {
    RunWork();
    origin_message_loop_proxy_->PostTask(
        FROM_HERE, Bind(&MessageLoopRelay::RunCallback, this));
  }

  scoped_refptr<MessageLoopProxy> origin_message_loop_proxy_;
  PlatformFileError error_code_;
};

class RelayCreateOrOpen : public MessageLoopRelay {
 public:
  RelayCreateOrOpen(
      scoped_refptr<MessageLoopProxy> message_loop_proxy,
      const FilePath& file_path,
      int file_flags,
      const FileUtilProxy::CreateOrOpenCallback& callback)
      : message_loop_proxy_(message_loop_proxy),
        file_path_(file_path),
        file_flags_(file_flags),
        callback_(callback),
        file_handle_(kInvalidPlatformFileValue),
        created_(false) {
    DCHECK_EQ(false, callback.is_null());
  }

 protected:
  virtual ~RelayCreateOrOpen() {
    if (file_handle_ != kInvalidPlatformFileValue)
      FileUtilProxy::Close(message_loop_proxy_, file_handle_,
                                 FileUtilProxy::StatusCallback());
  }

  virtual void RunWork() {
    if (!file_util::DirectoryExists(file_path_.DirName())) {
      // If its parent does not exist, should return NOT_FOUND error.
      set_error_code(PLATFORM_FILE_ERROR_NOT_FOUND);
      return;
    }
    PlatformFileError error_code = PLATFORM_FILE_OK;
    file_handle_ = CreatePlatformFile(file_path_, file_flags_,
                                            &created_, &error_code);
    set_error_code(error_code);
  }

  virtual void RunCallback() {
    callback_.Run(error_code(), PassPlatformFile(&file_handle_),
                  created_);
  }

 private:
  scoped_refptr<MessageLoopProxy> message_loop_proxy_;
  FilePath file_path_;
  int file_flags_;
  FileUtilProxy::CreateOrOpenCallback callback_;
  PlatformFile file_handle_;
  bool created_;
};

class RelayCreateTemporary : public MessageLoopRelay {
 public:
  RelayCreateTemporary(
      scoped_refptr<MessageLoopProxy> message_loop_proxy,
      int additional_file_flags,
      const FileUtilProxy::CreateTemporaryCallback& callback)
      : message_loop_proxy_(message_loop_proxy),
        additional_file_flags_(additional_file_flags),
        callback_(callback),
        file_handle_(kInvalidPlatformFileValue) {
    DCHECK_EQ(false, callback.is_null());
  }

 protected:
  virtual ~RelayCreateTemporary() {
    if (file_handle_ != kInvalidPlatformFileValue)
      FileUtilProxy::Close(message_loop_proxy_, file_handle_,
                                 FileUtilProxy::StatusCallback());
  }

  virtual void RunWork() {
    // TODO(darin): file_util should have a variant of CreateTemporaryFile
    // that returns a FilePath and a PlatformFile.
    file_util::CreateTemporaryFile(&file_path_);

    int file_flags =
        PLATFORM_FILE_WRITE |
        PLATFORM_FILE_TEMPORARY |
        PLATFORM_FILE_CREATE_ALWAYS |
        additional_file_flags_;

    PlatformFileError error_code = PLATFORM_FILE_OK;
    file_handle_ = CreatePlatformFile(file_path_, file_flags,
                                            NULL, &error_code);
    set_error_code(error_code);
  }

  virtual void RunCallback() {
    callback_.Run(error_code(), PassPlatformFile(&file_handle_),
                  file_path_);
  }

 private:
  scoped_refptr<MessageLoopProxy> message_loop_proxy_;
  int additional_file_flags_;
  FileUtilProxy::CreateTemporaryCallback callback_;
  PlatformFile file_handle_;
  FilePath file_path_;
};

class RelayWithStatusCallback : public MessageLoopRelay {
 public:
  explicit RelayWithStatusCallback(
      const FileUtilProxy::StatusCallback& callback)
      : callback_(callback) {
    // It is OK for callback to be NULL.
  }

 protected:
  virtual void RunCallback() {
    // The caller may not have been interested in the result.
    if (!callback_.is_null())
      callback_.Run(error_code());
  }

 private:
  FileUtilProxy::StatusCallback callback_;
};

class RelayClose : public RelayWithStatusCallback {
 public:
  RelayClose(PlatformFile file_handle,
             const FileUtilProxy::StatusCallback& callback)
      : RelayWithStatusCallback(callback),
        file_handle_(file_handle) {
  }

 protected:
  virtual void RunWork() {
    if (!ClosePlatformFile(file_handle_))
      set_error_code(PLATFORM_FILE_ERROR_FAILED);
  }

 private:
  PlatformFile file_handle_;
};

class RelayDelete : public RelayWithStatusCallback {
 public:
  RelayDelete(const FilePath& file_path,
              bool recursive,
              const FileUtilProxy::StatusCallback& callback)
      : RelayWithStatusCallback(callback),
        file_path_(file_path),
        recursive_(recursive) {
  }

 protected:
  virtual void RunWork() {
    if (!file_util::PathExists(file_path_)) {
      set_error_code(PLATFORM_FILE_ERROR_NOT_FOUND);
      return;
    }
    if (!file_util::Delete(file_path_, recursive_)) {
      if (!recursive_ && !file_util::IsDirectoryEmpty(file_path_)) {
        set_error_code(PLATFORM_FILE_ERROR_NOT_EMPTY);
        return;
      }
      set_error_code(PLATFORM_FILE_ERROR_FAILED);
    }
  }

 private:
  FilePath file_path_;
  bool recursive_;
};

class RelayGetFileInfo : public MessageLoopRelay {
 public:
  RelayGetFileInfo(const FilePath& file_path,
                   const FileUtilProxy::GetFileInfoCallback& callback)
      : callback_(callback),
        file_path_(file_path) {
    DCHECK_EQ(false, callback.is_null());
  }

 protected:
  virtual void RunWork() {
    if (!file_util::PathExists(file_path_)) {
      set_error_code(PLATFORM_FILE_ERROR_NOT_FOUND);
      return;
    }
    if (!file_util::GetFileInfo(file_path_, &file_info_))
      set_error_code(PLATFORM_FILE_ERROR_FAILED);
  }

  virtual void RunCallback() {
    callback_.Run(error_code(), file_info_);
  }

 private:
  FileUtilProxy::GetFileInfoCallback callback_;
  FilePath file_path_;
  PlatformFileInfo file_info_;
};

class RelayGetFileInfoFromPlatformFile : public MessageLoopRelay {
 public:
  RelayGetFileInfoFromPlatformFile(
      PlatformFile file,
      const FileUtilProxy::GetFileInfoCallback& callback)
      : callback_(callback),
        file_(file) {
    DCHECK_EQ(false, callback.is_null());
  }

 protected:
  virtual void RunWork() {
    if (!GetPlatformFileInfo(file_, &file_info_))
      set_error_code(PLATFORM_FILE_ERROR_FAILED);
  }

  virtual void RunCallback() {
    callback_.Run(error_code(), file_info_);
  }

 private:
  FileUtilProxy::GetFileInfoCallback callback_;
  PlatformFile file_;
  PlatformFileInfo file_info_;
};

class RelayRead : public MessageLoopRelay {
 public:
  RelayRead(PlatformFile file,
            int64 offset,
            int bytes_to_read,
            const FileUtilProxy::ReadCallback& callback)
      : file_(file),
        offset_(offset),
        buffer_(new char[bytes_to_read]),
        bytes_to_read_(bytes_to_read),
        callback_(callback),
        bytes_read_(0) {
  }

 protected:
  virtual void RunWork() {
    bytes_read_ = ReadPlatformFile(file_, offset_, buffer_.get(),
                                   bytes_to_read_);
    if (bytes_read_ < 0)
      set_error_code(PLATFORM_FILE_ERROR_FAILED);
  }

  virtual void RunCallback() {
    if (!callback_.is_null())
      callback_.Run(error_code(), buffer_.get(), bytes_read_);
  }

 private:
  PlatformFile file_;
  int64 offset_;
  scoped_array<char> buffer_;
  int bytes_to_read_;
  FileUtilProxy::ReadCallback callback_;
  int bytes_read_;
};

class RelayWrite : public MessageLoopRelay {
 public:
  RelayWrite(PlatformFile file,
             int64 offset,
             const char* buffer,
             int bytes_to_write,
             const FileUtilProxy::WriteCallback& callback)
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
    bytes_written_ = WritePlatformFile(file_, offset_, buffer_.get(),
                                             bytes_to_write_);
    if (bytes_written_ < 0)
      set_error_code(PLATFORM_FILE_ERROR_FAILED);
  }

  virtual void RunCallback() {
    if (!callback_.is_null())
      callback_.Run(error_code(), bytes_written_);
  }

 private:
  PlatformFile file_;
  int64 offset_;
  scoped_array<char> buffer_;
  int bytes_to_write_;
  FileUtilProxy::WriteCallback callback_;
  int bytes_written_;
};

class RelayTouch : public RelayWithStatusCallback {
 public:
  RelayTouch(PlatformFile file,
             const Time& last_access_time,
             const Time& last_modified_time,
             const FileUtilProxy::StatusCallback& callback)
      : RelayWithStatusCallback(callback),
        file_(file),
        last_access_time_(last_access_time),
        last_modified_time_(last_modified_time) {
  }

 protected:
  virtual void RunWork() {
    if (!TouchPlatformFile(file_, last_access_time_, last_modified_time_))
      set_error_code(PLATFORM_FILE_ERROR_FAILED);
  }

 private:
  PlatformFile file_;
  Time last_access_time_;
  Time last_modified_time_;
};

class RelayTouchFilePath : public RelayWithStatusCallback {
 public:
  RelayTouchFilePath(const FilePath& file_path,
                     const Time& last_access_time,
                     const Time& last_modified_time,
                     const FileUtilProxy::StatusCallback& callback)
      : RelayWithStatusCallback(callback),
        file_path_(file_path),
        last_access_time_(last_access_time),
        last_modified_time_(last_modified_time) {
  }

 protected:
  virtual void RunWork() {
    if (!file_util::TouchFile(
            file_path_, last_access_time_, last_modified_time_))
      set_error_code(PLATFORM_FILE_ERROR_FAILED);
  }

 private:
  FilePath file_path_;
  Time last_access_time_;
  Time last_modified_time_;
};

class RelayTruncatePlatformFile : public RelayWithStatusCallback {
 public:
  RelayTruncatePlatformFile(PlatformFile file,
                            int64 length,
                            const FileUtilProxy::StatusCallback& callback)
      : RelayWithStatusCallback(callback),
        file_(file),
        length_(length) {
  }

 protected:
  virtual void RunWork() {
    if (!TruncatePlatformFile(file_, length_))
      set_error_code(PLATFORM_FILE_ERROR_FAILED);
  }

 private:
  PlatformFile file_;
  int64 length_;
};

class RelayTruncate : public RelayWithStatusCallback {
 public:
  RelayTruncate(const FilePath& path,
                int64 length,
                const FileUtilProxy::StatusCallback& callback)
      : RelayWithStatusCallback(callback),
        path_(path),
        length_(length) {
  }

 protected:
  virtual void RunWork() {
    PlatformFileError error_code(PLATFORM_FILE_ERROR_FAILED);
    PlatformFile file =
        CreatePlatformFile(
            path_,
            PLATFORM_FILE_OPEN | PLATFORM_FILE_WRITE,
            NULL,
            &error_code);
    if (error_code != PLATFORM_FILE_OK) {
      set_error_code(error_code);
      return;
    }
    if (!TruncatePlatformFile(file, length_))
      set_error_code(PLATFORM_FILE_ERROR_FAILED);
    ClosePlatformFile(file);
  }

 private:
  FilePath path_;
  int64 length_;
};

class RelayFlush : public RelayWithStatusCallback {
 public:
  RelayFlush(PlatformFile file,
             const FileUtilProxy::StatusCallback& callback)
      : RelayWithStatusCallback(callback),
        file_(file) {
  }

 protected:
  virtual void RunWork() {
    if (!FlushPlatformFile(file_))
      set_error_code(PLATFORM_FILE_ERROR_FAILED);
  }

 private:
  PlatformFile file_;
};

bool Start(const tracked_objects::Location& from_here,
           scoped_refptr<MessageLoopProxy> message_loop_proxy,
           scoped_refptr<MessageLoopRelay> relay) {
  return relay->Start(message_loop_proxy, from_here);
}

}  // namespace

// static
bool FileUtilProxy::CreateOrOpen(
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    const FilePath& file_path, int file_flags,
    const CreateOrOpenCallback& callback) {
  return Start(FROM_HERE, message_loop_proxy, new RelayCreateOrOpen(
      message_loop_proxy, file_path, file_flags, callback));
}

// static
bool FileUtilProxy::CreateTemporary(
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    int additional_file_flags,
    const CreateTemporaryCallback& callback) {
  return Start(FROM_HERE, message_loop_proxy,
               new RelayCreateTemporary(message_loop_proxy,
                                        additional_file_flags,
                                        callback));
}

// static
bool FileUtilProxy::Close(scoped_refptr<MessageLoopProxy> message_loop_proxy,
                          PlatformFile file_handle,
                          const StatusCallback& callback) {
  return Start(FROM_HERE, message_loop_proxy,
               new RelayClose(file_handle, callback));
}

// Retrieves the information about a file. It is invalid to pass NULL for the
// callback.
bool FileUtilProxy::GetFileInfo(
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    const FilePath& file_path,
    const GetFileInfoCallback& callback) {
  return Start(FROM_HERE, message_loop_proxy, new RelayGetFileInfo(
               file_path, callback));
}

// static
bool FileUtilProxy::GetFileInfoFromPlatformFile(
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    PlatformFile file,
    const GetFileInfoCallback& callback) {
  return Start(FROM_HERE, message_loop_proxy,
               new RelayGetFileInfoFromPlatformFile(file, callback));
}

// static
bool FileUtilProxy::Delete(scoped_refptr<MessageLoopProxy> message_loop_proxy,
                           const FilePath& file_path,
                           bool recursive,
                           const StatusCallback& callback) {
  return Start(FROM_HERE, message_loop_proxy,
               new RelayDelete(file_path, recursive, callback));
}

// static
bool FileUtilProxy::RecursiveDelete(
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    const FilePath& file_path,
    const StatusCallback& callback) {
  return Start(FROM_HERE, message_loop_proxy,
               new RelayDelete(file_path, true, callback));
}

// static
bool FileUtilProxy::Read(
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    PlatformFile file,
    int64 offset,
    int bytes_to_read,
    const ReadCallback& callback) {
  if (bytes_to_read < 0)
    return false;

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
    const WriteCallback& callback) {
  if (bytes_to_write <= 0)
    return false;

  return Start(FROM_HERE, message_loop_proxy,
               new RelayWrite(file, offset, buffer, bytes_to_write, callback));
}

// static
bool FileUtilProxy::Touch(
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    PlatformFile file,
    const Time& last_access_time,
    const Time& last_modified_time,
    const StatusCallback& callback) {
  return Start(FROM_HERE, message_loop_proxy,
               new RelayTouch(file, last_access_time, last_modified_time,
                              callback));
}

// static
bool FileUtilProxy::Touch(
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    const FilePath& file_path,
    const Time& last_access_time,
    const Time& last_modified_time,
    const StatusCallback& callback) {
  return Start(FROM_HERE, message_loop_proxy,
               new RelayTouchFilePath(file_path, last_access_time,
                                      last_modified_time, callback));
}

// static
bool FileUtilProxy::Truncate(
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    PlatformFile file,
    int64 length,
    const StatusCallback& callback) {
  return Start(FROM_HERE, message_loop_proxy,
               new RelayTruncatePlatformFile(file, length, callback));
}

// static
bool FileUtilProxy::Truncate(
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    const FilePath& path,
    int64 length,
    const StatusCallback& callback) {
  return Start(FROM_HERE, message_loop_proxy,
               new RelayTruncate(path, length, callback));
}

// static
bool FileUtilProxy::Flush(
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    PlatformFile file,
    const StatusCallback& callback) {
  return Start(FROM_HERE, message_loop_proxy, new RelayFlush(file, callback));
}

}  // namespace base
