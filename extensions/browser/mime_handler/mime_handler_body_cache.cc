// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/mime_handler/mime_handler_body_cache.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "mojo/public/cpp/system/string_data_source.h"

namespace extensions {

namespace {

// Matches network::kDefaultDataPipeAllocationSize (see
// //services/network/public/cpp/loading_params.cc): response bodies
// flow in from the network service at this ring-buffer size, so the
// forwarding and replay pipes here use the same cadence.
constexpr uint32_t kDefaultPipeCapacity = 512 * 1024;  // 512 KB

// Cap on bytes buffered for replay. Responses larger than this abandon
// the cache to protect the browser process from OOM. Mutable so tests
// can override it via `SetMaxCacheBytesForTesting()`.
size_t g_max_cache_bytes = 100u * 1024u * 1024u;  // 100 MiB

}  // namespace

// static
scoped_refptr<MimeHandlerBodyCache> MimeHandlerBodyCache::Create(
    mojo::ScopedDataPipeConsumerHandle source,
    mojo::ScopedDataPipeConsumerHandle* out_forwarding_pipe) {
  if (!source.is_valid()) {
    return nullptr;
  }

  auto cache = base::MakeRefCounted<MimeHandlerBodyCache>();
  if (out_forwarding_pipe &&
      !cache->InitializeForwarding(out_forwarding_pipe)) {
    // Hand `source` back so the caller can still use the original pipe.
    *out_forwarding_pipe = std::move(source);
    return nullptr;
  }
  cache->StartReading(std::move(source));
  return cache;
}

// static
base::AutoReset<size_t>
MimeHandlerBodyCache::SetMaxCacheBytesForTesting(  // IN-TEST
    size_t max_bytes) {
  return base::AutoReset<size_t>(&g_max_cache_bytes, max_bytes);
}

MimeHandlerBodyCache::MimeHandlerBodyCache()
    : source_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      forwarding_watcher_(FROM_HERE,
                          mojo::SimpleWatcher::ArmingPolicy::MANUAL) {}

MimeHandlerBodyCache::~MimeHandlerBodyCache() = default;

bool MimeHandlerBodyCache::InitializeForwarding(
    mojo::ScopedDataPipeConsumerHandle* out_forwarding_pipe) {
  mojo::ScopedDataPipeConsumerHandle consumer;
  if (mojo::CreateDataPipe(kDefaultPipeCapacity, forwarding_producer_,
                           consumer) != MOJO_RESULT_OK) {
    return false;
  }

  forwarding_watcher_.Watch(
      forwarding_producer_.get(),
      MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&MimeHandlerBodyCache::OnForwardingPipeWritable,
                          weak_factory_.GetWeakPtr()));

  *out_forwarding_pipe = std::move(consumer);
  return true;
}

void MimeHandlerBodyCache::StartReading(
    mojo::ScopedDataPipeConsumerHandle source) {
  source_ = std::move(source);
  source_watcher_.Watch(
      source_.get(), MOJO_HANDLE_SIGNAL_READABLE,
      MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
      base::BindRepeating(&MimeHandlerBodyCache::OnSourceReadable,
                          weak_factory_.GetWeakPtr()));
  source_watcher_.ArmOrNotify();
}

void MimeHandlerBodyCache::OnSourceReadable(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  if (result != MOJO_RESULT_OK) {
    OnSourceDone();
    return;
  }

  base::span<const uint8_t> data;
  MojoResult read_result =
      source_->BeginReadData(MOJO_READ_DATA_FLAG_NONE, data);
  if (read_result == MOJO_RESULT_SHOULD_WAIT) {
    source_watcher_.ArmOrNotify();
    return;
  }
  if (read_result != MOJO_RESULT_OK) {
    OnSourceDone();
    return;
  }

  if (state_ == State::kAbandoned) {
    source_->EndReadData(data.size());
    return;
  }
  if (buffer_.size() + data.size() > g_max_cache_bytes) {
    // Response exceeds the cap. Release the buffer and tear down the
    // forwarding pipe; the live load aborts, and the fallback reload
    // path refetches from the network (`is_complete()` stays false).
    state_ = State::kAbandoned;
    buffer_.clear();
    buffer_.shrink_to_fit();
    forwarding_producer_.reset();
    forwarding_watcher_.Cancel();
    source_->EndReadData(data.size());
    source_watcher_.Cancel();
    source_.reset();
    return;
  }
  buffer_.insert(buffer_.end(), data.begin(), data.end());
  source_->EndReadData(data.size());

  if (forwarding_producer_.is_valid()) {
    WritePendingToForwarding();
  }
  source_watcher_.ArmOrNotify();
}

