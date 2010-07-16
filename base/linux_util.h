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

// Get the Linux Distro if we can, or return "Unknown", similar to
// GetWinVersion() in base/win_util.h.
std::string GetLinuxDistro();

// Return the inode number for the UNIX domain socket |fd|.
bool FileDescriptorGetInode(ino_t* inode_out, int fd);

// Find the process which holds the given socket, named by inode number. If
// multiple processes hold the socket, this function returns false.
bool FindProcessHoldingSocket(pid_t* pid_out, ino_t socket_inode);

// For a given process |pid|, look through all its threads and find the first
// thread with /proc/[pid]/task/[thread_id]/syscall whose first N bytes matches
// |expected_data|, where N is the length of |expected_data|.
// Returns the thread id or -1 on error.
pid_t FindThreadIDWithSyscall(pid_t pid, const std::string& expected_data);

}  // namespace base

#endif  // BASE_LINUX_UTIL_H_
