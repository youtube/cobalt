// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/flash/format.h"
#include "net/disk_cache/flash/log_store.h"
#include "net/disk_cache/flash/log_store_entry.h"

namespace disk_cache {

LogStoreEntry::LogStoreEntry(LogStore* store)
    : store_(store), id_(-1), init_(false), closed_(false) {
  DCHECK(store);
}

LogStoreEntry::LogStoreEntry(LogStore* store, int32 id)
    : store_(store), id_(id), init_(false), closed_(false) {
  DCHECK(store);
}

LogStoreEntry::~LogStoreEntry() {
  DCHECK(!init_ || closed_);
}

bool LogStoreEntry::Init() {
  DCHECK(!init_);
  if (!OnDisk()) {
    init_ = true;
    return true;
  }

  int32 stream_sizes[kFlashLogStoreEntryNumStreams];
  COMPILE_ASSERT(sizeof(stream_sizes) == kFlashLogStoreEntryHeaderSize,
                 invalid_log_store_entry_header_size);

  if (!store_->OpenEntry(id_) ||
      !store_->ReadData(id_, stream_sizes, kFlashLogStoreEntryHeaderSize, 0)) {
    return false;
  }
  for (int i = 0, offset = kFlashLogStoreEntryHeaderSize;
       i < kFlashLogStoreEntryNumStreams; ++i) {
    streams_[i].offset = offset;
    streams_[i].size = stream_sizes[i];
    offset += stream_sizes[i];
  }
  init_ = true;
  return true;
}

bool LogStoreEntry::Close() {
  DCHECK(init_ && !closed_);
  if (OnDisk())
    store_->CloseEntry(id_);
  else if (!Save())
    return false;
  closed_ = true;
  return true;
}

int32 LogStoreEntry::id() const {
  DCHECK(init_);
  return id_;
}

int32 LogStoreEntry::GetDataSize(int index) const {
  DCHECK(init_);
  return InvalidStream(index) ? 0 : streams_[index].size;
}

int LogStoreEntry::ReadData(int index, int offset, net::IOBuffer* buf,
                            int buf_len) {
  DCHECK(init_);
  if (InvalidStream(index))
    return net::ERR_INVALID_ARGUMENT;

  int stream_size = streams_[index].size;
  if (offset >= stream_size || offset < 0 || buf_len == 0)
    return 0;
  if (offset + buf_len > stream_size)
    buf_len = stream_size - offset;

  if (OnDisk()) {
    offset += streams_[index].offset;
    if (store_->ReadData(id_, buf->data(), buf_len, offset))
      return buf_len;
    return net::ERR_FAILED;
  }
  memcpy(buf->data(), &streams_[index].write_buffer[offset], buf_len);
  return buf_len;
}

int LogStoreEntry::WriteData(int index, int offset, net::IOBuffer* buf,
                             int buf_len) {
  DCHECK(init_ && !closed_);
  if (InvalidStream(index))
    return net::ERR_INVALID_ARGUMENT;

  DCHECK(offset >= 0 && buf_len >= 0);
  Stream& stream = streams_[index];
  size_t new_size = static_cast<size_t>(offset + buf_len);
  if (new_size) {
    // TODO(agayev): Currently, only append and overwrite is supported.  Add
    // support for arbitrary writes.
    DCHECK(!offset || offset == stream.size);
    if (stream.write_buffer.size() < new_size)
      stream.write_buffer.resize(new_size);
    memcpy(&streams_[index].write_buffer[offset], buf->data(), buf_len);
  }
  stream.size = new_size;
  return buf_len;
}

bool LogStoreEntry::OnDisk() const {
  return id_ != -1;
}

bool LogStoreEntry::InvalidStream(int stream_index) const {
  return stream_index < 0 || stream_index >= kFlashLogStoreEntryNumStreams;
}

int32 LogStoreEntry::Size() const {
  DCHECK(init_);
  int32 size = kFlashLogStoreEntryHeaderSize;
  for (int i = 0; i < kFlashLogStoreEntryNumStreams; ++i)
    size += streams_[i].size;
  DCHECK(size > 0 && size <= kFlashSegmentFreeSpace);
  return size;
}

bool LogStoreEntry::Save() {
  DCHECK(init_ && !closed_ && !OnDisk());
  int32 stream_sizes[kFlashLogStoreEntryNumStreams];
  COMPILE_ASSERT(sizeof(stream_sizes) == kFlashLogStoreEntryHeaderSize,
                 invalid_log_store_entry_header_size);

  for (int i = 0; i < kFlashLogStoreEntryNumStreams; ++i)
    stream_sizes[i] = streams_[i].size;

  if (!store_->CreateEntry(Size(), &id_))
    return false;
  if (!store_->WriteData(stream_sizes, kFlashLogStoreEntryHeaderSize))
    return false;
  for (int i = 0; i < kFlashLogStoreEntryNumStreams; ++i) {
    if (streams_[i].size > 0 &&
        !store_->WriteData(&streams_[i].write_buffer[0], streams_[i].size)) {
      return false;
    }
  }
  store_->CloseEntry(id_);
  return true;
}

LogStoreEntry::Stream::Stream() : offset(0), size(0) {
}

LogStoreEntry::Stream::~Stream() {
}

}  // namespace disk_cache