void MimeHandlerBodyCache::OnSourceDone() {
  source_watcher_.Cancel();
  source_.reset();
  if (state_ == State::kAbandoned) {
    return;
  }
  state_ = State::kComplete;

  if (forwarding_producer_.is_valid()) {
    WritePendingToForwarding();
  }
}

MimeHandlerBodyCache::ForwardResult MimeHandlerBodyCache::ForwardBytes(
    base::span<const uint8_t> pending,
    size_t& offset) {
  while (offset < pending.size()) {
    size_t bytes_written = 0;
    MojoResult result = forwarding_producer_->WriteData(
        pending.subspan(offset), MOJO_WRITE_DATA_FLAG_NONE, bytes_written);

    if (result == MOJO_RESULT_OK) {
      offset += bytes_written;
    } else if (result == MOJO_RESULT_SHOULD_WAIT) {
      // The pipe is full; re-arm the writable watcher to resume later.
      forwarding_watcher_.ArmOrNotify();
      return ForwardResult::kBackpressured;
    } else {
      return ForwardResult::kPeerGone;
    }
  }
  return ForwardResult::kDrained;
}

void MimeHandlerBodyCache::WritePendingToForwarding() {
  if (!forwarding_producer_.is_valid()) {
    return;
  }

  switch (ForwardBytes(buffer_, forwarding_offset_)) {
    case ForwardResult::kBackpressured:
      return;
    case ForwardResult::kPeerGone:
      CloseForwarding();
      return;
    case ForwardResult::kDrained:
      break;
  }

  if (state_ == State::kComplete && forwarding_offset_ >= buffer_.size()) {
    CloseForwarding();
  }
}

void MimeHandlerBodyCache::OnForwardingPipeWritable(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  if (result != MOJO_RESULT_OK || state.peer_closed()) {
    CloseForwarding();
    return;
  }

  WritePendingToForwarding();
}

void MimeHandlerBodyCache::CloseForwarding() {
  forwarding_watcher_.Cancel();
  forwarding_producer_.reset();
}

mojo::ScopedDataPipeConsumerHandle MimeHandlerBodyCache::CreatePipe() {
  if (!is_complete()) {
    return mojo::ScopedDataPipeConsumerHandle();
  }

  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer;
  if (mojo::CreateDataPipe(kDefaultPipeCapacity, producer_handle, consumer) !=
      MOJO_RESULT_OK) {
    return mojo::ScopedDataPipeConsumerHandle();
  }

  if (buffer_.empty()) {
    // Nothing to replay; drop the producer so the consumer sees EOF.
    return consumer;
  }

  // Stream the buffer asynchronously through `mojo::DataPipeProducer`.
  // It chunks the write into the pipe's ring-buffer capacity rather
  // than allocating shared memory the size of the response and
  // copying it synchronously on the calling sequence.
  auto pipe_producer =
      std::make_unique<mojo::DataPipeProducer>(std::move(producer_handle));
  mojo::DataPipeProducer* const producer_ptr = pipe_producer.get();
  producer_ptr->Write(
      std::make_unique<mojo::StringDataSource>(
          base::as_chars(base::span(buffer_)),
          mojo::StringDataSource::AsyncWritingMode::
              STRING_STAYS_VALID_UNTIL_COMPLETION),
      // The bound `scoped_refptr` keeps this cache (and `buffer_`)
      // alive while the worker sequence reads from it. The bound
      // `unique_ptr` owns the producer until the write completes; the
      // producer handle closes when the producer is destroyed at the
      // end of this lambda, signalling EOF to the consumer.
      base::BindOnce([](scoped_refptr<MimeHandlerBodyCache>,
                        std::unique_ptr<mojo::DataPipeProducer>, MojoResult) {},
                     base::WrapRefCounted(this), std::move(pipe_producer)));
  return consumer;
}

}  // namespace extensions
