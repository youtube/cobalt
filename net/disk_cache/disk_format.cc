// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/disk_format.h"

namespace disk_cache {

IndexHeader::IndexHeader() {
  memset(this, 0, sizeof(*this));
  magic = kIndexMagic;
  version = kCurrentVersion;
}

BlockFileHeader::BlockFileHeader() {
  memset(this, 0, sizeof(BlockFileHeader));
  magic = kBlockMagic;
  version = kCurrentVersion;
}

}  // namespace disk_cache
