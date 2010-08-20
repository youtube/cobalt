// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util_proxy.h"

#include "base/file_util.h"
#include "base/message_loop_proxy.h"

namespace {

class MessageLoopRelay
    : public base::RefCountedThreadSafe<MessageLoopRelay> {
 public:
  MessageLoopRelay()
      : origin_message_loop_proxy_(
          base::MessageLoopProxy::CreateForCurrentThread()) {
  }

  void Start(scoped_refptr<base::MessageLoopProxy> message_loop_proxy,
             const tracked_objects::Location& from_here) {
    message_loop_proxy->PostTask(
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

 private:
  void ProcessOnTargetThread() {
    RunWork();
    origin_message_loop_proxy_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &MessageLoopRelay::RunCallback));
  }

  scoped_refptr<base::MessageLoopProxy> origin_message_loop_proxy_;
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
    file_handle_ = base::CreatePlatformFile(file_path_, file_flags_, &created_);
  }

  virtual void RunCallback() {
    callback_->Run(base::PassPlatformFile(&file_handle_), created_);
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
    file_handle_ = base::CreatePlatformFile(file_path_, file_flags, NULL);
  }

  virtual void RunCallback() {
    callback_->Run(base::PassPlatformFile(&file_handle_), file_path_);
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
      : callback_(callback),
        succeeded_(false) {
    // It is OK for callback to be NULL.
  }

 protected:
  virtual void RunCallback() {
    // The caller may not have been interested in the result.
    if (callback_) {
      callback_->Run(succeeded_);
      delete callback_;
    }
  }

  void SetStatus(bool succeeded) { succeeded_ = succeeded; }

 private:
  base::FileUtilProxy::StatusCallback* callback_;
  bool succeeded_;
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
    SetStatus(base::ClosePlatformFile(file_handle_));
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
    SetStatus(file_util::Delete(file_path_, recursive_));
  }

 private:
  FilePath file_path_;
  bool recursive_;
};

void Start(const tracked_objects::Location& from_here,
           scoped_refptr<base::MessageLoopProxy> message_loop_proxy,
           scoped_refptr<MessageLoopRelay> relay) {
  relay->Start(message_loop_proxy, from_here);
}

}  // namespace

namespace base {

// static
void FileUtilProxy::CreateOrOpen(
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    const FilePath& file_path, int file_flags,
    CreateOrOpenCallback* callback) {
  Start(FROM_HERE, message_loop_proxy, new RelayCreateOrOpen(
      message_loop_proxy, file_path, file_flags, callback));
}

// static
void FileUtilProxy::CreateTemporary(
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    CreateTemporaryCallback* callback) {
  Start(FROM_HERE, message_loop_proxy,
        new RelayCreateTemporary(message_loop_proxy, callback));
}

// static
void FileUtilProxy::Close(scoped_refptr<MessageLoopProxy> message_loop_proxy,
                          base::PlatformFile file_handle,
                          StatusCallback* callback) {
  Start(FROM_HERE, message_loop_proxy, new RelayClose(file_handle, callback));
}

// static
void FileUtilProxy::Delete(scoped_refptr<MessageLoopProxy> message_loop_proxy,
                           const FilePath& file_path,
                           StatusCallback* callback) {
  Start(FROM_HERE, message_loop_proxy,
        new RelayDelete(file_path, false, callback));
}

// static
void FileUtilProxy::RecursiveDelete(
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    const FilePath& file_path,
    StatusCallback* callback) {
  Start(FROM_HERE, message_loop_proxy,
        new RelayDelete(file_path, true, callback));
}

} // namespace base
