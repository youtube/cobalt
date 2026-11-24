// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_chromium_packet_reader.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/features.h"
#include "net/base/net_errors.h"
#include "net/quic/address_utils.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_clock.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_types.h"

#if BUILDFLAG(IS_COBALT)
#include "net/quic/platform/impl/quic_chromium_clock.h"

#ifdef UNSAFE_BUFFERS_BUILD
#pragma allow_unsafe_buffers
#endif
#endif

namespace net {

namespace {
// Add 1 because some of our UDP socket implementations do not read successfully
// when the packet length is equal to the read buffer size.
const size_t kReadBufferSize =
    static_cast<size_t>(quic::kMaxIncomingPacketSize + 1);
}  // namespace

#if BUILDFLAG(IS_COBALT)
bool QuicChromiumPacketReader::try_reading_multiple_packets_{true};
#endif

QuicChromiumPacketReader::QuicChromiumPacketReader(
    std::unique_ptr<DatagramClientSocket> socket,
    const quic::QuicClock* clock,
    Visitor* visitor,
    int yield_after_packets,
    quic::QuicTime::Delta yield_after_duration,
    const NetLogWithSource& net_log)
    : socket_(std::move(socket)),
      visitor_(visitor),
      clock_(clock),
      yield_after_packets_(yield_after_packets),
      yield_after_duration_(yield_after_duration),
      yield_after_(quic::QuicTime::Infinite()),
#if BUILDFLAG(IS_COBALT)
      // Lazily allocated on single-packet fallback in StartReading().
      read_buffer_(nullptr),
#else
      read_buffer_(base::MakeRefCounted<IOBufferWithSize>(kReadBufferSize)),
#endif
      net_log_(net_log) {}

QuicChromiumPacketReader::~QuicChromiumPacketReader() = default;

#if BUILDFLAG(IS_COBALT)
int QuicChromiumPacketReader::StartReadingMultiplePackets() {
  // Packets handled here reuse the time read for the yield check.
  quic::QuicChromiumClock::ScopedApproximateNow approximate_now_scope;
  for (;;) {
if (read_pending_)
      return OK;

    if (num_packets_read_ == 0)
      yield_after_ = clock_->Now() + yield_after_duration_;

    CHECK(socket_);
    read_pending_ = true;
    int rv = socket_->ReadMultiplePackets(
        &read_results_, kReadBufferSize,
        base::BindOnce(&QuicChromiumPacketReader::OnReadMultiplePacketComplete,
                       weak_factory_.GetWeakPtr()));
    if (rv == ERR_NOT_IMPLEMENTED) {
      // The platform reports that ReadMultiplePackets is not implemented.
      if (read_results_.buffer) {
        read_results_.ReleaseBuffer();
      }
      read_pending_ = false;
      return rv;
    }

    UMA_HISTOGRAM_BOOLEAN("Net.QuicSession.AsyncRead", rv == ERR_IO_PENDING);
    if (rv == ERR_IO_PENDING) {
      if (read_results_.buffer) {
        read_results_.ReleaseBuffer();
      }
      num_packets_read_ = 0;
      return rv;
    }

    if (++num_packets_read_ > yield_after_packets_ ||
        clock_->Now() > yield_after_) {
      num_packets_read_ = 0;
      // Data was read, process it.
      // Schedule the work through the message loop to 1) prevent infinite
      // recursion and 2) avoid blocking the thread for too long.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &QuicChromiumPacketReader::OnReadMultiplePacketComplete,
              weak_factory_.GetWeakPtr(), rv));
    } else {
      if (!ProcessMultiplePacketReadResult(rv)) {
        return rv < 0 ? rv : OK;
      }
    }
  }
}

