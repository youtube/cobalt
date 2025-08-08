// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/cache_storage/cache_storage_blob_client_list.h"

#include <utility>

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/heap/prefinalizer.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"

namespace blink {

// Class implementing the BlobReaderClient interface.  This is used to
// propagate the completion of an eager body blob read to the
// DataPipeBytesConsumer.
class CacheStorageBlobClientList::Client
    : public GarbageCollected<CacheStorageBlobClientList::Client>,
      public mojom::blink::BlobReaderClient {
  // We must prevent any mojo messages from coming in after this object
  // starts getting garbage collected.
  USING_PRE_FINALIZER(CacheStorageBlobClientList::Client, Dispose);

 public:
  Client(CacheStorageBlobClientList* owner,
         ExecutionContext* context,
         mojo::PendingReceiver<mojom::blink::BlobReaderClient>
             client_pending_receiver,
         DataPipeBytesConsumer::CompletionNotifier* completion_notifier)
      : owner_(owner),
        client_receiver_(this, context),
        completion_notifier_(completion_notifier) {
    client_receiver_.Bind(std::move(client_pending_receiver),
                          context->GetTaskRunner(TaskType::kMiscPlatformAPI));
  }

  Client(const Client&) = delete;
  Client& operator=(const Client&) = delete;

  void OnCalculatedSize(uint64_t total_size,
                        uint64_t expected_content_size) override {}

  void OnComplete(int32_t status, uint64_t data_length) override {
    client_receiver_.reset();

    // 0 is net::OK
    if (status == 0)
      completion_notifier_->SignalComplete();
    else
      completion_notifier_->SignalError(BytesConsumer::Error());

    if (owner_)
      owner_->RevokeClient(this);
  }

  void Trace(Visitor* visitor) const {
    visitor->Trace(owner_);
    visitor->Trace(completion_notifier_);
    visitor->Trace(client_receiver_);
  }

 private:
  void Dispose() {
    // Use the existence of the client_receiver_ binding to see if this
    // client has already completed.
    if (!client_receiver_.is_bound())
      return;

    client_receiver_.reset();
    completion_notifier_->SignalError(BytesConsumer::Error("aborted"));

    // If we are already being garbage collected its not necessary to
    // call RevokeClient() on the owner.
  }

  WeakMember<CacheStorageBlobClientList> owner_;
  HeapMojoReceiver<mojom::blink::BlobReaderClient, Client> client_receiver_;
  Member<DataPipeBytesConsumer::CompletionNotifier> completion_notifier_;
};

void CacheStorageBlobClientList::AddClient(
    ExecutionContext* context,
    mojo::PendingReceiver<mojom::blink::BlobReaderClient>
        client_pending_receiver,
    DataPipeBytesConsumer::CompletionNotifier* completion_notifier) {
  clients_.emplace_back(MakeGarbageCollected<Client>(
      this, context, std::move(client_pending_receiver), completion_notifier));
}

void CacheStorageBlobClientList::Trace(Visitor* visitor) const {
  visitor->Trace(clients_);
}

void CacheStorageBlobClientList::RevokeClient(Client* client) {
  auto index = clients_.Find(client);
  clients_.EraseAt(index);
}

}  // namespace blink
