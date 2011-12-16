// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/os_compat_android.h"

#include <time64.h>

#include "base/stringprintf.h"

// There is no futimes() avaiable in Bionic, so we provide our own
// implementation until it is there.
extern "C" {

int futimes(int fd, const struct timeval tv[2]) {
  const std::string fd_path = StringPrintf("/proc/self/fd/%d", fd);
  return utimes(fd_path.c_str(), tv);
}

// Android has only timegm64() and no timegm().
// We replicate the behaviour of timegm() when the result overflows time_t.
time_t timegm(struct tm* const t) {
  // time_t is signed on Android.
  static const time_t kTimeMax = ~(1 << (sizeof(time_t) * CHAR_BIT - 1));
  static const time_t kTimeMin = (1 << (sizeof(time_t) * CHAR_BIT - 1));
  time64_t result = timegm64(t);
  if (result < kTimeMin || result > kTimeMax)
    return -1;
  return result;
}

}  // extern "C"
