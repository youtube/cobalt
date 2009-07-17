// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/linux_util.h"

#include <stdlib.h>

#include <vector>

#include "base/command_line.h"
#include "base/process_util.h"

namespace base {

uint8_t* BGRAToRGBA(const uint8_t* pixels, int width, int height, int stride) {
  if (stride == 0)
    stride = width * 4;

  uint8_t* new_pixels = static_cast<uint8_t*>(malloc(height * stride));

  // We have to copy the pixels and swap from BGRA to RGBA.
  for (int i = 0; i < height; ++i) {
    for (int j = 0; j < width; ++j) {
      int idx = i * stride + j * 4;
      new_pixels[idx] = pixels[idx + 2];
      new_pixels[idx + 1] = pixels[idx + 1];
      new_pixels[idx + 2] = pixels[idx];
      new_pixels[idx + 3] = pixels[idx + 3];
    }
  }

  return new_pixels;
}

// We use this static string to hold the Linux distro info. If we
// crash, the crash handler code will send this in the crash dump.
std::string linux_distro = "Unknown";

std::string GetLinuxDistro() {
  static bool checked_distro = false;
  if (!checked_distro) {
    std::vector<std::string> argv;
    argv.push_back("lsb_release");
    argv.push_back("-d");
    std::string output;
    base::GetAppOutput(CommandLine(argv), &output);
    if (output.length() > 0) {
      // lsb_release -d should return: Description:<tab>Distro Info
      static const std::string field = "Description:\t";
      if (output.compare(0, field.length(), field) == 0)
        linux_distro = output.substr(field.length());
    }
    // We do this check only once per process. If it fails, there's
    // little reason to believe it will work if we attempt to run
    // lsb_release again.
    checked_distro = true;
  }
  return linux_distro;
}

}  // namespace base
