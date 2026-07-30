/*
 * Copyright 2026 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef CHROMIUM_COMPAT_SYS_FILE_H_
#define CHROMIUM_COMPAT_SYS_FILE_H_

#include <io.h>

inline constexpr int LOCK_SH = 1;
inline constexpr int LOCK_EX = 2;
inline constexpr int LOCK_NB = 4;
inline constexpr int LOCK_UN = 8;

inline constexpr int S_IRUSR = 0400;
inline constexpr int S_IWUSR = 0200;
inline constexpr int S_IRGRP = 0040;
inline constexpr int S_IROTH = 0004;

// Redirect close to _close
inline int close(int fd) {
  return _close(fd);
}

// flock stub
inline int flock(int fd, int operation) {
  return 0;
}

#endif  // CHROMIUM_COMPAT_SYS_FILE_H_
