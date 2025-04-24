// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/decoder_buffer.h"

#include <sstream>

#include "base/containers/heap_array.h"
#include "base/debug/alias.h"
#include "media/base/subsample_entry.h"

namespace media {

#if BUILDFLAG(USE_STARBOARD_MEDIA)
namespace {
DecoderBuffer::Allocator* s_allocator = nullptr;
}  // namespace

// static
DecoderBuffer::Allocator* DecoderBuffer::Allocator::GetInstance() {
  DCHECK(s_allocator);
  return s_allocator;
}

// static
void DecoderBuffer::Allocator::Set(Allocator* allocator) {
  // One of them has to be nullptr, i.e. either setting a valid allocator, or
  // resetting an existing allocator.  Setting an allocator while another
  // allocator is in place will fail.
  DCHECK(s_allocator == nullptr || allocator == nullptr);
  s_allocator = allocator;
}
#endif // BUILDFLAG(USE_STARBOARD_MEDIA)

DecoderBuffer::TimeInfo::TimeInfo() = default;
DecoderBuffer::TimeInfo::~TimeInfo() = default;
DecoderBuffer::TimeInfo::TimeInfo(const TimeInfo&) = default;
DecoderBuffer::TimeInfo& DecoderBuffer::TimeInfo::operator=(const TimeInfo&) =
    default;

DecoderBuffer::DecoderBuffer(size_t size) : size_(size) {
  Initialize();
}

DecoderBuffer::DecoderBuffer(base::span<const uint8_t> data)
    : size_(data.size()) {
  Initialize();
#if BUILDFLAG(USE_STARBOARD_MEDIA)
  memcpy(data_, data.data(), size_);
#else // BUILDFLAG(USE_STARBOARD_MEDIA)
  data_.copy_from(data);
#endif // BUILDFLAG(USE_STARBOARD_MEDIA)
}

#if BUILDFLAG(USE_STARBOARD_MEDIA)
DecoderBuffer::DecoderBuffer(DemuxerStream::Type type,
                             const uint8_t* data,
                             size_t size,
                             const uint8_t* side_data,
                             size_t side_data_size)
    : size_(size), side_data_size_(side_data_size), is_key_frame_(false) {
  if (!data) {
    CHECK_EQ(size_, 0u);
    CHECK(!side_data);
    return;
  }

  Initialize(type);

  memcpy(data_, data, size_);

  if (!side_data) {
    CHECK_EQ(side_data_size, 0u);
    return;
  }

  DCHECK_GT(side_data_size_, 0u);
  memcpy(side_data_.get(), side_data, side_data_size_);
}
#endif // BUILDFLAG(USE_STARBOARD_MEDIA)

DecoderBuffer::DecoderBuffer(base::HeapArray<uint8_t> data, size_t size)
#if BUILDFLAG(USE_STARBOARD_MEDIA)
  : DecoderBuffer(data, size) {
  // TODO(b/378106931): revisit DecoderBufferAllocator once rebase to m126+
}
#else // BUILDFLAG(USE_STARBOARD_MEDIA)
    : data_(std::move(data)), size_(size) {}
#endif // BUILDFLAG(USE_STARBOARD_MEDIA)

DecoderBuffer::DecoderBuffer(base::ReadOnlySharedMemoryMapping mapping,
                             size_t size)
    : size_(size), read_only_mapping_(std::move(mapping)) {}

DecoderBuffer::DecoderBuffer(base::WritableSharedMemoryMapping mapping,
                             size_t size)
    : size_(size), writable_mapping_(std::move(mapping)) {}

DecoderBuffer::DecoderBuffer(std::unique_ptr<ExternalMemory> external_memory)
    : size_(external_memory->span().size()),
      external_memory_(std::move(external_memory)) {}

DecoderBuffer::DecoderBuffer(DecoderBufferType decoder_buffer_type)
    : is_end_of_stream_(decoder_buffer_type ==
                        DecoderBufferType::kEndOfStream) {}

#if BUILDFLAG(USE_STARBOARD_MEDIA)
DecoderBuffer::~DecoderBuffer() {
  DCHECK(s_allocator);
  s_allocator->Free(data_, allocated_size_);
}
#else // BUILDFLAG(USE_STARBOARD_MEDIA)
DecoderBuffer::~DecoderBuffer() = default;
#endif // BUILDFLAG(USE_STARBOARD_MEDIA)

void DecoderBuffer::Initialize() {
#if BUILDFLAG(USE_STARBOARD_MEDIA)
  // This is used by Mojo.
  // TODO: b/369245553 - Cobalt: Investigate the memory and performance impact
  //                     of using Mojo.
  Initialize(DemuxerStream::UNKNOWN);
#else // BUILDFLAG(USE_STARBOARD_MEDIA)
  data_ = base::HeapArray<uint8_t>::Uninit(size_);
#endif // BUILDFLAG(USE_STARBOARD_MEDIA)
}

#if BUILDFLAG(USE_STARBOARD_MEDIA)
void DecoderBuffer::Initialize(DemuxerStream::Type type) {
  DCHECK(s_allocator);
  DCHECK(!data_);

  int alignment = s_allocator->GetBufferAlignment();
  int padding = s_allocator->GetBufferPadding();
  allocated_size_ = size_ + padding;
  data_ = static_cast<uint8_t*>(s_allocator->Allocate(type,
                                                      allocated_size_,
                                                      alignment));
  memset(data_ + size_, 0, padding);
}
#endif // BUILDFLAG(USE_STARBOARD_MEDIA)

// static
scoped_refptr<DecoderBuffer> DecoderBuffer::CopyFrom(
    base::span<const uint8_t> data) {
  return base::WrapRefCounted(new DecoderBuffer(data));
}

// static
scoped_refptr<DecoderBuffer> DecoderBuffer::FromArray(
    base::HeapArray<uint8_t> data,
    size_t size) {
  return base::WrapRefCounted(new DecoderBuffer(std::move(data), size));
}

// static
scoped_refptr<DecoderBuffer> DecoderBuffer::FromSharedMemoryRegion(
    base::UnsafeSharedMemoryRegion region,
    uint64_t offset,
    size_t size) {
  if (size == 0) {
    return nullptr;
  }

  auto mapping = region.MapAt(offset, size);
  if (!mapping.IsValid()) {
    return nullptr;
  }
  return base::WrapRefCounted(new DecoderBuffer(std::move(mapping), size));
}

// static
scoped_refptr<DecoderBuffer> DecoderBuffer::FromSharedMemoryRegion(
    base::ReadOnlySharedMemoryRegion region,
    uint64_t offset,
    size_t size) {
  if (size == 0) {
    return nullptr;
  }
  auto mapping = region.MapAt(offset, size);
  if (!mapping.IsValid()) {
    return nullptr;
  }
  return base::WrapRefCounted(new DecoderBuffer(std::move(mapping), size));
}

// static
scoped_refptr<DecoderBuffer> DecoderBuffer::FromExternalMemory(
    std::unique_ptr<ExternalMemory> external_memory) {
  DCHECK(external_memory);
  if (external_memory->span().empty())
    return nullptr;
  return base::WrapRefCounted(new DecoderBuffer(std::move(external_memory)));
}

// static
scoped_refptr<DecoderBuffer> DecoderBuffer::CreateEOSBuffer() {
  return base::WrapRefCounted(
      new DecoderBuffer(DecoderBufferType::kEndOfStream));
}

// static
bool DecoderBuffer::DoSubsamplesMatch(const DecoderBuffer& buffer) {
  // If buffer is at end of stream, no subsamples to verify
  if (buffer.end_of_stream()) {
    return true;
  }

  // If stream is unencrypted, we do not have to verify subsamples size.
  if (!buffer.is_encrypted()) {
    return true;
  }

  const auto& subsamples = buffer.decrypt_config()->subsamples();
  if (subsamples.empty()) {
    return true;
  }
  return VerifySubsamplesMatchSize(subsamples, buffer.size());
}

DecoderBufferSideData& DecoderBuffer::WritableSideData() {
  if (!side_data_.has_value()) {
    side_data_.emplace();
  }
  return side_data_.value();
}

bool DecoderBuffer::MatchesMetadataForTesting(
    const DecoderBuffer& buffer) const {
  if (end_of_stream() != buffer.end_of_stream())
    return false;

  // It is illegal to call any member function if eos is true.
  if (end_of_stream())
    return true;

  if (timestamp() != buffer.timestamp() || duration() != buffer.duration() ||
      is_key_frame() != buffer.is_key_frame() ||
      discard_padding() != buffer.discard_padding()) {
    return false;
  }

  if (has_side_data() != buffer.has_side_data()) {
    return false;
  }

  if (has_side_data() && !side_data()->Matches(buffer.side_data().value())) {
    return false;
  }

  if ((decrypt_config() == nullptr) != (buffer.decrypt_config() == nullptr))
    return false;

  return decrypt_config() ? decrypt_config()->Matches(*buffer.decrypt_config())
                          : true;
}

bool DecoderBuffer::MatchesForTesting(const DecoderBuffer& buffer) const {
  if (!MatchesMetadataForTesting(buffer))  // IN-TEST
    return false;

  // It is illegal to call any member function if eos is true.
  if (end_of_stream())
    return true;

  DCHECK(!buffer.end_of_stream());
  return base::span(*this) == base::span(buffer);
}

std::string DecoderBuffer::AsHumanReadableString(bool verbose) const {
  if (end_of_stream())
    return "EOS";

  std::ostringstream s;

  s << "{timestamp=" << time_info_.timestamp.InMicroseconds()
    << " duration=" << time_info_.duration.InMicroseconds() << " size=" << size_
    << " is_key_frame=" << is_key_frame_
    << " encrypted=" << (decrypt_config_ != nullptr);

  if (verbose) {
    s << " has_side_data=" << has_side_data() << " discard_padding (us)=("
      << time_info_.discard_padding.first.InMicroseconds() << ", "
      << time_info_.discard_padding.second.InMicroseconds() << ")";

    if (decrypt_config_)
      s << " decrypt_config=" << (*decrypt_config_);
  }

  s << "}";

  return s.str();
}

void DecoderBuffer::set_timestamp(base::TimeDelta timestamp) {
  DCHECK(!end_of_stream());
  time_info_.timestamp = timestamp;
}

size_t DecoderBuffer::GetMemoryUsage() const {
  size_t memory_usage = sizeof(DecoderBuffer);

  if (end_of_stream()) {
    return memory_usage;
  }

  memory_usage += size();

  // Side data and decrypt config would not change after construction.
  if (has_side_data()) {
    memory_usage += sizeof(decltype(side_data_->spatial_layers)::value_type) *
                    side_data_->spatial_layers.capacity();
    memory_usage += sizeof(decltype(side_data_->alpha_data)::value_type) *
                    side_data_->alpha_data.capacity();
  }
  if (decrypt_config_) {
    memory_usage += sizeof(DecryptConfig);
    memory_usage += decrypt_config_->key_id().capacity();
    memory_usage += decrypt_config_->iv().capacity();
    memory_usage +=
        sizeof(SubsampleEntry) * decrypt_config_->subsamples().capacity();
  }

  return memory_usage;
}

}  // namespace media
