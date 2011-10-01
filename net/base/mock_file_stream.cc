// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/mock_file_stream.h"

namespace net {

namespace testing {

int MockFileStream::Open(const FilePath& path, int open_flags) {
  path_ = path;
  return ReturnError(net::FileStream::Open(path, open_flags));
}

int64 MockFileStream::Seek(net::Whence whence, int64 offset) {
  return ReturnError64(net::FileStream::Seek(whence, offset));
}

int64 MockFileStream::Available() {
  return ReturnError64(net::FileStream::Available());
}

int MockFileStream::Read(char* buf,
                 int buf_len,
                 net::OldCompletionCallback* callback) {
  return ReturnError(net::FileStream::Read(buf, buf_len, callback));
}

int MockFileStream::ReadUntilComplete(char *buf, int buf_len) {
  return ReturnError(net::FileStream::ReadUntilComplete(buf, buf_len));
}

int MockFileStream::Write(const char* buf,
                  int buf_len,
                  net::OldCompletionCallback* callback) {
  return ReturnError(net::FileStream::Write(buf, buf_len, callback));
}

int64 MockFileStream::Truncate(int64 bytes) {
  return ReturnError64(net::FileStream::Truncate(bytes));
}

int MockFileStream::Flush() {
  return ReturnError(net::FileStream::Flush());
}

}  // namespace testing

}  // namespace net
