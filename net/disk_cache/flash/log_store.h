// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_FLASH_LOG_STORE_H_
#define NET_DISK_CACHE_FLASH_LOG_STORE_H_

#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "net/base/net_export.h"

namespace disk_cache {

class Segment;
class Storage;

// This class implements a general purpose store for storing and retrieving
// entries consisting of arbitrary binary data.  The store has log semantics,
// i.e. it's not possible to overwrite data in place.  In order to update an
// entry, a new version must be written.  Only one entry can be written to at
// any given time, while concurrent reading of multiple entries is supported.
class NET_EXPORT_PRIVATE LogStore {
 public:
  explicit LogStore(Storage* storage);
  ~LogStore();

  // Performs initialization.  Must be the first function called and further
  // calls should be made only if it is successful.
  bool Init();

  // Closes the store.  Should be the last function called before destruction.
  bool Close();

  // Creates an entry of |size| bytes.  The id of the created entry is stored in
  // |entry_id|.
  bool CreateEntry(int32 size, int32* entry_id);

  // TODO(agayev): Add DeleteEntry.

  // Appends data to the end of the last created entry.
  bool WriteData(const void* buffer, int32 size);

  // Opens an entry with id |entry_id|.
  bool OpenEntry(int32 entry_id);

  // Reads |size| bytes starting from |offset| into |buffer|, where |offset| is
  // relative to the entry's content, from an entry identified by |entry_id|.
  bool ReadData(int32 entry_id, void* buffer, int32 size, int32 offset) const;

  // Closes an entry that was either opened with OpenEntry or created with
  // CreateEntry.
  void CloseEntry(int32 id);

 private:
  FRIEND_TEST_ALL_PREFIXES(FlashCacheTest,
                           LogStoreReadFromClosedSegment);
  FRIEND_TEST_ALL_PREFIXES(FlashCacheTest,
                           LogStoreSegmentSelectionIsFifo);
  FRIEND_TEST_ALL_PREFIXES(FlashCacheTest,
                           LogStoreInUseSegmentIsSkipped);
  FRIEND_TEST_ALL_PREFIXES(FlashCacheTest,
                           LogStoreReadFromCurrentAfterClose);

  int32 GetNextSegmentIndex();
  bool InUse(int32 segment_index) const;

  Storage* storage_;

  int32 num_segments_;

  // Currently open segments, either for reading or writing.  There can only be
  // one segment open for writing, and multiple open for reading.
  std::vector<Segment*> open_segments_;

  // The index of the segment currently being written to.  It's an index to
  // |open_segments_| vector.
  int32 write_index_;

  // Ids of entries currently open, either CreatEntry'ed or OpenEntry'ed.
  std::set<int32> open_entries_;

  // Id of the entry that is currently being written to, -1 if there is no entry
  // currently being written to.
  int32 current_entry_id_;

  // Number of bytes left to be written to the entry identified by
  // |current_entry_id_|.  Its value makes sense iff |current_entry_id_| is not
  // -1.
  int32 current_entry_num_bytes_left_to_write_;

  bool init_;  // Init was called.
  bool closed_;  // Close was called.

  DISALLOW_COPY_AND_ASSIGN(LogStore);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_FLASH_LOG_STORE_H_
