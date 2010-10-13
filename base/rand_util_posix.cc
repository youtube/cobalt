// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/rand_util.h"
#include "base/rand_util_c.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/stringprintf.h"

namespace {

// We keep the file descriptor for /dev/urandom around so we don't need to
// reopen it (which is expensive), and since we may not even be able to reopen
// it if we are later put in a sandbox. This class wraps the file descriptor so
// we can use LazyInstance to handle opening it on the first access.
class URandomFd {
 public:
  URandomFd() {
    fd_ = open("/dev/urandom", O_RDONLY);
    CHECK_GE(fd_, 0) << "Cannot open /dev/urandom: " << errno;
  }

  ~URandomFd() {
    close(fd_);
  }

  int fd() const { return fd_; }

 private:
  int fd_;
};

base::LazyInstance<URandomFd> g_urandom_fd(base::LINKER_INITIALIZED);

}  // namespace

namespace base {

uint64 RandUint64() {
  uint64 number;

  int urandom_fd = g_urandom_fd.Pointer()->fd();
  bool success = file_util::ReadFromFD(urandom_fd,
                                       reinterpret_cast<char*>(&number),
                                       sizeof(number));
  CHECK(success);

  return number;
}

// TODO(cmasone): Once we're comfortable this works, migrate Windows code to
// use this as well.
std::string RandomBytesToGUIDString(const uint64 bytes[2]) {
  return StringPrintf("%08X-%04X-%04X-%04X-%012llX",
                      static_cast<unsigned int>(bytes[0] >> 32),
                      static_cast<unsigned int>((bytes[0] >> 16) & 0x0000ffff),
                      static_cast<unsigned int>(bytes[0] & 0x0000ffff),
                      static_cast<unsigned int>(bytes[1] >> 48),
                      bytes[1] & 0x0000ffffffffffffULL);
}

std::string GenerateGUID() {
  uint64 sixteen_bytes[2] = { base::RandUint64(), base::RandUint64() };
  return RandomBytesToGUIDString(sixteen_bytes);
}

}  // namespace base

int GetUrandomFD(void) {
  return g_urandom_fd.Pointer()->fd();
}
