// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/file_stream.h"

namespace net {

FileStream::FileStream(net::NetLog* net_log)
    : impl_(net_log) {
}

FileStream::FileStream(
    base::PlatformFile file, int flags, net::NetLog* net_log)
    : impl_(file, flags, net_log) {
}

FileStream::~FileStream() {
}

void FileStream::Close(const CompletionCallback& callback) {
  impl_.Close(callback);
}

void FileStream::CloseSync() {
  impl_.CloseSync();
}

int FileStream::Open(const FilePath& path, int open_flags,
                     const CompletionCallback& callback) {
  return impl_.Open(path, open_flags, callback);
}

int FileStream::OpenSync(const FilePath& path, int open_flags) {
  return impl_.OpenSync(path, open_flags);
}

bool FileStream::IsOpen() const {
  return impl_.IsOpen();
}

int FileStream::Seek(Whence whence, int64 offset,
                     const Int64CompletionCallback& callback) {
  return impl_.Seek(whence, offset, callback);
}

int64 FileStream::SeekSync(Whence whence, int64 offset) {
  return impl_.SeekSync(whence, offset);
}

int64 FileStream::Available() {
  return impl_.Available();
}

int FileStream::Read(
    IOBuffer* in_buf, int buf_len, const CompletionCallback& callback) {
  return impl_.Read(in_buf, buf_len, callback);
}

int FileStream::ReadSync(char* buf, int buf_len) {
  return impl_.ReadSync(buf, buf_len);
}

int FileStream::ReadUntilComplete(char *buf, int buf_len) {
  return impl_.ReadUntilComplete(buf, buf_len);
}

int FileStream::Write(
    IOBuffer* buf, int buf_len, const CompletionCallback& callback) {
  return impl_.Write(buf, buf_len, callback);
}

int FileStream::WriteSync(const char* buf, int buf_len) {
  return impl_.WriteSync(buf, buf_len);
}

int64 FileStream::Truncate(int64 bytes) {
  return impl_.Truncate(bytes);
}

int FileStream::Flush() {
  return impl_.Flush();
}

void FileStream::EnableErrorStatistics() {
  impl_.EnableErrorStatistics();
}

void FileStream::SetBoundNetLogSource(
    const net::BoundNetLog& owner_bound_net_log) {
  impl_.SetBoundNetLogSource(owner_bound_net_log);
}

base::PlatformFile FileStream::GetPlatformFileForTesting() {
  return impl_.GetPlatformFileForTesting();
}

}  // namespace net
