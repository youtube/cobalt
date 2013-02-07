// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_FLASH_STORAGE_H_
#define NET_DISK_CACHE_FLASH_STORAGE_H_

#include "base/basictypes.h"
#include "base/platform_file.h"
#include "net/base/net_export.h"

namespace disk_cache {

class NET_EXPORT_PRIVATE Storage {
 public:
  Storage(const FilePath& path, int32 size);
  bool Init();
  ~Storage();

  int32 size() const { return size_; }

  bool Read(void* buffer, int32 size, int32 offset);
  bool Write(const void* buffer, int32 size, int32 offset);

 private:
  FilePath path_;
  int32 size_;
  base::PlatformFile file_;

  DISALLOW_COPY_AND_ASSIGN(Storage);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_FLASH_STORAGE_H_
