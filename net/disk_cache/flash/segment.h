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
// on a segment.  As a result, write APIs do not take an offset, but can return
// an offset of where the write took place.  The entries living on a segment are
// expected to consist of data part and header part where the header holds
// enough information to identify all the data belonging to it.  The difference
// between WriteData and WriteHeader is that writes issued with the latter have
// their offsets saved in the metadata, which can later be retrieved.  Thus, it
// is up to the client to specify data layout and associate the header with
// data.  For example the client may issue WriteData calls and save the offsets
// which then can be written using WriteHeader.  Note that it is possible to
// write entries using just WriteHeader as well, for example if it is an entry
// of the format where header specifies the length of the following data.
// Before attempting to write an entry, the client should call CanHold to make
// sure that there is enough space in the segment.
//
// ReadData can be called over the range that was previously written with
// WriteData/WriteHeader.  Reading from area that was not written will fail.

class NET_EXPORT_PRIVATE Segment {
 public:
  // |index| is the index of this segment on |storage|.  If the storage size is
  // X and the segment size is Y, where X >> Y and X % Y == 0, then the valid
  // values for the index are integers within the range [0, X/Y).  Thus, if
  // |index| is given value Z, then it covers bytes on storage starting at the
  // offset Z*Y and ending at the offset Z*Y+Y-1.
  Segment(int32 index, bool read_only, Storage* storage);
  ~Segment();

  const std::vector<int32>& header_offsets() const { return header_offsets_; }

  // Performs segment initialization.  Must be the first function called on the
  // segment and further calls should be made only if it is successful.
  bool Init();

  // Writes |size| bytes of data from |buffer| to segment, returns false if
  // fails and true if succeeds and sets the |offset|, if it is not NULL.  Can
  // block for a long time.
  bool WriteData(const void* buffer, int32 size, int32* offset);

  // Writes |header| of |size| bytes, returns false if fails and true if
  // succeeds and sets the |offset|, if it is not NULL.  Can block for a long
  // time.  The difference between this function and WriteData is that the
  // offset of this write operation is saved in the segment metadata and can
  // later be retrieved via |header_offsets|.
  bool WriteHeader(const void* header, int32 size, int32* offset);

  // Reads |size| bytes of data living at |offset| into |buffer|, returns true
  // on success and false on failure.
  bool ReadData(void* buffer, int32 size, int32 offset) const;

  // Closes the segment, returns true on success and false on failure.  Closing
  // a segment makes it immutable.
  bool Close();

  // Returns true if segment can accommodate an entry of |size| bytes.
  bool CanHold(int32 size) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(SegmentTest, CreateDestroy);

  bool read_only_;  // Indicates whether the segment can be written to.
  bool init_;  // Indicates whether segment was initialized.
  Storage* storage_;  // Storage on which the segment resides.
  const int32 offset_;  // Offset of the segment on |storage_|.
  const int32 summary_offset_;  // Offset of the segment summary.
  int32 write_offset_;  // Current write offset.
  std::vector<int32> header_offsets_;

  DISALLOW_COPY_AND_ASSIGN(Segment);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_FLASH_SEGMENT_H_
