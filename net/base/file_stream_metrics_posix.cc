// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/file_stream_metrics.h"

namespace net {

int GetFileErrorUmaBucket(int error) {
  return 1;
}

int MaxFileErrorUmaBucket() {
  return 2;
}

int MaxFileErrorUmaValue() {
  return 160;
}

}  // namespace net
