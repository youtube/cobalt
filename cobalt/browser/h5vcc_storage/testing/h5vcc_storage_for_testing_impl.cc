// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/browser/h5vcc_storage/testing/h5vcc_storage_for_testing_impl.h"

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/browser_context.h"

namespace h5vcc_storage_for_testing {

namespace {

constexpr char kTestFileName[] = "cache_test_file.json";
constexpr uint32_t kBufferSize = 16384;  // 16 KB

}  // namespace

// TODO (b/395126160): refactor mojom implementation on Android
H5vccStorageForTestingImpl::H5vccStorageForTestingImpl(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<mojom::H5vccStorageForTesting> receiver)
    : content::DocumentService<mojom::H5vccStorageForTesting>(
          render_frame_host,
          std::move(receiver)),
      user_data_path_(render_frame_host.GetBrowserContext()->GetPath()) {}

void H5vccStorageForTestingImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::H5vccStorageForTesting> receiver) {
  new H5vccStorageForTestingImpl(*render_frame_host, std::move(receiver));
}

void H5vccStorageForTestingImpl::WriteTest(uint32_t test_size,
                                           const std::string& test_string,
                                           WriteTestCallback callback) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  absl::optional<int32_t> bytes_written;
  absl::optional<std::string> error;

  base::DeletePathRecursively(user_data_path_);
  base::CreateDirectory(user_data_path_);

  base::FilePath test_file_path = user_data_path_.Append(kTestFileName);
  base::File test_file(test_file_path,
                       base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_WRITE);

  if (!test_file.IsValid()) {
    error = base::StringPrintf("Error while opening ScopedFile: %s",
                               test_file_path.value().c_str());
    std::move(callback).Run(bytes_written, error);
    return;
  }

  // Repeatedly write `test_string` to test_size bytes of `write_buffer`.
  std::string write_buffer;
  int iterations = test_size / test_string.length();
  for (int i = 0; i < iterations; ++i) {
    write_buffer.append(test_string);
  }
  write_buffer.append(test_string.substr(0, test_size % test_string.length()));

  // Incremental Writes of `test_string`, copies `SbWriteAll`, using a maximum
  // `kBufferSize` per write.
  uint32_t total_bytes_written = 0;

  do {
    auto bytes_written = test_file.WriteAtCurrentPosNoBestEffort(
        write_buffer.data() + total_bytes_written,
        std::min(kBufferSize, test_size - total_bytes_written));
    if (bytes_written <= 0) {
      base::DeleteFile(test_file_path);
      error = "SbWrite -1 return value error";
      std::move(callback).Run(bytes_written, error);
      return;
    }
    total_bytes_written += bytes_written;
  } while (total_bytes_written < test_size);

  test_file.Flush();

  bytes_written = total_bytes_written;
  std::move(callback).Run(bytes_written, error);
}

void H5vccStorageForTestingImpl::VerifyTest(uint32_t test_size,
                                            const std::string& test_string,
                                            VerifyTestCallback callback) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  absl::optional<int32_t> bytes_read;
  absl::optional<std::string> error;
  bool verified = false;

  base::FilePath test_file_path = user_data_path_.Append(kTestFileName);
  base::File test_file(test_file_path,
                       base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!test_file.IsValid()) {
    error = base::StringPrintf("Error while opening ScopedFile: %s",
                               test_file_path.value().c_str());
    std::move(callback).Run(bytes_read, error, verified);
    return;
  }

  // Incremental Reads of `test_string`, copies `SbReadAll`, using a maximum
  // `kBufferSize` per write.
  uint32_t total_bytes_read = 0;

  do {
    auto read_buffer = std::make_unique<char[]>(kBufferSize);
    auto bytes_read = test_file.ReadAtCurrentPosNoBestEffort(
        read_buffer.get(), std::min(kBufferSize, test_size - total_bytes_read));
    if (bytes_read <= 0) {
      base::DeleteFile(test_file_path);
      error = "SbRead -1 return value error";
      std::move(callback).Run(bytes_read, error, verified);
      return;
    }

    // Verify `read_buffer` equivalent to a repeated `test_string`.
    for (auto i = 0; i < bytes_read; ++i) {
      if (read_buffer.get()[i] !=
          test_string[(total_bytes_read + i) % test_string.size()]) {
        error = "File test data does not match with test data string";
        std::move(callback).Run(bytes_read, error, verified);
        return;
      }
    }

    total_bytes_read += bytes_read;
  } while (total_bytes_read < test_size);

  if (total_bytes_read != test_size) {
    base::DeleteFile(test_file_path);
    error = "File test data size does not match kTestDataSize";
    std::move(callback).Run(bytes_read, error, verified);
    return;
  }

  base::DeleteFile(test_file_path);
  verified = true;
  bytes_read = total_bytes_read;
  std::move(callback).Run(bytes_read, error, verified);
}

}  // namespace h5vcc_storage_for_testing
