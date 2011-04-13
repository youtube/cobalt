// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/addr.h"

#include "base/logging.h"

namespace disk_cache {

int Addr::start_block() const {
  DCHECK(is_block_file());
  return value_ & kStartBlockMask;
}

int Addr::num_blocks() const {
  DCHECK(is_block_file() || !value_);
  return ((value_ & kNumBlocksMask) >> kNumBlocksOffset) + 1;
}

bool Addr::SetFileNumber(int file_number) {
  DCHECK(is_separate_file());
  if (file_number & ~kFileNameMask)
    return false;
  value_ = kInitializedMask | file_number;
  return true;
}

bool Addr::SanityCheck() const {
  if (!is_initialized())
    return !value_;

  if (((value_ & kFileTypeMask) >> kFileTypeOffset) > 4)
    return false;

  if (is_separate_file())
    return true;

  const uint32 kReservedBitsMask = 0x0c000000;
  return !(value_ & kReservedBitsMask);
}

}  // namespace disk_cache
