// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

// This program compresses a file into a separate .lz4 file comprised of a
// single LZ4 Frame.
// It's written for a specific purpose: to produce LZ4 compressed versions of
// the Cobalt Shared Library that can be tested and released. As such, the
// encoding must remain compatible with what the client-side decompression code
// in the Loader Application expects. Ad hoc users wanting more flexibility
// should use the official LZ4 CLI instead.

#include <stdio.h>

#include <iostream>
#include <vector>

#include "third_party/lz4_lib/lz4frame.h"
#include "third_party/lz4_lib/lz4hc.h"

bool ReadFile(const char* file_path, std::vector<char>& buffer) {
  FILE* file;
  if ((file = fopen(file_path, "rb")) == NULL) {
    std::cerr << "Failed to open " << file_path << "\n";
    return false;
  }

  if (fseek(file, 0L, SEEK_END) != 0) {
    std::cerr << "Failed to seek in " << file_path << "\n";
    fclose(file);
    return false;
  }
  std::int64_t file_size;
  if ((file_size = ftell(file)) == -1L) {
    std::cerr << "Failed to get size of " << file_path << "\n";
    fclose(file);
    return false;
  }
  rewind(file);

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

bool Compress(std::vector<char>& src_buffer, std::vector<char>& dst_buffer) {
  // Based on experimentation these preferences yield a high compression ratio
  // and fast decompression speed for a diverse set of devices.
  LZ4F_preferences_t prefs = {
      {LZ4F_max256KB, LZ4F_blockIndependent, LZ4F_noContentChecksum, LZ4F_frame,
       src_buffer.size(), 0 /* no dictID */, LZ4F_noBlockChecksum},
      LZ4HC_CLEVEL_MAX,
      0,         /* autoflush */
      0,         /* favor decompression speed */
      {0, 0, 0}, /* reserved, must be set to 0 */
  };
  size_t dst_capacity = LZ4F_compressFrameBound(src_buffer.size(), &prefs);
  dst_buffer.resize(dst_capacity);

  size_t encoded_size =
      LZ4F_compressFrame(dst_buffer.data(), dst_capacity, src_buffer.data(),
                         src_buffer.size(), &prefs);
  if (LZ4F_isError(encoded_size)) {
    std::cerr << LZ4F_getErrorName(encoded_size) << "\n";
    return false;
  }
  dst_buffer.resize(encoded_size);

  return true;
}

bool WriteFile(const char* file_path, std::vector<char>& buffer) {
  FILE* file;
  if ((file = fopen(file_path, "wb")) == NULL) {
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

int main(int argc, char* argv[]) {
  if (argc != 3) {
    std::cerr << "Usage: lz4_compress <source> <destination>"
              << "\n";
    return 1;
  }

  std::vector<char> src_buffer;
  if (!ReadFile(argv[1], src_buffer)) {
    std::cerr << "Failed to read the src file"
              << "\n";
    return 1;
  }

  std::vector<char> dst_buffer;
  if (!Compress(src_buffer, dst_buffer)) {
    std::cerr << "Failed to compress"
              << "\n";
    return 1;
  }

  if (!WriteFile(argv[2], dst_buffer)) {
    std::cerr << "Failed to write the dst file"
              << "\n";
    return 1;
  }

  return 0;
}
