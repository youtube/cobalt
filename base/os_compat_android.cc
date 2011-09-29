// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/os_compat_android.h"

#include "base/stringprintf.h"

// There is no futimes() avaiable in Bionic, so we provide our own
// implementation until it is there.
extern "C" {

int futimes(int fd, const struct timeval tv[2]) {
  const std::string fd_path = StringPrintf("/proc/self/fd/%d", fd);
  return utimes(fd_path.c_str(), tv);
}

}  // extern "C"
