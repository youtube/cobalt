// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/socket.h"

#include <set>

#include "net/base/net_errors.h"

#if BUILDFLAG(IS_COBALT)
#include <algorithm>
#include <array>
#include <utility>

#include "base/compiler_specific.h"
#include "base/no_destructor.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#endif

namespace net {

#if BUILDFLAG(IS_COBALT)
namespace {

// Cap the shared recvmmsg scratch pool by both entry count and total bytes so
// concurrent active readers reuse buffers without parking oversized or excess
// memory across idle sockets (256 KiB comfortably holds two 64-packet buffers).
constexpr size_t kMaxCachedRecvmmsgBuffers = 2;
constexpr size_t kMaxCachedRecvmmsgPoolBytes = 256 * 1024;
static_assert(kMaxCachedRecvmmsgBuffers > 0);

// Bounded process-wide LIFO/LRU pool of pre-initialized backing buffers reused
// across recvmmsg reads so idle sockets hold no packet-batch memory.
// Lifetime/ownership: owned as a never-destroyed process-wide singleton via
// base::NoDestructor in GetRecvmmsgBufferPool().
// Threading: thread-safe via internal base::Lock (preferred over thread_local
// to avoid Android emutls overhead and bound capacity process-wide).
class RecvmmsgBufferPool {
 public:
  scoped_refptr<IOBufferWithSize> Acquire(size_t total_size,
                                          size_t num_packets,
                                          size_t packets_array_size,
                                          int packet_buf_size) {
    {
      base::AutoLock lock(lock_);
      // Search LIFO so the most recently used (cache-warm) buffer is reused.
      for (size_t i = count_; i-- > 0;) {
        if (entries_[i].buffer->span().size() == total_size &&
            entries_[i].packet_buffer_size == packet_buf_size) {
          scoped_refptr<IOBufferWithSize> buffer =
              std::move(entries_[i].buffer);
          cached_bytes_ -= total_size;
          for (size_t j = i + 1; j < count_; ++j) {
            entries_[j - 1] = std::move(entries_[j]);
          }
          entries_[--count_] = {};
          return buffer;
        }
      }
    }

    // Pool miss: allocate and initialize ReadPacketResult::buffer pointers once
    // for the lifetime of this backing buffer.
    auto buffer = base::MakeRefCounted<IOBufferWithSize>(total_size);
    std::ranges::fill(buffer->span().first(packets_array_size), 0);
    auto* packets = reinterpret_cast<Socket::ReadPacketResult*>(buffer->data());
    // SAFETY: |buffer| owns |total_size| bytes containing |num_packets|
    // ReadPacketResult entries followed by |num_packets| packet buffers of
    // |packet_buf_size| bytes each.
    char* packet_data_start =
        UNSAFE_BUFFERS(buffer->data() + packets_array_size);
    const size_t per_packet_size = static_cast<size_t>(packet_buf_size);
    for (size_t i = 0; i < num_packets; ++i) {
      UNSAFE_BUFFERS(packets[i].buffer =
                         packet_data_start + (i * per_packet_size));
    }
    return buffer;
  }

  void Release(scoped_refptr<IOBufferWithSize> buffer, int packet_buf_size) {
    if (!buffer || !buffer->HasOneRef() || packet_buf_size <= 0) {
      return;
    }
    const size_t buffer_bytes = buffer->span().size();
    if (buffer_bytes > kMaxCachedRecvmmsgPoolBytes) {
      return;
    }
    // Release any evicted buffers after dropping |lock_| so deallocation stays
    // off the critical section.
    std::array<scoped_refptr<IOBufferWithSize>, kMaxCachedRecvmmsgBuffers>
        evicted;
    size_t evicted_count = 0;
    {
      base::AutoLock lock(lock_);
      // Plain LRU eviction: evict the oldest entries until both the slot count
      // and total byte cap allow caching the newly returned buffer.
      while (count_ > 0 &&
             (count_ >= kMaxCachedRecvmmsgBuffers ||
              cached_bytes_ + buffer_bytes > kMaxCachedRecvmmsgPoolBytes)) {
        cached_bytes_ -= entries_[0].buffer->span().size();
        evicted[evicted_count++] = std::move(entries_[0].buffer);
        for (size_t i = 1; i < count_; ++i) {
          entries_[i - 1] = std::move(entries_[i]);
        }
        entries_[--count_] = {};
      }
      entries_[count_++] = {std::move(buffer), packet_buf_size};
      cached_bytes_ += buffer_bytes;
    }
  }

  void ClearForTesting() {
    std::array<Entry, kMaxCachedRecvmmsgBuffers> cleared;
    {
      base::AutoLock lock(lock_);
      for (size_t i = 0; i < count_; ++i) {
        cleared[i] = std::move(entries_[i]);
      }
      count_ = 0;
      cached_bytes_ = 0;
    }
  }

  size_t SizeForTesting() {
    base::AutoLock lock(lock_);
    return count_;
  }

 private:
  struct Entry {
    scoped_refptr<IOBufferWithSize> buffer;
    int packet_buffer_size = 0;
  };

