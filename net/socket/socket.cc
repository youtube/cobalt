// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/socket.h"

#include <set>

#include "net/base/net_errors.h"

#if BUILDFLAG(IS_COBALT)
#include <algorithm>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/no_destructor.h"
#include "base/numerics/checked_math.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#endif

namespace net {

#if BUILDFLAG(IS_COBALT)
namespace {

// Cap the shared recvmmsg scratch pool so concurrent active readers or
// reentrant callbacks reuse buffers without pinning memory per idle socket.
constexpr size_t kMaxCachedRecvmmsgBuffers = 2;

// Thread-safe bounded pool of backing buffers reused across recvmmsg reads.
class RecvmmsgBufferPool {
 public:
  scoped_refptr<IOBufferWithSize> Acquire(size_t total_size) {
    {
      base::AutoLock lock(lock_);
      for (auto it = buffers_.begin(); it != buffers_.end(); ++it) {
        if ((*it)->span().size() == total_size) {
          scoped_refptr<IOBufferWithSize> buffer = std::move(*it);
          buffers_.erase(it);
          return buffer;
        }
      }
    }
    return base::MakeRefCounted<IOBufferWithSize>(total_size);
  }

  void Release(scoped_refptr<IOBufferWithSize> buffer) {
    if (!buffer || !buffer->HasOneRef()) {
      return;
    }
    base::AutoLock lock(lock_);
    if (buffers_.size() < kMaxCachedRecvmmsgBuffers) {
      buffers_.push_back(std::move(buffer));
      return;
    }
    const size_t size = buffer->span().size();
    const bool has_matching_size = std::ranges::any_of(
        buffers_, [size](const scoped_refptr<IOBufferWithSize>& entry) {
          return entry->span().size() == size;
        });
    if (!has_matching_size) {
      buffers_.erase(buffers_.begin());
      buffers_.push_back(std::move(buffer));
    }
  }

  void ClearForTesting() {
    base::AutoLock lock(lock_);
    buffers_.clear();
  }

  size_t SizeForTesting() {
    base::AutoLock lock(lock_);
    return buffers_.size();
  }

 private:
  base::Lock lock_;
  std::vector<scoped_refptr<IOBufferWithSize>> buffers_ GUARDED_BY(lock_);
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

Socket::ReadPacketResults::ReadPacketResults(
    ReadPacketResults&& other) noexcept
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

void Socket::ReadPacketResults::AcquireBuffer(int max_packets,
                                              int packet_buf_size) {
  DCHECK_GT(max_packets, 0);
  DCHECK_GT(packet_buf_size, 0);

  const size_t num_packets = static_cast<size_t>(max_packets);
  const size_t per_packet_size = static_cast<size_t>(packet_buf_size);
  const size_t packets_array_size =
      base::CheckMul(num_packets, sizeof(ReadPacketResult)).ValueOrDie();
  const size_t all_packet_buffers_size =
      base::CheckMul(num_packets, per_packet_size).ValueOrDie();
  const size_t total_size =
      base::CheckAdd(packets_array_size, all_packet_buffers_size).ValueOrDie();

  if (!buffer || !buffer->HasOneRef() || buffer->span().size() != total_size ||
      packet_buffer_size != packet_buf_size || packets == nullptr) {
    ReleaseBuffer();
    buffer = GetRecvmmsgBufferPool().Acquire(total_size);
    packet_buffer_size = packet_buf_size;
    packets = reinterpret_cast<ReadPacketResult*>(buffer->data());
  }

  result = 0;
  std::ranges::fill(buffer->span().first(packets_array_size), 0);
  // SAFETY: |buffer| owns |total_size| bytes containing |num_packets|
  // ReadPacketResult entries followed by |num_packets| packet buffers of
  // |per_packet_size| bytes each.
  char* packet_data_start = UNSAFE_BUFFERS(buffer->data() + packets_array_size);
  for (size_t i = 0; i < num_packets; ++i) {
    UNSAFE_BUFFERS(packets[i].buffer =
                       packet_data_start + (i * per_packet_size));
  }
}

void Socket::ReadPacketResults::ReleaseBuffer() {
  scoped_refptr<IOBufferWithSize> old_buffer = std::move(buffer);
  result = 0;
  packet_buffer_size = 0;
  packets = nullptr;
  GetRecvmmsgBufferPool().Release(std::move(old_buffer));
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
