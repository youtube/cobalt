// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/p2p/base/fake_connection_factory.h"

#include "third_party/webrtc/p2p/base/basic_packet_socket_factory.h"
#include "third_party/webrtc/p2p/base/fake_port_allocator.h"
#include "third_party/webrtc/p2p/base/port.h"
#include "third_party/webrtc_overrides/rtc_base/fake_socket_factory.h"

namespace blink {

FakeConnectionFactory::FakeConnectionFactory(rtc::Thread* thread,
                                             base::WaitableEvent* readyEvent)
    : readyEvent_(readyEvent),
      sf_(std::make_unique<blink::FakeSocketFactory>()),
      socket_factory_(
          std::make_unique<rtc::BasicPacketSocketFactory>(sf_.get())),
      allocator_(
          std::make_unique<cricket::FakePortAllocator>(thread,
                                                       socket_factory_.get(),
                                                       nullptr)) {}

void FakeConnectionFactory::Prepare(uint32_t allocator_flags) {
  if (sessions_.size() > 0) {
    return;
  }
  allocator_->set_flags(allocator_flags);
  auto session = allocator_->CreateSession("test", /*component=*/1, "ice_ufrag",
                                           "ice_password");
  session->set_generation(0);
  session->SignalPortReady.connect(this, &FakeConnectionFactory::OnPortReady);
  session->StartGettingPorts();
  sessions_.push_back(std::move(session));
}

cricket::Connection* FakeConnectionFactory::CreateConnection(
    CandidateType type,
    base::StringPiece remote_ip,
    int remote_port,
    int priority) {
  if (ports_.size() == 0) {
    return nullptr;
  }
  cricket::Candidate remote =
      CreateUdpCandidate(GetPortType(type), remote_ip, remote_port, priority);
  cricket::Connection* conn = nullptr;
  for (auto port : ports_) {
    if (port->SupportsProtocol(remote.protocol())) {
      conn = port->GetConnection(remote.address());
      if (!conn) {
        conn = port->CreateConnection(remote,
                                      cricket::PortInterface::ORIGIN_MESSAGE);
      }
    }
  }
  return conn;
}

base::StringPiece FakeConnectionFactory::GetPortType(CandidateType type) {
  switch (type) {
    case CandidateType::LOCAL:
      return cricket::LOCAL_PORT_TYPE;
    case CandidateType::SRFLX:
      return cricket::STUN_PORT_TYPE;
    case CandidateType::PRFLX:
      return cricket::PRFLX_PORT_TYPE;
    case CandidateType::RELAY:
      return cricket::RELAY_PORT_TYPE;
  }
}

void FakeConnectionFactory::OnPortReady(cricket::PortAllocatorSession* session,
                                        cricket::PortInterface* port) {
  ports_.push_back(port);
  if (!readyEvent_->IsSignaled()) {
    readyEvent_->Signal();
  }
}

cricket::Candidate FakeConnectionFactory::CreateUdpCandidate(
    base::StringPiece type,
    base::StringPiece ip,
    int port,
    int priority,
    base::StringPiece ufrag) {
  cricket::Candidate c;
  c.set_address(rtc::SocketAddress(ip.data(), port));
  c.set_component(::cricket::ICE_CANDIDATE_COMPONENT_DEFAULT);
  c.set_protocol(::cricket::UDP_PROTOCOL_NAME);
  c.set_priority(priority);
  c.set_username(ufrag.data());
  c.set_type(type.data());
  return c;
}

}  // namespace blink
