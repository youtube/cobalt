// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/idb_request_loader.h"

#include <algorithm>

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_loader.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_request.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_request_queue_item.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_value_wrapping.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

IDBRequestLoader::IDBRequestLoader(
    IDBRequestQueueItem* queue_item,
    Vector<std::unique_ptr<IDBValue>>& result_values)
    : queue_item_(queue_item), values_(result_values) {
  DCHECK(IDBValueUnwrapper::IsWrapped(*values_));
}

IDBRequestLoader::~IDBRequestLoader() {}

void IDBRequestLoader::Start() {
#if DCHECK_IS_ON()
  DCHECK(!started_) << "Start() was already called";
  started_ = true;
#endif  // DCHECK_IS_ON()

  // TODO(pwnall): Start() / StartNextValue() unwrap large values sequentially.
  //               Consider parallelizing. The main issue is that the Blob reads
  //               will have to be throttled somewhere, and the extra complexity
  //               only benefits applications that use getAll().
  current_value_ = values_->begin();
  StartNextValue();
}

void IDBRequestLoader::Cancel() {
#if DCHECK_IS_ON()
  DCHECK(started_) << "Cancel() called on a loader that hasn't been Start()ed";
  DCHECK(!canceled_) << "Cancel() was already called";
  canceled_ = true;

  DCHECK(file_reader_loading_);
  file_reader_loading_ = false;
#endif  // DCHECK_IS_ON()
  if (loader_)
    loader_->Cancel();
}

void IDBRequestLoader::StartNextValue() {
  IDBValueUnwrapper unwrapper;

  while (true) {
    if (current_value_ == values_->end()) {
      ReportSuccess();
      return;
    }
    if (unwrapper.Parse(current_value_->get()))
      break;
    ++current_value_;
  }

  DCHECK(current_value_ != values_->end());

  ExecutionContext* exection_context =
      queue_item_->Request()->GetExecutionContext();
  // The execution context was torn down. The loader will eventually get a
  // Cancel() call.
  if (!exection_context)
    return;

  wrapped_data_.reserve(unwrapper.WrapperBlobSize());
#if DCHECK_IS_ON()
  DCHECK(!file_reader_loading_);
  file_reader_loading_ = true;
#endif  // DCHECK_IS_ON()
  loader_ = MakeGarbageCollected<FileReaderLoader>(
      this, exection_context->GetTaskRunner(TaskType::kDatabaseAccess));
  loader_->Start(unwrapper.WrapperBlobHandle());
}

FileErrorCode IDBRequestLoader::DidStartLoading(uint64_t) {
  return FileErrorCode::kOK;
}

FileErrorCode IDBRequestLoader::DidReceiveData(const char* data,
                                               unsigned data_length) {
  DCHECK_LE(wrapped_data_.size() + data_length, wrapped_data_.capacity())
      << "The reader returned more data than we were prepared for";

  wrapped_data_.Append(data, data_length);
  return FileErrorCode::kOK;
}

void IDBRequestLoader::DidFinishLoading() {
#if DCHECK_IS_ON()
  DCHECK(started_)
      << "FileReaderLoader called DidFinishLoading() before it was Start()ed";
  DCHECK(!canceled_)
      << "FileReaderLoader called DidFinishLoading() after it was Cancel()ed";

  DCHECK(file_reader_loading_);
  file_reader_loading_ = false;
#endif  // DCHECK_IS_ON()

  IDBValueUnwrapper::Unwrap(SharedBuffer::AdoptVector(wrapped_data_),
                            current_value_->get());
  ++current_value_;

  StartNextValue();
}

void IDBRequestLoader::DidFail(FileErrorCode) {
#if DCHECK_IS_ON()
  DCHECK(started_)
      << "FileReaderLoader called DidFail() before it was Start()ed";
  DCHECK(!canceled_)
      << "FileReaderLoader called DidFail() after it was Cancel()ed";

  DCHECK(file_reader_loading_);
  file_reader_loading_ = false;
#endif  // DCHECK_IS_ON()
  ReportError();
}

void IDBRequestLoader::ReportSuccess() {
#if DCHECK_IS_ON()
  DCHECK(started_);
  DCHECK(!canceled_);
#endif  // DCHECK_IS_ON()
  queue_item_->OnResultLoadComplete();
}

void IDBRequestLoader::ReportError() {
#if DCHECK_IS_ON()
  DCHECK(started_);
  DCHECK(!canceled_);
#endif  // DCHECK_IS_ON()
  queue_item_->OnResultLoadComplete(MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kDataError, "Failed to read large IndexedDB value"));
}

}  // namespace blink
