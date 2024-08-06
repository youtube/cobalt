// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_CAST_TEST_LOOPBACK_TRANSPORT_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_CAST_TEST_LOOPBACK_TRANSPORT_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "third_party/chromium/media/cast/cast_environment.h"
#include "third_party/chromium/media/cast/net/cast_transport_config.h"

namespace base {
class SingleThreadTaskRunner;
class TickClock;
}  // namespace base

namespace media_m96 {
namespace cast {

namespace test {
class PacketPipe;
}  // namespace test

// Class that sends the packet to a receiver through a stack of PacketPipes.
class LoopBackTransport final : public PacketTransport {
 public:
  explicit LoopBackTransport(scoped_refptr<CastEnvironment> cast_environment);

  LoopBackTransport(const LoopBackTransport&) = delete;
  LoopBackTransport& operator=(const LoopBackTransport&) = delete;

  ~LoopBackTransport() final;

  bool SendPacket(PacketRef packet, base::OnceClosure cb) final;

  int64_t GetBytesSent() final;

  void StartReceiving(PacketReceiverCallbackWithStatus packet_receiver) final {}

  void StopReceiving() final {}

  // Initiailize this loopback transport.
  // Establish a flow of packets from |pipe| to |packet_receiver|.
  //
  // The data flow looks like:
  // SendPacket() -> |pipe| -> Fake loopback pipe -> |packet_receiver|.
  //
  // If |pipe| is NULL then the data flow looks like:
  // SendPacket() -> Fake loopback pipe -> |packet_receiver|.
  void Initialize(
      std::unique_ptr<test::PacketPipe> pipe,
      const PacketReceiverCallback& packet_receiver,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      const base::TickClock* clock);

 private:
  const scoped_refptr<CastEnvironment> cast_environment_;
  std::unique_ptr<test::PacketPipe> packet_pipe_;
  int64_t bytes_sent_;
};

}  // namespace cast
}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_CAST_TEST_LOOPBACK_TRANSPORT_H_
