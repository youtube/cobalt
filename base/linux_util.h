// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_LINUX_UTIL_H_
#define BASE_LINUX_UTIL_H_

#include <stdint.h>
#include <sys/types.h>

#include <string>

class FilePath;

namespace base {

class EnvVarGetter;

static const char kFindInodeSwitch[] = "--find-inode";

// Makes a copy of |pixels| with the ordering changed from BGRA to RGBA.
// The caller is responsible for free()ing the data. If |stride| is 0,
// it's assumed to be 4 * |width|.
uint8_t* BGRAToRGBA(const uint8_t* pixels, int width, int height, int stride);

// Get the Linux Distro if we can, or return "Unknown", similar to
// GetWinVersion() in base/win_util.h.
std::string GetLinuxDistro();

// Return the inode number for the UNIX domain socket |fd|.
bool FileDescriptorGetInode(ino_t* inode_out, int fd);

// Find the process which holds the given socket, named by inode number. If
// multiple processes hold the socket, this function returns false.
bool FindProcessHoldingSocket(pid_t* pid_out, ino_t socket_inode);

}  // namespace base

#endif  // BASE_LINUX_UTIL_H_
