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

#include "starboard/tools/file_utils/file_utils.h"

#include <stdio.h>

#include <cstdint>
#include <iostream>
#include <vector>

namespace starboard {

bool ReadFile(const char* file_path, std::vector<char>& buffer) {
  FILE* file;
  if ((file = fopen(file_path, "rb")) == nullptr) {
    std::cerr << "Failed to open " << file_path << "\n";
    return false;
  }

  if (fseek(file, 0L, SEEK_END) != 0) {
    std::cerr << "Failed to seek in " << file_path << "\n";
    fclose(file);
    return false;
  }
  std::int64_t file_size_signed;
  if ((file_size_signed = ftell(file)) == -1L) {
    std::cerr << "Failed to get size of " << file_path << "\n";
    fclose(file);
    return false;
  }
  rewind(file);
  const size_t file_size = static_cast<size_t>(file_size_signed);
  if (file_size == 0) {
    std::cerr << "File is empty: " << file_path << "\n";
    fclose(file);
    return false;
  }

  buffer.resize(file_size);
  size_t result;
  if ((result = fread(buffer.data(), sizeof(char), file_size, file)) !=
      file_size) {
    std::cerr << "Expected to read " << file_size << " bytes from " << file_path
              << " but only read " << result << "\n";
    fclose(file);
    return false;
  }

  fclose(file);
  return true;
}

bool WriteFile(const char* file_path, const std::vector<char>& buffer) {
  if (buffer.empty()) {
    std::cerr << "Buffer is empty for " << file_path << "\n";
    return false;
  }

  FILE* file;
  if ((file = fopen(file_path, "wb")) == nullptr) {
    std::cerr << "Failed to open " << file_path << "\n";
    return false;
  }

  size_t result;
  if ((result = fwrite(buffer.data(), sizeof(char), buffer.size(), file)) !=
      buffer.size()) {
    std::cerr << "Expected to write " << buffer.size() << " bytes to "
              << file_path << " but only wrote " << result << "\n";
    fclose(file);
    return false;
  }

  fclose(file);
  return true;
}

}  // namespace starboard
