// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <iostream>
#include <vector>

#include "starboard/tools/file_utils/file_utils.h"
#include "starboard/tools/zstd_compress/zstd_compress.h"

int main(int argc, char* argv[]) {
  if (argc != 3) {
    std::cerr << "Usage: zstd_compress <source> <destination>\n";
    return 1;
  }

  std::vector<char> src_buffer;
  if (!starboard::ReadFile(argv[1], src_buffer)) {
    std::cerr << "Failed to read the src file\n";
    return 1;
  }

  std::vector<char> dst_buffer;
  if (!starboard::zstd_compress::Compress(src_buffer, dst_buffer)) {
    std::cerr << "Failed to compress\n";
    return 1;
  }

  if (!starboard::WriteFile(argv[2], dst_buffer)) {
    std::cerr << "Failed to write the dst file\n";
    return 1;
  }

  return 0;
}
