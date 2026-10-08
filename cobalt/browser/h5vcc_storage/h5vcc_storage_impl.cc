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
#include <array>
#include <memory>
#include <optional>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_enumerator.h"
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
#include "starboard/configuration_constants.h"
#include "starboard/system.h"

namespace h5vcc_storage {

namespace {

#if BUILDFLAG(USE_EVERGREEN)
const char kCrashpadDBName[] = "crashpad_database";
#endif

#if !BUILDFLAG(IS_ANDROID)
constexpr char kTestFileName[] = "cache_test_file.json";
constexpr uint32_t kBufferSizeBytes = 16 * 1024;

base::FilePath GetCacheDirectory() {
  std::vector<char> cache_dir(kSbFileMaxPath + 1, 0);
  if (!SbSystemGetPath(kSbSystemPathCacheDirectory, cache_dir.data(),
                       kSbFileMaxPath)) {
    return base::FilePath();
  }
  return base::FilePath(cache_dir.data());
}

// Deletes everything inside `dir` (files, symlinks and subdirectories) while
// leaving `dir` itself in place. Unlike base::DeletePathRecursively(), which
// also deletes `dir`. A non-existent `dir` is treated as already empty. Returns
// false if `dir` could not be enumerated or if any entry could not be deleted.
bool DeleteDirectoryContents(const base::FilePath& dir) {
  if (!base::DirectoryExists(dir)) {
    return true;
  }

  bool success = true;
  base::FileEnumerator enumerator(
      dir, /*recursive=*/false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES |
          base::FileEnumerator::SHOW_SYM_LINKS,
      base::FilePath::StringType(),
      base::FileEnumerator::FolderSearchPolicy::ALL,
      base::FileEnumerator::ErrorPolicy::STOP_ENUMERATION);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    // base::DeletePathRecursively() does not follow symlinks, so a symlink to
    // a directory is unlinked rather than having its target emptied.
    if (!base::DeletePathRecursively(path)) {
      DLOG(ERROR) << "Failed to delete " << path.value();
      success = false;
    }
  }
  if (enumerator.GetError() != base::File::FILE_OK) {
    DLOG(ERROR) << "Failed to enumerate " << dir.value() << ": "
                << base::File::ErrorToString(enumerator.GetError());
    return false;
  }
  return success;
}
#endif  // !BUILDFLAG(IS_ANDROID)

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
#if BUILDFLAG(IS_ANDROID)
  std::move(callback).Run(std::nullopt,
                          "WriteTest is not supported on Android.");
#else
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::optional<int32_t> bytes_written;
  std::optional<std::string> error;

  base::FilePath cache_path = GetCacheDirectory();
  if (cache_path.empty()) {
    error = "Failed to get cache directory path.";
    std::move(callback).Run(bytes_written, error);
    return;
  }

  // Make sure the cache directory exists, then empty it in place so that the
  // directory itself (and its mode/ownership) is left untouched.
  if (!base::CreateDirectory(cache_path)) {
    error = "Failed to create cache directory.";
    std::move(callback).Run(bytes_written, error);
    return;
  }
  if (!DeleteDirectoryContents(cache_path)) {
    error = "Failed to clear cache directory.";
    std::move(callback).Run(bytes_written, error);
    return;
  }

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
    const auto current_bytes_written = test_file.WriteAtCurrentPosNoBestEffort(
        base::as_byte_span(write_buffer));
    if (!current_bytes_written.has_value() || current_bytes_written <= 0) {
      base::DeleteFile(test_file_path);
      error = "SbWrite -1 return value error";
      std::move(callback).Run(bytes_written, error);
      return;
    }
    total_bytes_written += *current_bytes_written;
  } while (total_bytes_written < test_size);

  test_file.Flush();

  bytes_written = total_bytes_written;
  std::move(callback).Run(bytes_written, error);
#endif  // !BUILDFLAG(IS_ANDROID)
}

void H5vccStorageImpl::VerifyTest(uint32_t test_size,
                                  const std::string& test_string,
                                  VerifyTestCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if BUILDFLAG(IS_ANDROID)
  std::move(callback).Run(std::nullopt,
                          "VerifyTest is not supported on Android.",
                          /*verified=*/false);
#else
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::optional<int32_t> bytes_read;
  std::optional<std::string> error;
  bool verified = false;

  base::FilePath cache_path = GetCacheDirectory();
  if (cache_path.empty()) {
    error = "Failed to get cache directory path.";
    std::move(callback).Run(bytes_read, error, verified);
    return;
  }

  base::FilePath test_file_path = cache_path.Append(kTestFileName);
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

  std::array<unsigned char, kBufferSizeBytes> read_buffer;
  do {
    const auto current_bytes_read =
        test_file.ReadAtCurrentPosNoBestEffort(read_buffer);
    if (!current_bytes_read.has_value() || current_bytes_read <= 0) {
      base::DeleteFile(test_file_path);
      error = "SbRead -1 return value error";
      std::move(callback).Run(bytes_read, error, verified);
      return;
    }

    // Verify `read_buffer` equivalent to a repeated `test_string`.
    for (size_t i = 0; i < *current_bytes_read; ++i) {
      if (read_buffer[i] !=
          test_string[(total_bytes_read + i) % test_string.size()]) {
        base::DeleteFile(test_file_path);
        error = "File test data does not match with test data string";
        std::move(callback).Run(bytes_read, error, verified);
        return;
      }
    }

    total_bytes_read += *current_bytes_read;
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
#endif  // !BUILDFLAG(IS_ANDROID)
}

}  // namespace h5vcc_storage
