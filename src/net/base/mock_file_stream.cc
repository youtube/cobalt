// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/mock_file_stream.h"

namespace net {

namespace testing {

int MockFileStream::OpenSync(const FilePath& path, int open_flags) {
  path_ = path;
  return ReturnError(FileStream::OpenSync(path, open_flags));
}

int MockFileStream::Seek(Whence whence, int64 offset,
                         const Int64CompletionCallback& callback) {
  return ReturnError(FileStream::Seek(whence, offset, callback));
}

int64 MockFileStream::SeekSync(Whence whence, int64 offset) {
  return ReturnError64(FileStream::SeekSync(whence, offset));
}

int64 MockFileStream::Available() {
  return ReturnError64(FileStream::Available());
}

int MockFileStream::Read(IOBuffer* buf,
                         int buf_len,
                         const CompletionCallback& callback) {
  return ReturnError(FileStream::Read(buf, buf_len, callback));
}

int MockFileStream::ReadSync(char* buf, int buf_len) {
  return ReturnError(FileStream::ReadSync(buf, buf_len));
}

int MockFileStream::ReadUntilComplete(char *buf, int buf_len) {
  return ReturnError(FileStream::ReadUntilComplete(buf, buf_len));
}

int MockFileStream::Write(IOBuffer* buf,
                          int buf_len,
                          const CompletionCallback& callback) {
  return ReturnError(FileStream::Write(buf, buf_len, callback));
}

int MockFileStream::WriteSync(const char* buf, int buf_len) {
  return ReturnError(FileStream::WriteSync(buf, buf_len));
}

int64 MockFileStream::Truncate(int64 bytes) {
  return ReturnError64(FileStream::Truncate(bytes));
}

int MockFileStream::Flush(const CompletionCallback& callback) {
  return ReturnError(FileStream::Flush(callback));
}

int MockFileStream::FlushSync() {
  return ReturnError(FileStream::FlushSync());
}

}  // namespace testing

}  // namespace net
