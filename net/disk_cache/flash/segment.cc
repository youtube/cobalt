// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/logging.h"
#include "net/disk_cache/flash/format.h"
#include "net/disk_cache/flash/segment.h"
#include "net/disk_cache/flash/storage.h"

namespace disk_cache {

Segment::Segment(int32 index, bool read_only, Storage* storage)
    : index_(index),
      num_users_(0),
      read_only_(read_only),
      init_(false),
      storage_(storage),
      offset_(index * kFlashSegmentSize),
      summary_offset_(offset_ + kFlashSegmentSize - kFlashSummarySize),
      write_offset_(offset_) {
  DCHECK(storage);
  DCHECK(storage->size() % kFlashSegmentSize == 0);
}

Segment::~Segment() {
  DCHECK(!init_ || read_only_);
  if (num_users_ != 0)
    LOG(WARNING) << "Users exist, but we don't care? " << num_users_;
}

bool Segment::HaveOffset(int32 offset) const {
  DCHECK(init_);
  return std::binary_search(offsets_.begin(), offsets_.end(), offset);
}

void Segment::AddUser() {
  DCHECK(init_);
  ++num_users_;
}

void Segment::ReleaseUser() {
  DCHECK(init_);
  --num_users_;
}

bool Segment::HasNoUsers() const {
  DCHECK(init_);
  return num_users_ == 0;
}

bool Segment::Init() {
  DCHECK(!init_);

  if (offset_ < 0 || offset_ + kFlashSegmentSize > storage_->size())
    return false;

  if (!read_only_) {
    init_ = true;
    return true;
  }

  int32 summary[kFlashMaxEntryCount + 1];
  if (!storage_->Read(summary, kFlashSummarySize, summary_offset_))
    return false;

  size_t entry_count = summary[0];
  DCHECK_LE(entry_count, kFlashMaxEntryCount);

  std::vector<int32> tmp(summary + 1, summary + 1 + entry_count);
  offsets_.swap(tmp);
  init_ = true;
  return true;
}

bool Segment::WriteData(const void* buffer, int32 size) {
  DCHECK(init_ && !read_only_);
  DCHECK(write_offset_ + size <= summary_offset_);
  if (!storage_->Write(buffer, size, write_offset_))
    return false;
  write_offset_ += size;
  return true;
}

void Segment::StoreOffset(int32 offset) {
  DCHECK(init_ && !read_only_);
  DCHECK(offsets_.size() < kFlashMaxEntryCount);
  offsets_.push_back(offset);
}

bool Segment::ReadData(void* buffer, int32 size, int32 offset) const {
  DCHECK(init_);
  DCHECK(offset >= offset_ && offset + size <= offset_ + kFlashSegmentSize);
  return storage_->Read(buffer, size, offset);
}

bool Segment::Close() {
  DCHECK(init_);
  if (read_only_)
    return true;

  DCHECK(offsets_.size() <= kFlashMaxEntryCount);

  int32 summary[kFlashMaxEntryCount + 1];
  memset(summary, 0, kFlashSummarySize);
  summary[0] = offsets_.size();
  std::copy(offsets_.begin(), offsets_.end(), summary + 1);
  if (!storage_->Write(summary, kFlashSummarySize, summary_offset_))
    return false;

  read_only_ = true;
  return true;
}

bool Segment::CanHold(int32 size) const {
  DCHECK(init_);
  return offsets_.size() < kFlashMaxEntryCount &&
      write_offset_ + size <= summary_offset_;
}

}  // namespace disk_cache
