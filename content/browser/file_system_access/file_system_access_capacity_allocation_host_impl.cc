// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_capacity_allocation_host_impl.h"

#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "storage/browser/file_system/file_system_util.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_manager_proxy.h"

namespace content {

FileSystemAccessCapacityAllocationHostImpl::
    FileSystemAccessCapacityAllocationHostImpl(
        FileSystemAccessManagerImpl* manager,
        const storage::FileSystemURL& url,
        base::PassKey<FileSystemAccessAccessHandleHostImpl> pass_key,
        mojo::PendingReceiver<
            blink::mojom::FileSystemAccessCapacityAllocationHost> receiver,
        int64_t file_size)
    : manager_(manager),
      url_(url),
      receiver_(this, std::move(receiver)),
      granted_capacity_(file_size) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(manager_);
  // base::Unretained is safe here because this
  // FileSystemAccessCapacityAllocationHostImpl owns `receiver_`. So, the
  // unretained FileSystemAccessCapacityAllocationHostImpl is guaranteed to
  // outlive `receiver_` and the closure that it uses.
  receiver_.set_disconnect_handler(base::BindOnce(
      &FileSystemAccessCapacityAllocationHostImpl::OnReceiverDisconnect,
      base::Unretained(this)));
}

// Constructor for testing.
FileSystemAccessCapacityAllocationHostImpl::
    FileSystemAccessCapacityAllocationHostImpl(
        FileSystemAccessManagerImpl* manager,
        const storage::FileSystemURL& url,
        base::PassKey<FileSystemAccessCapacityAllocationHostImplTest> pass_key,
        mojo::PendingReceiver<
            blink::mojom::FileSystemAccessCapacityAllocationHost> receiver,
        int64_t file_size)
    : manager_(manager),
      url_(url),
      receiver_(this, std::move(receiver)),
      granted_capacity_(file_size) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(manager_);
  // base::Unretained is safe here because this
  // FileSystemAccessCapacityAllocationHostImpl owns `receiver_`. So, the
  // unretained FileSystemAccessCapacityAllocationHostImpl is guaranteed to
  // outlive `receiver_` and the closure that it uses.
  receiver_.set_disconnect_handler(base::BindOnce(
      &FileSystemAccessCapacityAllocationHostImpl::OnReceiverDisconnect,
      base::Unretained(this)));
}

FileSystemAccessCapacityAllocationHostImpl::
    ~FileSystemAccessCapacityAllocationHostImpl() = default;

void FileSystemAccessCapacityAllocationHostImpl::OnReceiverDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  receiver_.reset();
}

void FileSystemAccessCapacityAllocationHostImpl::RequestCapacityChange(
    int64_t capacity_delta,
    RequestCapacityChangeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (capacity_delta < 0) {
    int64_t new_granted_capacity;
    bool usage_within_bounds = base::CheckAdd(granted_capacity_, capacity_delta)
                                   .AssignIfValid(&new_granted_capacity);
    if (!usage_within_bounds || new_granted_capacity < 0) {
      mojo::ReportBadMessage(
          "A file's size cannot be negative or out of bounds");
      std::move(callback).Run(0);
      return;
    }
  }

  quota_manager_proxy()->GetUsageAndQuota(
      url_.storage_key(), blink::mojom::StorageType::kTemporary,
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(
          &FileSystemAccessCapacityAllocationHostImpl::DidGetUsageAndQuota,
          weak_factory_.GetWeakPtr(), capacity_delta, std::move(callback)));
}

void FileSystemAccessCapacityAllocationHostImpl::DidGetUsageAndQuota(
    int64_t capacity_delta,
    RequestCapacityChangeCallback callback,
    blink::mojom::QuotaStatusCode status,
    int64_t usage,
    int64_t quota) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int64_t new_usage;
  bool usage_within_bounds =
      base::CheckAdd(usage, capacity_delta).AssignIfValid(&new_usage);

  if (status != blink::mojom::QuotaStatusCode::kOk || !usage_within_bounds ||
      new_usage > quota) {
    std::move(callback).Run(0);
    return;
  }
  granted_capacity_ += capacity_delta;
  quota_manager_proxy()->NotifyBucketModified(
      storage::QuotaClientType::kFileSystem, *url_.bucket(), capacity_delta,
      base::Time::Now(), base::SequencedTaskRunner::GetCurrentDefault(),
      base::DoNothing());
  std::move(callback).Run(capacity_delta);
}

}  // namespace content