bool QuicChromiumPacketReader::ProcessMultiplePacketReadResult(int result) {
  quic::QuicChromiumClock::ScopedApproximateNow approximate_now_scope;
  // Move the batch out of |read_results_| before invoking the visitor so any
  // reentrant read or CloseSocket() cannot clobber it and the backing buffer is
  // always recycled on exit, even if OnPacket() destroys |this|.
  Socket::ReadPacketResults read_results = std::move(read_results_);
  read_pending_ = false;
  if (result <= 0 && net_log_.IsCapturing()) {
    net_log_.AddEventWithIntParams(NetLogEventType::QUIC_READ_ERROR,
                                   "net_error", result);
  }
  if (result == 0) {
    // 0-length UDP packets are legal but useless, ignore them.
    return true;
  }
  if (result == ERR_MSG_TOO_BIG) {
    // This indicates that we received a UDP packet larger than our receive
    // buffer, ignore it.
    return true;
  }
  if (result < 0) {
    // Report all other errors to the visitor.
    return visitor_->OnReadError(result, socket_.get());
  }

  // Since we only work on connected sockets, the local and peer address don't
  // change from packet to packet.
  IPEndPoint local_address;
  IPEndPoint peer_address;
  socket_->GetLocalAddress(&local_address);
  socket_->GetPeerAddress(&peer_address);
  quic::QuicSocketAddress quick_local_address =
      ToQuicSocketAddress(local_address);
  quic::QuicSocketAddress quick_peer_address =
      ToQuicSocketAddress(peer_address);

  auto self = weak_factory_.GetWeakPtr();
  struct Socket::ReadPacketResult* read_packet = read_results.packets;
  for (int p = 0; p < read_results.result; ++p, ++read_packet) {
    if (read_packet->result <= 0) {
      continue;
    }
    quic::QuicReceivedPacket packet(read_packet->buffer, read_packet->result,
                                    clock_->ApproximateNow());
    if (!(visitor_->OnPacket(packet, quick_local_address, quick_peer_address) &&
          self)) {
      return false;
    }
  }

  // Retain the scratch buffer across back-to-back iterations of the burst-drain
  // loop so StartReadingMultiplePackets() skips pool acquire/release until the
  // burst drains (ERR_IO_PENDING) or the socket closes.
  if (!read_pending_ && !read_results_.buffer) {
    read_results.result = 0;
    read_results_ = std::move(read_results);
  }

  return true;
}

void QuicChromiumPacketReader::OnReadMultiplePacketComplete(int result) {
  if (ProcessMultiplePacketReadResult(result))
    StartReadingMultiplePackets();
}

#endif

void QuicChromiumPacketReader::StartReading() {
#if BUILDFLAG(IS_COBALT)
  // Packets handled here reuse the time read for the yield check.
  quic::QuicChromiumClock::ScopedApproximateNow approximate_now_scope;
  if (try_reading_multiple_packets_) {
    int rv = StartReadingMultiplePackets();
    if (rv == OK || rv == ERR_IO_PENDING) {
      // If there was no error, or a callback was scheduled, there is no need
      // to attempt single packet reading.
      return;
    }
    if (rv == ERR_NOT_IMPLEMENTED || rv == ERR_MSG_TOO_BIG) {
      // Remember that the platform reported that ReadMultiplePackets is not
      // implemented or is returning datagrams that are too large.
      try_reading_multiple_packets_ = false;
      read_results_.clear();
    } else {
      // Other errors (such as ERR_CONNECTION_REFUSED) were already delivered to
      // visitor_->OnReadError(), which may have closed the socket. Do not fall
      // through into single-packet Read() or lazily allocate read_buffer_.
      return;
    }
  }
#endif

  for (;;) {
    if (read_pending_)
      return;

    if (num_packets_read_ == 0)
      yield_after_ = clock_->Now() + yield_after_duration_;

    CHECK(socket_);
#if BUILDFLAG(IS_COBALT)
    if (!read_buffer_) {
      read_buffer_ = base::MakeRefCounted<IOBufferWithSize>(kReadBufferSize);
    }
#endif
    read_pending_ = true;
    int rv =
        socket_->Read(read_buffer_.get(), read_buffer_->size(),
                      base::BindOnce(&QuicChromiumPacketReader::OnReadComplete,
                                     weak_factory_.GetWeakPtr()));
    UMA_HISTOGRAM_BOOLEAN("Net.QuicSession.AsyncRead", rv == ERR_IO_PENDING);
    if (rv == ERR_IO_PENDING) {
      num_packets_read_ = 0;
      return;
    }

    if (++num_packets_read_ > yield_after_packets_ ||
        clock_->Now() > yield_after_) {
      num_packets_read_ = 0;
      // Data was read, process it.
      // Schedule the work through the message loop to 1) prevent infinite
      // recursion and 2) avoid blocking the thread for too long.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&QuicChromiumPacketReader::OnReadComplete,
                                    weak_factory_.GetWeakPtr(), rv));
    } else {
      if (!ProcessReadResult(rv)) {
        return;
      }
    }
  }
}

