// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/pseudotcp_channel_factory.h"

#include <utility>

#include "base/functional/bind.h"
#include "net/base/net_errors.h"
#include "remoting/base/constants.h"
#include "remoting/protocol/datagram_channel_factory.h"
#include "remoting/protocol/p2p_datagram_socket.h"
#include "remoting/protocol/pseudotcp_adapter.h"

namespace remoting::protocol {

namespace {

// Value is chosen to balance the extra latency against the reduced load due to
// ACK traffic.
const int kTcpAckDelayMilliseconds = 10;

// Values for the TCP send and receive buffer size. This should be tuned to
// accommodate high latency network but not backlog the decoding pipeline.
const int kTcpReceiveBufferSize = 256 * 1024;
const int kTcpSendBufferSize = kTcpReceiveBufferSize + 30 * 1024;

}  // namespace

PseudoTcpChannelFactory::PseudoTcpChannelFactory(
    DatagramChannelFactory* datagram_channel_factory)
    : datagram_channel_factory_(datagram_channel_factory) {}

PseudoTcpChannelFactory::~PseudoTcpChannelFactory() {
  // CancelChannelCreation() is expected to be called before destruction.
  DCHECK(pending_sockets_.empty());
}

void PseudoTcpChannelFactory::CreateChannel(const std::string& name,
                                            ChannelCreatedCallback callback) {
  datagram_channel_factory_->CreateChannel(
      name, base::BindOnce(&PseudoTcpChannelFactory::OnDatagramChannelCreated,
                           base::Unretained(this), name, std::move(callback)));
}

void PseudoTcpChannelFactory::CancelChannelCreation(const std::string& name) {
  auto it = pending_sockets_.find(name);
  if (it == pending_sockets_.end()) {
    datagram_channel_factory_->CancelChannelCreation(name);
  } else {
    delete it->second;
    pending_sockets_.erase(it);
  }
}

void PseudoTcpChannelFactory::OnDatagramChannelCreated(
    const std::string& name,
    ChannelCreatedCallback callback,
    std::unique_ptr<P2PDatagramSocket> datagram_socket) {
  PseudoTcpAdapter* adapter = new PseudoTcpAdapter(std::move(datagram_socket));
  pending_sockets_[name] = adapter;

  adapter->SetSendBufferSize(kTcpSendBufferSize);
  adapter->SetReceiveBufferSize(kTcpReceiveBufferSize);
  adapter->SetNoDelay(true);
  adapter->SetAckDelay(kTcpAckDelayMilliseconds);

  // TODO(sergeyu): This is a hack to improve latency of the video channel.
  // Consider removing it once we have better flow control implemented.
  if (name == kVideoChannelName) {
    adapter->SetWriteWaitsForSend(true);
  }

  net::CompletionOnceCallback returned_callback = adapter->Connect(
      base::BindOnce(&PseudoTcpChannelFactory::OnPseudoTcpConnected,
                     base::Unretained(this), name, std::move(callback)));
  if (returned_callback) {
    std::move(returned_callback).Run(net::ERR_FAILED);
  }
}

void PseudoTcpChannelFactory::OnPseudoTcpConnected(
    const std::string& name,
    ChannelCreatedCallback callback,
    int result) {
  auto it = pending_sockets_.find(name);
  DCHECK(it != pending_sockets_.end());
  std::unique_ptr<P2PStreamSocket> socket(it->second);
  pending_sockets_.erase(it);

  if (result != net::OK) {
    socket.reset();
  }

  std::move(callback).Run(std::move(socket));
}

}  // namespace remoting::protocol
