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

#include "cobalt/browser/h5vcc_storage/h5vcc_storage_impl.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#if BUILDFLAG(USE_EVERGREEN)
#include "starboard/configuration_constants.h"  // nogncheck
#endif

namespace h5vcc_storage {

namespace {

#if BUILDFLAG(USE_EVERGREEN)
const char kCrashpadDBName[] = "crashpad_database";
#endif
constexpr char kTestFileName[] = "cache_test_file.json";
constexpr uint32_t kBufferSize = 16 * 1024;

}  // namespace

H5vccStorageImpl::H5vccStorageImpl(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<mojom::H5vccStorage> receiver)
    : content::DocumentService<mojom::H5vccStorage>(render_frame_host,
                                                    std::move(receiver)) {
  DETACH_FROM_THREAD(thread_checker_);
}

H5vccStorageImpl::~H5vccStorageImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void H5vccStorageImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::H5vccStorage> receiver) {
  new H5vccStorageImpl(*render_frame_host, std::move(receiver));
}

void H5vccStorageImpl::ClearCrashpadDatabase(
    ClearCrashpadDatabaseCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if BUILDFLAG(USE_EVERGREEN)
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
  sequenced_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<base::SingleThreadTaskRunner> task_runner,
             ClearCrashpadDatabaseCallback callback) {
            std::vector<char> cache_dir(kSbFileMaxPath + 1, 0);
            if (SbSystemGetPath(kSbSystemPathCacheDirectory, cache_dir.data(),
                                kSbFileMaxPath)) {
              base::FilePath crashpad_db_dir =
                  base::FilePath(cache_dir.data()).Append(kCrashpadDBName);
              if (!base::DeletePathRecursively(crashpad_db_dir)) {
                DLOG(ERROR) << "Failed to delete crashpad directory: "
                            << crashpad_db_dir.value();
              }
              if (!base::CreateDirectory(crashpad_db_dir)) {
                DLOG(ERROR) << "Failed to recreate crashpad directory: "
                            << crashpad_db_dir.value();
              }
            } else {
              DLOG(ERROR) << "Failed to get cache directory path.";
            }
            task_runner->PostTask(FROM_HERE, std::move(callback));
          },
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          std::move(callback)));
#else
  std::move(callback).Run();
#endif  // BUILDFLAG(USE_EVERGREEN)
}

void H5vccStorageImpl::WriteTest(uint32_t test_size,
                                 const std::string& test_string,
                                 WriteTestCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::optional<int32_t> bytes_written;
  std::optional<std::string> error;

  std::vector<char> cache_dir(kSbFileMaxPath + 1, 0);
  if (!SbSystemGetPath(kSbSystemPathCacheDirectory, cache_dir.data(),
                       kSbFileMaxPath)) {
    error = "Failed to get cache directory path.";
    std::move(callback).Run(bytes_written, error);
    return;
  }
  base::FilePath cache_path = base::FilePath(cache_dir.data());

  if (!base::DeletePathRecursively(cache_path)) {
    error = "Failed to delete cache directory.";
    std::move(callback).Run(bytes_written, error);
    return;
  };
  if (!base::CreateDirectory(cache_path)) {
    error = "Failed to create cache directory.";
    std::move(callback).Run(bytes_written, error);
    return;
  };

  base::FilePath test_file_path = cache_path.Append(kTestFileName);
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
    auto current_bytes_written = test_file.WriteAtCurrentPosNoBestEffort(
        write_buffer.data() + total_bytes_written,
        std::min(kBufferSize, test_size - total_bytes_written));
    if (current_bytes_written <= 0) {
      base::DeleteFile(test_file_path);
      error = "SbWrite -1 return value error";
      std::move(callback).Run(bytes_written, error);
      return;
    }
    total_bytes_written += current_bytes_written;
  } while (total_bytes_written < test_size);

  test_file.Flush();

  bytes_written = total_bytes_written;
  std::move(callback).Run(bytes_written, error);
}

void H5vccStorageImpl::VerifyTest(uint32_t test_size,
                                  const std::string& test_string,
                                  VerifyTestCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::optional<int32_t> bytes_read;
  std::optional<std::string> error;
  bool verified = false;

  std::vector<char> cache_dir(kSbFileMaxPath + 1, 0);
  if (!SbSystemGetPath(kSbSystemPathCacheDirectory, cache_dir.data(),
                       kSbFileMaxPath)) {
    error = "Failed to get cache directory path.";
    std::move(callback).Run(bytes_read, error, verified);
    return;
  }

  base::FilePath test_file_path =
      base::FilePath(cache_dir.data()).Append(kTestFileName);
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
    auto current_bytes_read = test_file.ReadAtCurrentPosNoBestEffort(
        read_buffer.get(), std::min(kBufferSize, test_size - total_bytes_read));
    if (current_bytes_read <= 0) {
      base::DeleteFile(test_file_path);
      error = "SbRead -1 return value error";
      std::move(callback).Run(bytes_read, error, verified);
      return;
    }

    // Verify `read_buffer` equivalent to a repeated `test_string`.
    for (auto i = 0; i < current_bytes_read; ++i) {
      if (read_buffer.get()[i] !=
          test_string[(total_bytes_read + i) % test_string.size()]) {
        error = "File test data does not match with test data string";
        std::move(callback).Run(bytes_read, error, verified);
        return;
      }
    }

    total_bytes_read += current_bytes_read;
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

}  // namespace h5vcc_storage
