// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_FLASH_SEGMENT_H_
#define NET_DISK_CACHE_FLASH_SEGMENT_H_

#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "net/base/net_export.h"

namespace disk_cache {

class Storage;

// The underlying storage represented by Storage class, is divided into fixed
// size logical segments, represented by this class.  Since segment size is
// fixed, the storage size should be a multiple of segment size.  The picture
// below describes the relation between storage and segments:
//
//   |-----------+-----------+-----+-------------+-----------|
//   | segment 0 | segment 1 | ... | segment n-1 | segment n |
//   |-----------+-----------+-----+-------------+-----------|
//
//   |-------------------------------------------------------|
//   |                       storage                         |
//   |-------------------------------------------------------|
//
// A segment is constructed by taking its index within the storage, a flag
// indicating whether it is a read-only segment and a pointer to the storage on
// which it resides.  It provides an API for reading/writing entries residing on
// it.  Init() function must be called right after the construction of a segment
// and one should proceed to calling other functions only if Init() has
// succeeded.  After a successful initialization, one may proceed to call
// non-mutating functions; mutating functions can be called if the segment is
// not read-only.  Finally, Close() function must be called right before the
// destruction.  Calling Close() makes the segment immutable, which means
// mutating functions cannot be called on the object after that.
//
// Segment can only be used as a log, i.e. all writes are laid out sequentially
// on a segment.  As a result, WriteData() function does not take an offset.
// Current write offset can be learned by calling write_offset().
//
// Once the entries are written to the Segment and Close() called on it and the
// object destroyed, we should later be able to instantiate a read-only Segment
// object and recreate all the entries that were previously written to it.  To
// achieve this, a tiny region of Segment is used for its metadata and Segment
// provides two calls for interacting with metadata: StoreOffset() and
// GetOffsets().  The former can be used to store an offset that was returned by
// write_offset() and the latter can be used to retrieve all the offsets that
// were stored in the Segment.  Before attempting to write an entry, the client
// should call CanHold() to make sure that there is enough space in the segment.
//
// ReadData can be called over the range that was previously written with
// WriteData.  Reading from area that was not written will fail.

class NET_EXPORT_PRIVATE Segment {
 public:
  // |index| is the index of this segment on |storage|.  If the storage size is
  // X and the segment size is Y, where X >> Y and X % Y == 0, then the valid
  // values for the index are integers within the range [0, X/Y).  Thus, if
  // |index| is given value Z, then it covers bytes on storage starting at the
  // offset Z*Y and ending at the offset Z*Y+Y-1.
  Segment(int32 index, bool read_only, Storage* storage);
  ~Segment();

  int32 index() const { return index_; }
  int32 write_offset() const { return write_offset_; }

  bool HaveOffset(int32 offset) const;
  std::vector<int32> GetOffsets() const { return offsets_; }

  // Manage the number of users of this segment.
  void AddUser();
  void ReleaseUser();
  bool HasNoUsers() const;

  // Performs segment initialization.  Must be the first function called on the
  // segment and further calls should be made only if it is successful.
  bool Init();

  // Writes |size| bytes of data from |buffer| to segment, returns false if
  // fails and true if succeeds.  Can block for a long time.
  bool WriteData(const void* buffer, int32 size);

  // Reads |size| bytes of data living at |offset| into |buffer| returns true on
  // success and false on failure.
  bool ReadData(void* buffer, int32 size, int32 offset) const;

  // Stores the offset in the metadata.
  void StoreOffset(int32 offset);

  // Closes the segment, returns true on success and false on failure.  Closing
  // a segment makes it immutable.
  bool Close();

  // Returns true if segment can accommodate an entry of |size| bytes.
  bool CanHold(int32 size) const;

 private:
  int32 index_;
  int32 num_users_;
  bool read_only_;  // Indicates whether the segment can be written to.
  bool init_;  // Indicates whether segment was initialized.
  Storage* storage_;  // Storage on which the segment resides.
  const int32 offset_;  // Offset of the segment on |storage_|.
  const int32 summary_offset_;  // Offset of the segment summary.
  int32 write_offset_;  // Current write offset.
  std::vector<int32> offsets_;

  DISALLOW_COPY_AND_ASSIGN(Segment);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_FLASH_SEGMENT_H_
