// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USE_HACKY_COBALT_CHANGES
#error Many definitions in this file should instead be defined in disk_cache.cc
#endif

#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/single_thread_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "build/build_config.h"
#include "net/base/cache_type.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/backend_cleanup_tracker.h"
#include "net/disk_cache/cache_util.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/memory/mem_backend_impl.h"
#include "net/disk_cache/simple/simple_backend_impl.h"
#include "net/disk_cache/simple/simple_file_enumerator.h"
#include "net/disk_cache/simple/simple_util.h"

// Completing partial implementation of disk_cache.h, in place of
// disk_cache.cc.
namespace disk_cache {

namespace {

using FileEnumerator = disk_cache::BackendFileOperations::FileEnumerator;

class TrivialFileEnumerator final : public FileEnumerator {
 public:
  using FileEnumerationEntry =
      disk_cache::BackendFileOperations::FileEnumerationEntry;

  explicit TrivialFileEnumerator(const base::FilePath& path)
      : enumerator_(path) {}
  ~TrivialFileEnumerator() override = default;

  absl::optional<FileEnumerationEntry> Next() override {
    return enumerator_.Next();
  }
  bool HasError() const override { return enumerator_.HasError(); }

 private:
  disk_cache::SimpleFileEnumerator enumerator_;
};

class UnboundTrivialFileOperations
    : public disk_cache::UnboundBackendFileOperations {
 public:
  std::unique_ptr<disk_cache::BackendFileOperations> Bind(
      scoped_refptr<base::SequencedTaskRunner> task_runner) override {
    return std::make_unique<disk_cache::TrivialFileOperations>();
  }
};

}

// Many functions below are copied from their implementation in
// //net/disk_cache/disk_cache.cc as we compile this file in place of that one.
BackendResult::BackendResult() = default;
BackendResult::~BackendResult() = default;
BackendResult::BackendResult(BackendResult&&) = default;
BackendResult& BackendResult::operator=(BackendResult&&) = default;

// static
BackendResult BackendResult::MakeError(net::Error error_in) {
  DCHECK_NE(error_in, net::OK);
  BackendResult result;
  result.net_error = error_in;
  return result;
}

// static
BackendResult BackendResult::Make(std::unique_ptr<Backend> backend_in) {
  DCHECK(backend_in);
  BackendResult result;
  result.net_error = net::OK;
  result.backend = std::move(backend_in);
  return result;
}

EntryResult::EntryResult() = default;
EntryResult::~EntryResult() = default;

EntryResult::EntryResult(EntryResult&& other) {
  net_error_ = other.net_error_;
  entry_ = std::move(other.entry_);
  opened_ = other.opened_;

  other.net_error_ = net::ERR_FAILED;
  other.opened_ = false;
}

EntryResult& EntryResult::operator=(EntryResult&& other) {
  net_error_ = other.net_error_;
  entry_ = std::move(other.entry_);
  opened_ = other.opened_;

  other.net_error_ = net::ERR_FAILED;
  other.opened_ = false;
  return *this;
}

// static
EntryResult EntryResult::MakeOpened(Entry* new_entry) {
  DCHECK(new_entry);

  EntryResult result;
  result.net_error_ = net::OK;
  result.entry_.reset(new_entry);
  result.opened_ = true;
  return result;
}

// static
EntryResult EntryResult::MakeCreated(Entry* new_entry) {
  DCHECK(new_entry);

  EntryResult result;
  result.net_error_ = net::OK;
  result.entry_.reset(new_entry);
  result.opened_ = false;
  return result;
}

// static
EntryResult EntryResult::MakeError(net::Error status) {
  DCHECK_NE(status, net::OK);

  EntryResult result;
  result.net_error_ = status;
  return result;
}

Entry* EntryResult::ReleaseEntry() {
  Entry* ret = entry_.release();
  net_error_ = net::ERR_FAILED;
  opened_ = false;
  return ret;
}

TrivialFileOperations::TrivialFileOperations() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

TrivialFileOperations::~TrivialFileOperations() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool TrivialFileOperations::CreateDirectory(const base::FilePath& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
  DCHECK(bound_);
#endif

  // This is needed for some unittests.
  if (path.empty()) {
    return false;
  }

  DCHECK(path.IsAbsolute());

  bool result = base::CreateDirectory(path);
  return result;
}

bool TrivialFileOperations::PathExists(const base::FilePath& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if DCHECK_IS_ON()
  DCHECK(bound_);
#endif

  // This is needed for some unittests.
  if (path.empty()) {
    return false;
  }

  DCHECK(path.IsAbsolute());

  bool result = base::PathExists(path);
  return result;
}

bool TrivialFileOperations::DirectoryExists(const base::FilePath& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(path.IsAbsolute());
#if DCHECK_IS_ON()
  DCHECK(bound_);
#endif

  bool result = base::DirectoryExists(path);
  return result;
}

base::File TrivialFileOperations::OpenFile(const base::FilePath& path,
                                           uint32_t flags) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(path.IsAbsolute());
#if DCHECK_IS_ON()
  DCHECK(bound_);
#endif

  base::File file(path, flags);
  return file;
}

