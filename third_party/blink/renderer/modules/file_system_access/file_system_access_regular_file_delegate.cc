// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/file_system_access_regular_file_delegate.h"

#include "base/files/file.h"
#include "base/files/file_error_or.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/checked_math.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_capacity_allocation_host.mojom-blink.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_handle.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/file_system_access/file_system_access_capacity_tracker.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#endif

namespace blink {

FileSystemAccessFileDelegate* FileSystemAccessFileDelegate::Create(
    ExecutionContext* context,
    mojom::blink::FileSystemAccessRegularFilePtr regular_file) {
  base::File backing_file = std::move(regular_file->os_file);
  int64_t backing_file_size = regular_file->file_size;
  mojo::PendingRemote<mojom::blink::FileSystemAccessCapacityAllocationHost>
      capacity_allocation_host_remote =
          std::move(regular_file->capacity_allocation_host);
  return MakeGarbageCollected<FileSystemAccessRegularFileDelegate>(
      context, std::move(backing_file), backing_file_size,
      std::move(capacity_allocation_host_remote),
      base::PassKey<FileSystemAccessFileDelegate>());
}

FileSystemAccessRegularFileDelegate::FileSystemAccessRegularFileDelegate(
    ExecutionContext* context,
    base::File backing_file,
    int64_t backing_file_size,
    mojo::PendingRemote<mojom::blink::FileSystemAccessCapacityAllocationHost>
        capacity_allocation_host_remote,
    base::PassKey<FileSystemAccessFileDelegate>)
    :
#if BUILDFLAG(IS_MAC)
      context_(context),
      file_utilities_host_(context),
#endif  // BUILDFLAG(IS_MAC)
      backing_file_(std::move(backing_file)),
      capacity_tracker_(MakeGarbageCollected<FileSystemAccessCapacityTracker>(
          context,
          std::move(capacity_allocation_host_remote),
          backing_file_size,
          base::PassKey<FileSystemAccessRegularFileDelegate>())),
      task_runner_(context->GetTaskRunner(TaskType::kStorage)) {
}

base::FileErrorOr<int> FileSystemAccessRegularFileDelegate::Read(
    int64_t offset,
    base::span<uint8_t> data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_GE(offset, 0);

  int size = base::checked_cast<int>(data.size());
  int result =
      backing_file_.Read(offset, reinterpret_cast<char*>(data.data()), size);
  if (result >= 0) {
    return result;
  }
  return base::unexpected(base::File::GetLastFileError());
}

base::FileErrorOr<int> FileSystemAccessRegularFileDelegate::Write(
    int64_t offset,
    const base::span<uint8_t> data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_GE(offset, 0);

  int write_size = base::checked_cast<int>(data.size());

  int64_t write_end_offset;
  if (!base::CheckAdd(offset, write_size).AssignIfValid(&write_end_offset)) {
    return base::unexpected(base::File::FILE_ERROR_NO_SPACE);
  }

  int64_t file_size_before = backing_file_.GetLength();
  if (write_end_offset > file_size_before) {
    // Attempt to pre-allocate quota. Do not attempt to write unless we have
    // enough quota for the whole operation.
    if (!capacity_tracker_->RequestFileCapacityChangeSync(write_end_offset))
      return base::unexpected(base::File::FILE_ERROR_NO_SPACE);
  }

  int result = backing_file_.Write(offset, reinterpret_cast<char*>(data.data()),
                                   write_size);
  // The file size may not have changed after the write operation. `CheckAdd()`
  // is not needed here since `result` is guaranteed to be no more than
  // `write_size`.
  int64_t new_file_size = std::max(file_size_before, offset + result);
  capacity_tracker_->CommitFileSizeChange(new_file_size);

  // Only return an error if no bytes were written. Partial writes should return
  // the number of bytes written.
  return result < 0 ? base::File::GetLastFileError() : result;
}

base::FileErrorOr<int64_t> FileSystemAccessRegularFileDelegate::GetLength() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int64_t length = backing_file_.GetLength();

  // If the length is negative, the file operation failed.
  return length >= 0 ? length : base::File::GetLastFileError();
}

base::FileErrorOr<bool> FileSystemAccessRegularFileDelegate::SetLength(
    int64_t new_length) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_GE(new_length, 0);

  if (!capacity_tracker_->RequestFileCapacityChangeSync(new_length))
    return base::unexpected(base::File::FILE_ERROR_NO_SPACE);

#if BUILDFLAG(IS_MAC)
  // On macOS < 10.15, a sandboxing limitation causes failures in ftruncate()
  // syscalls issued from renderers. For this reason, base::File::SetLength()
  // fails in the renderer. We work around this problem by calling ftruncate()
  // in the browser process. See https://crbug.com/1084565.
  if (!base::mac::IsAtLeastOS10_15()) {
    if (!file_utilities_host_.is_bound()) {
      context_->GetBrowserInterfaceBroker().GetInterface(
          file_utilities_host_.BindNewPipeAndPassReceiver(task_runner_));
    }
    bool result;
    file_utilities_host_->SetLength(std::move(backing_file_), new_length,
                                    &backing_file_, &result);
    if (result) {
      capacity_tracker_->CommitFileSizeChange(new_length);
      return true;
    }
    // Unfortunately we don't have access to the error code when using
    // the FileUtilitiesHost, so we can say the operation failed but
    // not why (ex: out of quota).
    return base::unexpected(base::File::Error::FILE_ERROR_FAILED);
  }
#endif  // BUILDFLAG(IS_MAC)

  if (backing_file_.SetLength(new_length)) {
    capacity_tracker_->CommitFileSizeChange(new_length);
    return true;
  }
  return base::unexpected(base::File::GetLastFileError());
}

bool FileSystemAccessRegularFileDelegate::Flush() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return backing_file_.Flush();
}

void FileSystemAccessRegularFileDelegate::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backing_file_.Close();
}

}  // namespace blink
