// Copyright 2015 Google Inc. All Rights Reserved.
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
#include "base/file_util.h"
#include "base/file_util_proxy.h"
#include "base/location.h"
#include "base/path_service.h"
#include "cobalt/base/cobalt_paths.h"

namespace cobalt {
namespace loader {

FileFetcher::FileFetcher(const FilePath& file_path, Handler* handler,
                         const Options& options)
    : Fetcher(handler),
      buffer_size_(options.buffer_size),
      file_(base::kInvalidPlatformFileValue),
      file_offset_(options.start_offset),
      bytes_left_to_read_(options.bytes_to_read),
      message_loop_proxy_(options.message_loop_proxy),
      file_path_(file_path),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  DCHECK_GT(buffer_size_, 0);

  // Ensure the request does not attempt to navigate outside the whitelisted
  // directory.
  if (file_path_.ReferencesParent() || file_path_.IsAbsolute()) {
    handler->OnError(this, PlatformFileErrorToString(
                               base::PLATFORM_FILE_ERROR_ACCESS_DENIED));
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
  DCHECK(thread_checker_.CalledOnValidThread());
  CloseFile();
}

void FileFetcher::BuildSearchPath(const FilePath& extra_search_dir) {
  // Build the vector of paths to search for files.
  // Paths will be tried in the order they are listed.
  // Add the user-specified extra directory first, if specified,
  // so it has precendence.
  if (!extra_search_dir.empty()) {
    search_path_.push_back(extra_search_dir);
  }

  FilePath search_dir;
  PathService::Get(paths::DIR_COBALT_WEB_ROOT, &search_dir);
  search_path_.push_back(search_dir);

// We also search DIR_SOURCE_ROOT in non-release builds.
#if defined(ENABLE_DIR_SOURCE_ROOT_ACCESS)
  PathService::Get(base::DIR_SOURCE_ROOT, &search_dir);
  search_path_.push_back(search_dir);
#endif  // ENABLE_DIR_SOURCE_ROOT_ACCESS

// We also search DIR_TEST_DATA in non-release builds.
#if defined(ENABLE_TEST_DATA)
  PathService::Get(base::DIR_TEST_DATA, &search_dir);
  search_path_.push_back(search_dir);
#endif  // ENABLE_TEST_DATA
}

void FileFetcher::TryFileOpen() {
  // Append the file path to the current search path entry and try.
  FilePath actual_file_path;
  actual_file_path = curr_search_path_iter_->Append(file_path_);

  base::FileUtilProxy::CreateOrOpen(
      message_loop_proxy_, actual_file_path,
      base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ,
      base::Bind(&FileFetcher::DidCreateOrOpen,
                 weak_ptr_factory_.GetWeakPtr()));
}

void FileFetcher::ReadNextChunk() {
  int32 bytes_to_read = buffer_size_;
  if (bytes_to_read > bytes_left_to_read_) {
    bytes_to_read = static_cast<int32>(bytes_left_to_read_);
  }
  base::FileUtilProxy::Read(
      message_loop_proxy_, file_, file_offset_, bytes_to_read,
      base::Bind(&FileFetcher::DidRead, weak_ptr_factory_.GetWeakPtr()));
}

void FileFetcher::CloseFile() {
  if (file_ != base::kInvalidPlatformFileValue) {
    base::ClosePlatformFile(file_);
    file_ = base::kInvalidPlatformFileValue;
  }
}

const char* FileFetcher::PlatformFileErrorToString(
    base::PlatformFileError error) {
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
    case base::PLATFORM_FILE_OK:
      return kPlatformFileOk;
    case base::PLATFORM_FILE_ERROR_NOT_FOUND:
      return kPlatformFileErrorNotFound;
    case base::PLATFORM_FILE_ERROR_IN_USE:
      return kPlatformFileErrorInUse;
    case base::PLATFORM_FILE_ERROR_ACCESS_DENIED:
      return kPlatformFileErrorAccessDenied;
    case base::PLATFORM_FILE_ERROR_SECURITY:
      return kPlatformFileErrorSecurity;
    case base::PLATFORM_FILE_ERROR_INVALID_URL:
      return kPlatformFileErrorInvalidUrl;
    case base::PLATFORM_FILE_ERROR_ABORT:
      return kPlatformFileErrorAbort;
    case base::PLATFORM_FILE_ERROR_NOT_A_FILE:
      return kPlatformFileErrorNotAFile;
    case base::PLATFORM_FILE_ERROR_FAILED:
    case base::PLATFORM_FILE_ERROR_EXISTS:
    case base::PLATFORM_FILE_ERROR_TOO_MANY_OPENED:
    case base::PLATFORM_FILE_ERROR_NO_MEMORY:
    case base::PLATFORM_FILE_ERROR_NO_SPACE:
    case base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY:
    case base::PLATFORM_FILE_ERROR_INVALID_OPERATION:
    case base::PLATFORM_FILE_ERROR_NOT_EMPTY:
    case base::PLATFORM_FILE_ERROR_MAX:
    default:
      break;
  }
  return kPlatformFileErrorNotDefined;
}

void FileFetcher::DidCreateOrOpen(base::PlatformFileError error,
                                  base::PassPlatformFile file,
                                  bool /*created*/) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (error != base::PLATFORM_FILE_OK) {
    // File could not be opened at the current search path entry.
    // Try the next, or if we've searched the whole path, signal error.
    if (++curr_search_path_iter_ != search_path_.end()) {
      TryFileOpen();
    } else {
      handler()->OnError(this, PlatformFileErrorToString(error));
    }
    return;
  }

  file_ = file.ReleaseValue();
  ReadNextChunk();
}

void FileFetcher::DidRead(base::PlatformFileError error, const char* data,
                          int num_bytes_read) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (error != base::PLATFORM_FILE_OK) {
    handler()->OnError(this, PlatformFileErrorToString(error));
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
