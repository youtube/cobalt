// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_CAST_TEST_LOOPBACK_TRANSPORT_H_
#define COBALT_MEDIA_CAST_TEST_LOOPBACK_TRANSPORT_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "media/cast/cast_environment.h"
#include "media/cast/net/cast_transport_config.h"

namespace base {
class SingleThreadTaskRunner;
class TickClock;
}  // namespace base

namespace media {
namespace cast {

namespace test {
class PacketPipe;
}  // namespace test

// Class that sends the packet to a receiver through a stack of PacketPipes.
class LoopBackTransport : public PacketTransport {
 public:
  explicit LoopBackTransport(scoped_refptr<CastEnvironment> cast_environment);
  ~LoopBackTransport() final;

  bool SendPacket(PacketRef packet, const base::Closure& cb) final;

  int64_t GetBytesSent() final;

  void StartReceiving(
      const PacketReceiverCallbackWithStatus& packet_receiver) final {}

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
      base::TickClock* clock);

 private:
  const scoped_refptr<CastEnvironment> cast_environment_;
  std::unique_ptr<test::PacketPipe> packet_pipe_;
  int64_t bytes_sent_;

  DISALLOW_COPY_AND_ASSIGN(LoopBackTransport);
};

}  // namespace cast
}  // namespace media

#endif  // COBALT_MEDIA_CAST_TEST_LOOPBACK_TRANSPORT_H_
