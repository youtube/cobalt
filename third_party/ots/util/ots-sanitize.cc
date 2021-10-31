// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "test-context.h"

namespace {

bool g_quiet = false;


int Log(const std::string& msg) {
  if (!g_quiet)
    std::cout << msg << std::endl;
  return 0;
}

int Error(const std::string& msg) {
  if (!g_quiet)
    std::cerr << msg << std::endl;
  return 1;
}

class FileStream : public ots::OTSStream {
 public:
  explicit FileStream(std::string& filename)
      : file_(false), off_(0) {
    if (!filename.empty()) {
      stream_.open(filename.c_str(), std::ofstream::out | std::ofstream::binary);
      file_ = true;
    }
  }

  bool WriteRaw(const void *data, size_t length) {
    off_ += length;
    if (file_) {
      stream_.write(static_cast<const char*>(data), length);
      return stream_.good();
    }
    return true;
  }

  bool Seek(off_t position) {
    off_ = position;
    if (file_) {
      stream_.seekp(position);
      return stream_.good();
    }
    return true;
  }

  off_t Tell() const {
    return off_;
  }

 private:
  std::ofstream stream_;
  bool file_;
  off_t off_;
};

int Usage(const std::string& name) {
  return Error("Usage: " + name + " [options] font_file [dest_font_file] [font_index]");
}

}  // namespace

int main(int argc, char **argv) {
  std::string in_filename;
  std::string out_filename;
  int font_index = -1;

  for (int i = 1; i < argc; i++) {
    std::string arg(argv[i]);
    if (arg.at(0) == '-') {
      if (arg == "--version")
        return Log(PACKAGE_STRING);
      else if (arg == "--quiet")
        g_quiet = true;
      else
        return Error("Unrecognized option: " + arg);
    }
    else if (in_filename.empty())
      in_filename = arg;
    else if (out_filename.empty())
      out_filename = arg;
    else if (font_index == -1)
      font_index = std::strtol(arg.c_str(), NULL, 10);
    else
      return Error("Unrecognized argument: " + arg);
  }

  if (in_filename.empty())
    return Usage(argv[0]);

  std::ifstream ifs(in_filename.c_str(), std::ifstream::binary);
  if (!ifs.good())
    return Error("Failed to open: " + in_filename);

  std::vector<uint8_t> in((std::istreambuf_iterator<char>(ifs)),
                          (std::istreambuf_iterator<char>()));

  ots::TestContext context(g_quiet ? -1 : 4);

  FileStream output(out_filename);
  const bool result = context.Process(&output, in.data(), in.size(), font_index);

  if (result)
    Log("File sanitized successfully!");
  else
    Log("Failed to sanitize file!");

  return !result;
}
