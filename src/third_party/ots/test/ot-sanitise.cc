// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A very simple driver program while sanitises the file given as argv[1] and
// writes the sanitised version to stdout.

#include <fcntl.h>
#include <sys/stat.h>
#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif  // defined(_WIN32)

#include <cstdio>
#include <cstdlib>

#include "file-stream.h"
#include "opentype-sanitiser.h"

namespace {

int Usage(const char *argv0) {
  std::fprintf(stderr, "Usage: %s ttf_file > dest_ttf_file\n", argv0);
  return 1;
}

}  // namespace

int main(int argc, char **argv) {
  if (argc != 2) return Usage(argv[0]);
  if (::isatty(1)) return Usage(argv[0]);

  const int fd = ::open(argv[1], O_RDONLY);
  if (fd < 0) {
    ::perror("open");
    return 1;
  }

  struct stat st;
  ::fstat(fd, &st);

  uint8_t *data = new uint8_t[st.st_size];
  if (::read(fd, data, st.st_size) != st.st_size) {
    ::perror("read");
    return 1;
  }
  ::close(fd);

  ots::FILEStream output(stdout);
  const bool result = ots::Process(&output, data, st.st_size);

  if (!result) {
    std::fprintf(stderr, "Failed to sanitise file!\n");
  }
  return !result;
}
