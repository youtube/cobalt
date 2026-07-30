// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/iamf_tools/src/iamf/cli/tests/portable/get_test_path.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"

namespace iamf_tools {

std::string GetRunfilesPath(std::string_view path) {
  base::FilePath src_root;
  if (base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_root)) {
    std::string resolved_path =
        src_root.AppendASCII("third_party/iamf_tools/src")
            .AppendASCII(std::string(path))
            .AsUTF8Unsafe();
    return resolved_path;
  }
  // Fallback to current path if PathService fails.
  return (std::filesystem::current_path() / path).string();
}

}  // namespace iamf_tools
