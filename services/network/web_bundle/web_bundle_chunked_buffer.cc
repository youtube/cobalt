// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/web_bundle/web_bundle_chunked_buffer.h"

#include <algorithm>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/checked_math.h"

namespace network {

namespace {

class ChunkedBufferDataSource : public mojo::DataPipeProducer::DataSource {
 public:
  ChunkedBufferDataSource(std::unique_ptr<const WebBundleChunkedBuffer> buffer,
                          uint64_t offset,
                          uint64_t length)
      : buffer_(std::move(buffer)), offset_(offset), length_(length) {
    DCHECK(buffer_);
  }
  ~ChunkedBufferDataSource() override = default;

  // Disallow copy and assign.
  ChunkedBufferDataSource(const ChunkedBufferDataSource&) = delete;
  ChunkedBufferDataSource& operator=(const ChunkedBufferDataSource&) = delete;

  uint64_t GetLength() const override { return length_; }

  ReadResult Read(uint64_t offset, base::span<char> buffer) override {
    ReadResult result;
    if (offset >= length_) {
      result.result = MOJO_RESULT_OUT_OF_RANGE;
      return result;
    }
    uint64_t read_start = offset_ + offset;
    uint64_t out_size = buffer.size();
    uint64_t len = std::min(length_ - offset, out_size);
    buffer_->ReadData(read_start, len,
                      reinterpret_cast<uint8_t*>(buffer.data()));
    result.bytes_read = len;
    return result;
  }

 private:
  const std::unique_ptr<const WebBundleChunkedBuffer> buffer_;
  const uint64_t offset_;
  const uint64_t length_;
};

}  // namespace

WebBundleChunkedBuffer::Chunk::Chunk(
    uint64_t start_pos,
    scoped_refptr<const base::RefCountedBytes> bytes)
    : start_pos_(start_pos), bytes_(std::move(bytes)) {
  DCHECK(bytes_);
  DCHECK(bytes_->size() != 0);
  DCHECK(base::CheckAdd<uint64_t>(start_pos_, bytes_->size()).IsValid());
}

WebBundleChunkedBuffer::Chunk::~Chunk() = default;

WebBundleChunkedBuffer::Chunk::Chunk(const WebBundleChunkedBuffer::Chunk&) =
    default;
WebBundleChunkedBuffer::Chunk::Chunk(WebBundleChunkedBuffer::Chunk&&) = default;

uint64_t WebBundleChunkedBuffer::Chunk::start_pos() const {
  return start_pos_;
}

uint64_t WebBundleChunkedBuffer::Chunk::end_pos() const {
  return start_pos_ + bytes_->size();
}

uint64_t WebBundleChunkedBuffer::Chunk::size() const {
  return bytes_->size();
}

const uint8_t* WebBundleChunkedBuffer::Chunk::data() const {
  return bytes_->data().data();
}

WebBundleChunkedBuffer::WebBundleChunkedBuffer() = default;

WebBundleChunkedBuffer::WebBundleChunkedBuffer(ChunkVector chunks)
    : chunks_(std::move(chunks)) {}

WebBundleChunkedBuffer::~WebBundleChunkedBuffer() = default;

void WebBundleChunkedBuffer::Append(const uint8_t* data, size_t num_bytes) {
  DCHECK(base::CheckAdd<uint64_t>(end_pos(), num_bytes).IsValid());
  if (num_bytes == 0)
    return;
  auto bytes = base::MakeRefCounted<base::RefCountedBytes>(data, num_bytes);
  chunks_.push_back(Chunk(end_pos(), std::move(bytes)));
}

bool WebBundleChunkedBuffer::ContainsAll(uint64_t offset,
                                         uint64_t length) const {
  DCHECK(base::CheckAdd<uint64_t>(offset, length).IsValid());
  if (length == 0)
    return true;
  if (offset < start_pos())
    return false;
  if (offset + length > end_pos())
    return false;
  return true;
}

std::unique_ptr<mojo::DataPipeProducer::DataSource>
WebBundleChunkedBuffer::CreateDataSource(uint64_t offset,
                                         uint64_t max_length) const {
  uint64_t length = GetAvailableLength(offset, max_length);
  if (length == 0)
    return nullptr;
  return std::make_unique<ChunkedBufferDataSource>(
      CreatePartialBuffer(offset, length), offset, length);
}

uint64_t WebBundleChunkedBuffer::size() const {
  DCHECK_GE(end_pos(), start_pos());
  return end_pos() - start_pos();
}

WebBundleChunkedBuffer::ChunkVector::const_iterator
WebBundleChunkedBuffer::FindChunk(uint64_t pos) const {
  if (empty())
    return chunks_.end();
  // |pos| ls before everything
  if (pos < chunks_.begin()->start_pos())
    return chunks_.end();
  // As an optimization, check the last region first
  if (chunks_.back().start_pos() <= pos) {
    if (chunks_.back().end_pos() <= pos)
      return chunks_.end();
    return chunks_.end() - 1;
  }
  // Binary search
  return std::partition_point(
      chunks_.begin(), chunks_.end(),
      [pos](const Chunk& chunk) { return chunk.end_pos() <= pos; });
}

std::unique_ptr<const WebBundleChunkedBuffer>
WebBundleChunkedBuffer::CreatePartialBuffer(uint64_t offset,
                                            uint64_t length) const {
  DCHECK(ContainsAll(offset, length));
  ChunkVector::const_iterator it = FindChunk(offset);
  CHECK(it != chunks_.end());
  ChunkVector new_chunks;
  while (it != chunks_.end() && it->start_pos() < offset + length) {
    new_chunks.push_back(*it);
    ++it;
  }
  return base::WrapUnique(new WebBundleChunkedBuffer(std::move(new_chunks)));
}

bool WebBundleChunkedBuffer::empty() const {
  return chunks_.empty();
}

uint64_t WebBundleChunkedBuffer::start_pos() const {
  if (empty())
    return 0;
  return chunks_.front().start_pos();
}

uint64_t WebBundleChunkedBuffer::end_pos() const {
  if (empty())
    return 0;
  return chunks_.back().end_pos();
}

uint64_t WebBundleChunkedBuffer::GetAvailableLength(uint64_t offset,
                                                    uint64_t max_length) const {
  if (offset < start_pos())
    return 0;
  if (end_pos() <= offset)
    return 0;
  return std::min(max_length, end_pos() - offset);
}

uint64_t WebBundleChunkedBuffer::ReadData(uint64_t offset,
                                          uint64_t max_length,
                                          uint8_t* out) const {
  uint64_t length = GetAvailableLength(offset, max_length);
  if (length == 0)
    return 0;
  ChunkVector::const_iterator it = FindChunk(offset);
  DCHECK(it != chunks_.end());
  uint64_t written = 0;
  while (length > written) {
    DCHECK(it != chunks_.end());
    uint64_t offset_in_chunk = offset + written - it->start_pos();
    uint64_t length_in_chunk =
        std::min(it->size() - offset_in_chunk, length - written);
    memcpy(out + written, it->data() + offset_in_chunk, length_in_chunk);
    written += length_in_chunk;
    ++it;
  }
  return written;
}

}  // namespace network
