// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"

#include "base/file_path.h"

namespace file_util {

bool GetShmemTempDir(FilePath* path, bool executable) {
  *path = FilePath("/data/local/tmp");
  return true;
}

}  // namespace file_util