  base::Lock lock_;
  std::array<Entry, kMaxCachedRecvmmsgBuffers> entries_ GUARDED_BY(lock_);
  size_t count_ GUARDED_BY(lock_) = 0;
  size_t cached_bytes_ GUARDED_BY(lock_) = 0;
};

RecvmmsgBufferPool& GetRecvmmsgBufferPool() {
  static base::NoDestructor<RecvmmsgBufferPool> pool;
  return *pool;
}

}  // namespace
#endif

Socket::Socket() = default;

Socket::~Socket() = default;

#if BUILDFLAG(IS_COBALT)

Socket::ReadPacketResults::ReadPacketResults() = default;

Socket::ReadPacketResults::ReadPacketResults(ReadPacketResults&& other) noexcept
    : buffer(std::move(other.buffer)),
      result(std::exchange(other.result, 0)),
      packet_buffer_size(std::exchange(other.packet_buffer_size, 0)),
      packets(std::exchange(other.packets, nullptr)) {}

Socket::ReadPacketResults& Socket::ReadPacketResults::operator=(
    ReadPacketResults&& other) noexcept {
  if (this != &other) {
    ReleaseBuffer();
    buffer = std::move(other.buffer);
    result = std::exchange(other.result, 0);
    packet_buffer_size = std::exchange(other.packet_buffer_size, 0);
    packets = std::exchange(other.packets, nullptr);
  }
  return *this;
}

Socket::ReadPacketResults::~ReadPacketResults() {
  ReleaseBuffer();
}

bool Socket::ReadPacketResults::AcquireBuffer(int max_packets,
                                              int packet_buf_size) {
  if (max_packets <= 0 || packet_buf_size <= 0) {
    ReleaseBuffer();
    return false;
  }

  const size_t num_packets = static_cast<size_t>(max_packets);
  const size_t per_packet_size = static_cast<size_t>(packet_buf_size);
  size_t packets_array_size = 0;
  size_t all_packet_buffers_size = 0;
  size_t total_size = 0;
  if (!base::CheckMul(num_packets, sizeof(ReadPacketResult))
           .AssignIfValid(&packets_array_size) ||
      !base::CheckMul(num_packets, per_packet_size)
           .AssignIfValid(&all_packet_buffers_size) ||
      !base::CheckAdd(packets_array_size, all_packet_buffers_size)
           .AssignIfValid(&total_size) ||
      !base::IsValueInRangeForNumericType<int>(total_size)) {
    ReleaseBuffer();
    return false;
  }

  if (!buffer || !buffer->HasOneRef() || buffer->span().size() != total_size ||
      packet_buffer_size != packet_buf_size || packets == nullptr) {
    if (buffer) {
      ReleaseBuffer();
    }
    buffer = GetRecvmmsgBufferPool().Acquire(
        total_size, num_packets, packets_array_size, packet_buf_size);
    packet_buffer_size = packet_buf_size;
    packets = reinterpret_cast<ReadPacketResult*>(buffer->data());
  }

  result = 0;
  return true;
}

void Socket::ReadPacketResults::ReleaseBuffer() {
  if (!buffer) {
    result = 0;
    packet_buffer_size = 0;
    packets = nullptr;
    return;
  }
  scoped_refptr<IOBufferWithSize> old_buffer = std::move(buffer);
  const int old_packet_buffer_size = std::exchange(packet_buffer_size, 0);
  const bool was_initialized = std::exchange(packets, nullptr) != nullptr;
  result = 0;
  if (was_initialized) {
    GetRecvmmsgBufferPool().Release(std::move(old_buffer),
                                    old_packet_buffer_size);
  }
}

// static
void Socket::ReadPacketResults::ClearBufferPoolForTesting() {
  GetRecvmmsgBufferPool().ClearForTesting();
}

// static
size_t Socket::ReadPacketResults::GetBufferPoolSizeForTesting() {
  return GetRecvmmsgBufferPool().SizeForTesting();
}

int Socket::ReadMultiplePackets(ReadPacketResults* results,
                                int read_buffer_size,
                                CompletionOnceCallback callback) {
  // Default to not implemented
  return ERR_NOT_IMPLEMENTED;
}
#endif

int Socket::ReadIfReady(IOBuffer* buf,
                        int buf_len,
                        CompletionOnceCallback callback) {
  return ERR_READ_IF_READY_NOT_IMPLEMENTED;
}

int Socket::CancelReadIfReady() {
  return ERR_READ_IF_READY_NOT_IMPLEMENTED;
}

void Socket::SetDnsAliases(std::set<std::string> aliases) {
  if (aliases == std::set<std::string>({""})) {
    // Reset field to empty vector. Necessary because some tests and other
    // inputs still use a trivial canonical name of std::string().
    dns_aliases_.clear();
    return;
  }

  dns_aliases_ = std::move(aliases);
}

const std::set<std::string>& Socket::GetDnsAliases() const {
  return dns_aliases_;
}

}  // namespace net
