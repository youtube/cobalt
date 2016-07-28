// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media.h"

#include "base/logging.h"

// This file is intended for platforms that don't need to load any media
// libraries (e.g., Android and iOS).
namespace media {

bool InitializeMediaLibrary(const FilePath& module_dir) {
  return true;
}

void InitializeMediaLibraryForTesting() {
}

bool IsMediaLibraryInitialized() {
  return true;
}

}  // namespace media
