// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "media/starboard/starboard_media_external_memory_allocator.h"

#include <cstring>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "media/base/demuxer_stream.h"
#include "media/starboard/decoder_buffer_allocator.h"

namespace media {

namespace {

// Wraps a memory block handle allocated from Starboard's media buffer pool
// (DecoderBufferAllocator), exposing read access via Span() and the underlying
// Starboard handle via handle() for zero-copy media pipeline ingestion.
//
// Lifetime/Ownership: Owns a single allocated memory block in the Starboard
// media pool. Upon destruction (when the owning DecoderBuffer is released),
// it automatically returns the block to the pool via pool_->Free().
//
// Threading Model: Thread-safe for destruction and span access. Since
// DecoderBuffer::Allocator::Free() is internally synchronized with a mutex,
// instances of this class can be safely transferred across thread boundaries
// (e.g., from demuxer/parser threads to audio/video render threads) and
// destroyed on any thread.
class StarboardPoolExternalMemory : public DecoderBuffer::ExternalMemory {
 public:
  StarboardPoolExternalMemory(DecoderBuffer::Allocator* pool,
                              DecoderBuffer::Allocator::Handle handle,
                              size_t size,
                              const uint8_t* data,
                              DemuxerStream::Type type)
      : pool_(pool),
        handle_(handle),
        size_(size),
        span_(data, size),
        type_(type) {
    CHECK(pool_);
    if (size_ > 0) {
      CHECK_NE(handle_, DecoderBuffer::Allocator::kInvalidHandle);
      CHECK(data);
    }
  }

  ~StarboardPoolExternalMemory() override {
    if (size_ > 0 && handle_ != DecoderBuffer::Allocator::kInvalidHandle &&
        pool_) {
      pool_->Free(type_, handle_, size_);
    }
  }

  const base::span<const uint8_t> Span() const override { return span_; }

  DecoderBuffer::Allocator::Handle handle() const override { return handle_; }

 private:
  DecoderBuffer::Allocator* const pool_;
  const DecoderBuffer::Allocator::Handle handle_;
  const size_t size_;
  const base::span<const uint8_t> span_;
  const DemuxerStream::Type type_;
};

}  // namespace

StarboardMediaExternalMemoryAllocator::StarboardMediaExternalMemoryAllocator() =
    default;

StarboardMediaExternalMemoryAllocator::
    ~StarboardMediaExternalMemoryAllocator() = default;

std::unique_ptr<DecoderBuffer::ExternalMemory>
StarboardMediaExternalMemoryAllocator::CopyFrom(
    base::span<const uint8_t> span) {
  // In Starboard builds, stream parsers (e.g., mp4_stream_parser.cc) should
  // always invoke the 2-parameter overload CopyFrom(span, type) so that media
  // pool strategies receive the exact DemuxerStream::Type. This 1-parameter
  // implementation exists to satisfy the pure virtual vtable contract and acts
  // as a fallback for generic or legacy tests.
  return CopyFrom(span, DemuxerStream::UNKNOWN);
}

std::unique_ptr<DecoderBuffer::ExternalMemory>
StarboardMediaExternalMemoryAllocator::CopyFrom(base::span<const uint8_t> span,
                                                DemuxerStream::Type type) {
  auto* pool = DecoderBufferAllocator::Get();
  if (!pool) {
    LOG(ERROR) << "DecoderBufferAllocator instance is not initialized.";
    return nullptr;
  }

  if (span.empty()) {
    return std::make_unique<StarboardPoolExternalMemory>(
        pool, DecoderBuffer::Allocator::kInvalidHandle, 0, nullptr, type);
  }

  auto handle = pool->Allocate(type, span.size());
  if (handle == DecoderBuffer::Allocator::kInvalidHandle) {
    LOG(ERROR) << "Failed to allocate " << span.size()
               << " bytes from Starboard media buffer pool.";
    return nullptr;
  }

  pool->Write(handle, span.data(), span.size());

  // Cast handle to pointer for read access in span.
  // Note: If annotated pointers are enabled, IsPointerAnnotated check will
  // occur when Span().data() is accessed, consistent with
  // DecoderBuffer::data().
  const uint8_t* data_ptr = reinterpret_cast<const uint8_t*>(handle);
  return std::make_unique<StarboardPoolExternalMemory>(
      pool, handle, span.size(), data_ptr, type);
}

}  // namespace media
