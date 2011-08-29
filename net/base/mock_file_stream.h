// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines MockFileStream, a test class for FileStream.

#ifndef NET_BASE_MOCK_FILE_STREAM_H_
#define NET_BASE_MOCK_FILE_STREAM_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "net/base/file_stream.h"
#include "net/base/net_errors.h"

namespace net {

namespace testing {

class MockFileStream : public net::FileStream {
 public:
  MockFileStream() : forced_error_(net::OK) {}

  MockFileStream(base::PlatformFile file, int flags)
      : net::FileStream(file, flags), forced_error_(net::OK) {}

  // FileStream methods.
  virtual int Open(const FilePath& path, int open_flags) OVERRIDE;
  virtual int64 Seek(net::Whence whence, int64 offset) OVERRIDE;
  virtual int64 Available() OVERRIDE;
  virtual int Read(char* buf,
                   int buf_len,
                   net::CompletionCallback* callback) OVERRIDE;
  virtual int ReadUntilComplete(char *buf, int buf_len) OVERRIDE;
  virtual int Write(const char* buf,
                    int buf_len,
                    net::CompletionCallback* callback) OVERRIDE;
  virtual int64 Truncate(int64 bytes) OVERRIDE;
  virtual int Flush() OVERRIDE;

  void set_forced_error(int error) { forced_error_ = error; }
  void clear_forced_error() { forced_error_ = net::OK; }
  const FilePath& get_path() const { return path_; }

 private:
  int ReturnError(int function_error) {
    if (forced_error_ != net::OK) {
      int ret = forced_error_;
      clear_forced_error();
      return ret;
    }

    return function_error;
  }

  int64 ReturnError64(int64 function_error) {
    if (forced_error_ != net::OK) {
      int64 ret = forced_error_;
      clear_forced_error();
      return ret;
    }

    return function_error;
  }

  int forced_error_;
  FilePath path_;
};

}  // namespace testing

}  // namespace net

#endif  // NET_BASE_MOCK_FILE_STREAM_H_
