// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "net/disk_cache/flash/format.h"
#include "net/disk_cache/flash/log_store.h"
#include "net/disk_cache/flash/segment.h"
#include "net/disk_cache/flash/storage.h"

namespace disk_cache {

LogStore::LogStore(Storage* storage)
    : storage_(storage),
      num_segments_(storage->size() / kFlashSegmentSize),
      open_segments_(num_segments_),
      write_index_(0),
      current_entry_id_(-1),
      current_entry_num_bytes_left_to_write_(0),
      init_(false),
      closed_(false) {
  DCHECK(storage->size() % kFlashSegmentSize == 0);
}

LogStore::~LogStore() {
  DCHECK(!init_ || closed_);
  STLDeleteElements(&open_segments_);
}

bool LogStore::Init() {
  DCHECK(!init_);
  // TODO(agayev): Once we start persisting segment metadata to disk, we will
  // start from where we left off during the last shutdown.
  scoped_ptr<Segment> segment(new Segment(write_index_, false, storage_));
  if (!segment->Init())
    return false;

  segment->AddUser();
  open_segments_[write_index_] = segment.release();
  init_ = true;
  return true;
}

bool LogStore::Close() {
  DCHECK(init_ && !closed_);
  open_segments_[write_index_]->ReleaseUser();
  if (!open_segments_[write_index_]->Close())
    return false;
  closed_ = true;
  return true;
  // TODO(agayev): persist metadata to disk.
}

bool LogStore::CreateEntry(int32 size, int32* id) {
  DCHECK(init_ && !closed_);
  DCHECK(current_entry_id_ == -1 && size <= disk_cache::kFlashSegmentFreeSpace);

  // TODO(agayev): Avoid large entries from leaving the segments almost empty.
  if (!open_segments_[write_index_]->CanHold(size)) {
    if (!open_segments_[write_index_]->Close())
      return false;

    open_segments_[write_index_]->ReleaseUser();
    if (open_segments_[write_index_]->HasNoUsers()) {
      delete open_segments_[write_index_];
      open_segments_[write_index_] = NULL;
    }

    write_index_ = GetNextSegmentIndex();
    scoped_ptr<Segment> segment(new Segment(write_index_, false, storage_));
    if (!segment->Init())
      return false;

    segment->AddUser();
    open_segments_[write_index_] = segment.release();
  }

  *id = open_segments_[write_index_]->write_offset();
  open_segments_[write_index_]->StoreOffset(*id);
  current_entry_id_ = *id;
  current_entry_num_bytes_left_to_write_ = size;
  open_entries_.insert(current_entry_id_);
  return true;
}

bool LogStore::WriteData(const void* buffer, int32 size) {
  DCHECK(init_ && !closed_);
  DCHECK(current_entry_id_ != -1 &&
         size <= current_entry_num_bytes_left_to_write_);
  if (open_segments_[write_index_]->WriteData(buffer, size)) {
    current_entry_num_bytes_left_to_write_ -= size;
    return true;
  }
  return false;
}

bool LogStore::OpenEntry(int32 id) {
  DCHECK(init_ && !closed_);
  if (open_entries_.find(id) != open_entries_.end())
    return false;

  // Segment is already open.
  int32 index = id / disk_cache::kFlashSegmentSize;
  if (open_segments_[index]) {
    if (!open_segments_[index]->HaveOffset(id))
      return false;
    open_segments_[index]->AddUser();
    open_entries_.insert(id);
    return true;
  }

  // Segment is not open.
  scoped_ptr<Segment> segment(new Segment(index, true, storage_));
  if (!segment->Init() || !segment->HaveOffset(id))
    return false;

  segment->AddUser();
  open_segments_[index] = segment.release();
  open_entries_.insert(id);
  return true;
}

bool LogStore::ReadData(int32 id, void* buffer, int32 size,
                                  int32 offset) const {
  DCHECK(init_ && !closed_);
  DCHECK(open_entries_.find(id) != open_entries_.end());

  int32 index = id / disk_cache::kFlashSegmentSize;
  DCHECK(open_segments_[index] && open_segments_[index]->HaveOffset(id));
  return open_segments_[index]->ReadData(buffer, size, id + offset);
}

void LogStore::CloseEntry(int32 id) {
  DCHECK(init_ && !closed_);
  std::set<int32>::iterator entry_iter = open_entries_.find(id);
  DCHECK(entry_iter != open_entries_.end());

  if (current_entry_id_ != -1) {
    DCHECK(id == current_entry_id_ && !current_entry_num_bytes_left_to_write_);
    open_entries_.erase(entry_iter);
    current_entry_id_ = -1;
    return;
  }

  int32 index = id / disk_cache::kFlashSegmentSize;
  DCHECK(open_segments_[index]);
  open_entries_.erase(entry_iter);

  open_segments_[index]->ReleaseUser();
  if (open_segments_[index]->HasNoUsers()) {
    delete open_segments_[index];
    open_segments_[index] = NULL;
  }
}

int32 LogStore::GetNextSegmentIndex() {
  DCHECK(init_ && !closed_);
  int32 next_index = (write_index_ + 1) % num_segments_;

  while (InUse(next_index)) {
    next_index = (next_index + 1) % num_segments_;
    DCHECK_NE(next_index, write_index_);
  }
  return next_index;
}

bool LogStore::InUse(int32 index) const {
  DCHECK(init_ && !closed_);
  DCHECK(index >= 0 && index < num_segments_);
  return open_segments_[index] != NULL;
}

}  // namespace disk_cache
