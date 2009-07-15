// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "media/base/mock_filters.h"

namespace media {

void RunFilterCallback(::testing::Unused, FilterCallback* callback) {
  callback->Run();
  delete callback;
}

void DestroyFilterCallback(::testing::Unused, FilterCallback* callback) {
  delete callback;
}

}  // namespace media
