// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/loader/file_fetcher.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/path_service.h"
#include "cobalt/base/cobalt_paths.h"

namespace cobalt {
namespace loader {
namespace {
const char* FileErrorToString(base::File::Error error) {
  static const char kPlatformFileOk[] = "Platform file OK";
  static const char kPlatformFileErrorNotFound[] =
      "Platform file error: Not found";
  static const char kPlatformFileErrorInUse[] = "Platform file error: In use";
  static const char kPlatformFileErrorAccessDenied[] =
      "Platform file error: Access denied";
  static const char kPlatformFileErrorSecurity[] =
      "Platform file error: Security";
  static const char kPlatformFileErrorInvalidUrl[] =
      "Platform file error: Invalid URL";
  static const char kPlatformFileErrorAbort[] = "Platform file error: Abort";
  static const char kPlatformFileErrorNotAFile[] =
      "Platform file error: Not a file";
  static const char kPlatformFileErrorNotDefined[] =
      "Platform file error: Undefined error";

  switch (error) {
    case base::File::FILE_OK:
      return kPlatformFileOk;
    case base::File::FILE_ERROR_NOT_FOUND:
      return kPlatformFileErrorNotFound;
    case base::File::FILE_ERROR_IN_USE:
      return kPlatformFileErrorInUse;
    case base::File::FILE_ERROR_ACCESS_DENIED:
      return kPlatformFileErrorAccessDenied;
    case base::File::FILE_ERROR_SECURITY:
      return kPlatformFileErrorSecurity;
    case base::File::FILE_ERROR_INVALID_URL:
      return kPlatformFileErrorInvalidUrl;
    case base::File::FILE_ERROR_ABORT:
      return kPlatformFileErrorAbort;
    case base::File::FILE_ERROR_NOT_A_FILE:
      return kPlatformFileErrorNotAFile;
    case base::File::FILE_ERROR_FAILED:
    case base::File::FILE_ERROR_EXISTS:
    case base::File::FILE_ERROR_TOO_MANY_OPENED:
    case base::File::FILE_ERROR_NO_MEMORY:
    case base::File::FILE_ERROR_NO_SPACE:
    case base::File::FILE_ERROR_NOT_A_DIRECTORY:
    case base::File::FILE_ERROR_INVALID_OPERATION:
    case base::File::FILE_ERROR_NOT_EMPTY:
    case base::File::FILE_ERROR_IO:
    case base::File::FILE_ERROR_MAX:
      break;
  }
  return kPlatformFileErrorNotDefined;
}
}  // namespace

FileFetcher::FileFetcher(const base::FilePath& file_path, Handler* handler,
                         const Options& options)
    : Fetcher(handler),
      buffer_size_(options.buffer_size),
      file_(base::kInvalidPlatformFile),
      file_offset_(options.start_offset),
      bytes_left_to_read_(options.bytes_to_read),
      task_runner_(options.message_loop_proxy),
      file_path_(file_path),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      file_proxy_(task_runner_.get()) {
  DCHECK_GT(buffer_size_, 0);

  // Ensure the request does not attempt to navigate outside the whitelisted
  // directory.
  if (file_path_.ReferencesParent() || file_path_.IsAbsolute()) {
    handler->OnError(this,
                     FileErrorToString(base::File::FILE_ERROR_ACCESS_DENIED));
    return;
  }

  // Try fetching the file from each search path entry in turn.
  // Start at the beginning. On failure, we'll try the next path entry,
  // and so on until we open the file or reach the end of the search path.
  BuildSearchPath(options.extra_search_dir);
  curr_search_path_iter_ = search_path_.begin();
  TryFileOpen();
}

FileFetcher::~FileFetcher() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (task_runner_ != base::MessageLoop::current()->task_runner()) {
    // In case we are currently in the middle of a fetch (in which case it will
    // be aborted), invalidate the weak pointers to this FileFetcher object to
    // ensure that we do not process any responses from pending file I/O, which
    // also guarantees that no new file I/O requests will be generated after the
    // current one (if one is currently active).
    weak_ptr_factory_.InvalidateWeakPtrs();
    // Then wait for any currently active file I/O to complete, after which
    // everything will be quiet and we can shutdown safely.
    task_runner_->WaitForFence();
  }
}

void FileFetcher::BuildSearchPath(const base::FilePath& extra_search_dir) {
  // Build the vector of paths to search for files.
  // Paths will be tried in the order they are listed.
  // Add the user-specified extra directory first, if specified,
  // so it has precedence.
  if (!extra_search_dir.empty()) {
    search_path_.push_back(extra_search_dir);
  }

  base::FilePath search_dir;
  base::PathService::Get(paths::DIR_COBALT_WEB_ROOT, &search_dir);
  search_path_.push_back(search_dir);

// We also search DIR_TEST_DATA in non-release builds.
#if defined(ENABLE_TEST_DATA)
  base::PathService::Get(base::DIR_TEST_DATA, &search_dir);
  search_path_.push_back(search_dir);
#endif  // ENABLE_TEST_DATA
}

void FileFetcher::TryFileOpen() {
  // Append the file path to the current search path entry and try.
  base::FilePath actual_file_path;
  actual_file_path = curr_search_path_iter_->Append(file_path_);

  file_proxy_.CreateOrOpen(actual_file_path,
                           base::File::FLAG_OPEN | base::File::FLAG_READ,
                           base::Bind(&FileFetcher::DidCreateOrOpen,
                                      weak_ptr_factory_.GetWeakPtr()));
}

void FileFetcher::ReadNextChunk() {
  int32 bytes_to_read = buffer_size_;
  if (bytes_to_read > bytes_left_to_read_) {
    bytes_to_read = static_cast<int32>(bytes_left_to_read_);
  }
  file_proxy_.Read(
      file_offset_, bytes_to_read,
      base::Bind(&FileFetcher::DidRead, weak_ptr_factory_.GetWeakPtr()));
}

void FileFetcher::DidCreateOrOpen(base::File::Error error) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (error != base::File::FILE_OK) {
    // File could not be opened at the current search path entry.
    // Try the next, or if we've searched the whole path, signal error.
    if (++curr_search_path_iter_ != search_path_.end()) {
      TryFileOpen();
    } else {
      // base::File::Error and Starboard file error can cast to each other.
      handler()->OnError(this, FileErrorToString(error));
    }
    return;
  }
  ReadNextChunk();
}

void FileFetcher::DidRead(base::File::Error error, const char* data,
                          int num_bytes_read) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (error != base::File::FILE_OK) {
    handler()->OnError(this, FileErrorToString(error));
    return;
  }

  DCHECK_LE(num_bytes_read, bytes_left_to_read_);

  if (!num_bytes_read) {
    handler()->OnDone(this);
    return;
  }

  handler()->OnReceived(this, data, static_cast<size_t>(num_bytes_read));

  bytes_left_to_read_ -= num_bytes_read;

  if (!bytes_left_to_read_) {
    handler()->OnDone(this);
    return;
  }

  file_offset_ += num_bytes_read;
  ReadNextChunk();
}

}  // namespace loader
}  // namespace cobalt