void QuicChromiumPacketReader::CloseSocket() {
  socket_->Close();
#if BUILDFLAG(IS_COBALT)
  // Cancel any queued yielded read completions before releasing buffers so a
  // post-close task never dereferences cleared buffers or reads a closed fd.
  weak_factory_.InvalidateWeakPtrs();
  read_results_.ReleaseBuffer();
  read_buffer_.reset();
#endif
}

static_assert(static_cast<EcnCodePoint>(quic::ECN_NOT_ECT) == ECN_NOT_ECT &&
                  static_cast<EcnCodePoint>(quic::ECN_ECT1) == ECN_ECT1 &&
                  static_cast<EcnCodePoint>(quic::ECN_ECT0) == ECN_ECT0 &&
                  static_cast<EcnCodePoint>(quic::ECN_CE) == ECN_CE,
              "Mismatch ECN codepoint values");
bool QuicChromiumPacketReader::ProcessReadResult(int result) {
#if BUILDFLAG(IS_COBALT)
  quic::QuicChromiumClock::ScopedApproximateNow approximate_now_scope;
#endif
  read_pending_ = false;
  if (result <= 0 && net_log_.IsCapturing()) {
    net_log_.AddEventWithIntParams(NetLogEventType::QUIC_READ_ERROR,
                                   "net_error", result);
  }
  if (result == 0) {
    // 0-length UDP packets are legal but useless, ignore them.
    return true;
  }
  if (result == ERR_MSG_TOO_BIG) {
    // This indicates that we received a UDP packet larger than our receive
    // buffer, ignore it.
    return true;
  }
  if (result < 0) {
    // Report all other errors to the visitor.
    return visitor_->OnReadError(result, socket_.get());
  }

#if BUILDFLAG(IS_COBALT)
  // Keep the backing buffer alive across OnPacket() in case the visitor closes
  // the socket (which resets read_buffer_) or destroys |this|.
  scoped_refptr<IOBufferWithSize> read_buffer = read_buffer_;
  quic::QuicReceivedPacket packet(read_buffer->data(), result,
                                  clock_->ApproximateNow());
#else
  DscpAndEcn tos = socket_->GetLastTos();
  quic::QuicEcnCodepoint ecn = static_cast<quic::QuicEcnCodepoint>(tos.ecn);
  quic::QuicReceivedPacket packet(read_buffer_->data(), result, clock_->Now(),
                                  /*owns_buffer=*/false, /*ttl=*/0,
                                  /*ttl_valid=*/true,
                                  /*packet_headers=*/nullptr,
                                  /*headers_length=*/0,
                                  /*owns_header_buffer=*/false, ecn);
#endif
  IPEndPoint local_address;
  IPEndPoint peer_address;
  socket_->GetLocalAddress(&local_address);
  socket_->GetPeerAddress(&peer_address);
  auto self = weak_factory_.GetWeakPtr();
  // Notifies the visitor that |this| reader gets a new packet, which may delete
  // |this| if |this| is a connectivity probing reader.
  return visitor_->OnPacket(packet, ToQuicSocketAddress(local_address),
                            ToQuicSocketAddress(peer_address)) &&
         self;
}

void QuicChromiumPacketReader::OnReadComplete(int result) {
  if (ProcessReadResult(result)) {
    StartReading();
  }
}

}  // namespace net