bool TrivialFileOperations::DeleteFile(const base::FilePath& path,
                                       DeleteFileMode mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(path.IsAbsolute());
#if DCHECK_IS_ON()
  DCHECK(bound_);
#endif

  bool result = false;
  switch (mode) {
    case DeleteFileMode::kDefault:
      result = base::DeleteFile(path);
      break;
    case DeleteFileMode::kEnsureImmediateAvailability:
      result = disk_cache::simple_util::SimpleCacheDeleteFile(path);
      break;
  }
  return result;
}

bool TrivialFileOperations::ReplaceFile(const base::FilePath& from_path,
                                        const base::FilePath& to_path,
                                        base::File::Error* error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(from_path.IsAbsolute());
  DCHECK(to_path.IsAbsolute());
#if DCHECK_IS_ON()
  DCHECK(bound_);
#endif

  return base::ReplaceFile(from_path, to_path, error);
}

absl::optional<base::File::Info> TrivialFileOperations::GetFileInfo(
    const base::FilePath& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(path.IsAbsolute());
#if DCHECK_IS_ON()
  DCHECK(bound_);
#endif

  base::File::Info file_info;
  if (!base::GetFileInfo(path, &file_info)) {
    return absl::nullopt;
  }
  return file_info;
}

std::unique_ptr<FileEnumerator> TrivialFileOperations::EnumerateFiles(
    const base::FilePath& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(path.IsAbsolute());
#if DCHECK_IS_ON()
  DCHECK(bound_);
#endif
  return std::make_unique<TrivialFileEnumerator>(path);
}

void TrivialFileOperations::CleanupDirectory(
    const base::FilePath& path,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This is needed for some unittests.
  if (path.empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  DCHECK(path.IsAbsolute());
#if DCHECK_IS_ON()
  DCHECK(bound_);
#endif

  disk_cache::CleanupDirectory(path, std::move(callback));
}

std::unique_ptr<UnboundBackendFileOperations> TrivialFileOperations::Unbind() {
#if DCHECK_IS_ON()
  DCHECK(bound_);
  bound_ = false;
#endif
  return std::make_unique<UnboundTrivialFileOperations>();
}

TrivialFileOperationsFactory::TrivialFileOperationsFactory() = default;
TrivialFileOperationsFactory::~TrivialFileOperationsFactory() = default;

std::unique_ptr<BackendFileOperations> TrivialFileOperationsFactory::Create(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  return std::make_unique<TrivialFileOperations>();
}

std::unique_ptr<UnboundBackendFileOperations>
TrivialFileOperationsFactory::CreateUnbound() {
  return std::make_unique<UnboundTrivialFileOperations>();
}

BackendResult CreateCacheBackend(net::CacheType type,
                              net::BackendType backend_type,
                              scoped_refptr<BackendFileOperationsFactory> file_operations,
                              const base::FilePath& path,
                              int64_t max_bytes,
                              ResetHandling reset_handling,
                              net::NetLog* net_log,
                              BackendResultCallback callback) {
  return CreateCacheBackend(type, backend_type, std::move(file_operations), path, max_bytes, reset_handling, net_log, base::OnceClosure(), std::move(callback));
}

#if defined(OS_ANDROID)
NET_EXPORT net::Error CreateCacheBackend(
    net::CacheType type,
    net::BackendType backend_type,
    const base::FilePath& path,
    int64_t max_bytes,
    bool force,
    net::NetLog* net_log,
    std::unique_ptr<Backend>* backend,
    net::CompletionOnceCallback callback,
    base::android::ApplicationStatusListener* app_status_listener) {
  return net::ERR_FAILED;
}
#endif

BackendResult CreateCacheBackend(net::CacheType type,
                              net::BackendType backend_type,
                              scoped_refptr<BackendFileOperationsFactory> file_operations,
                              const base::FilePath& path,
                              int64_t max_bytes,
                              ResetHandling reset_handling,
                              net::NetLog* net_log,
                              base::OnceClosure post_cleanup_callback,
                              BackendResultCallback callback) {
  DCHECK(!callback.is_null());

  if (type == net::MEMORY_CACHE) {
    std::unique_ptr<MemBackendImpl> mem_backend_impl =
        disk_cache::MemBackendImpl::CreateBackend(max_bytes, net_log);
    if (mem_backend_impl) {
      mem_backend_impl->SetPostCleanupCallback(
          std::move(post_cleanup_callback));
      return BackendResult::MakeError(net::OK);
    } else {
      if (!post_cleanup_callback.is_null())
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, std::move(post_cleanup_callback));
      return BackendResult::MakeError(net::ERR_FAILED);
    }
  }
  return BackendResult::MakeError(net::ERR_FAILED);
}

void FlushCacheThreadForTesting() {
  // For simple backend.
#ifndef USE_HACKY_COBALT_CHANGES
  SimpleBackendImpl::FlushWorkerPoolForTesting();
  base::ThreadPoolInstance::GetInstance()->FlushForTesting();
#endif

  // Block backend.
  // BackendImpl::FlushForTesting();
}

int64_t Backend::CalculateSizeOfEntriesBetween(
    base::Time initial_time,
    base::Time end_time,
    Int64CompletionOnceCallback callback) {
  return net::ERR_NOT_IMPLEMENTED;
}

uint8_t Backend::GetEntryInMemoryData(const std::string& key) {
  return 0;
}

void Backend::SetEntryInMemoryData(const std::string& key, uint8_t data) {}

}  // namespace disk_cache
