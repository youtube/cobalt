// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Collect metrics of SocketStream usage.
// TODO(ukai): collect WebSocket specific metrics (e.g. handshake time, etc).

#ifndef NET_SOCKET_STREAM_SOCKET_STREAM_METRICS_H_
#define NET_SOCKET_STREAM_SOCKET_STREAM_METRICS_H_
#pragma once

#include "base/basictypes.h"
#include "base/time.h"

class GURL;

namespace net {

class SocketStreamMetrics {
 public:
  enum ProtocolType {
    PROTOCOL_UNKNOWN,
    PROTOCOL_WEBSOCKET,
    PROTOCOL_WEBSOCKET_SECURE,
    NUM_PROTOCOL_TYPES,
  };

  enum ConnectionType {
    CONNECTION_NONE,
    ALL_CONNECTIONS,
    TUNNEL_CONNECTION,
    SOCKS_CONNECTION,
    SSL_CONNECTION,
    NUM_CONNECTION_TYPES,
  };

  explicit SocketStreamMetrics(const GURL& url);
  ~SocketStreamMetrics();

  void OnWaitConnection();
  void OnStartConnection();
  void OnTunnelProxy();
  void OnSOCKSProxy();
  void OnSSLConnection();
  void OnConnected();
  void OnRead(int len);
  void OnWrite(int len);
  void OnClose();

 private:
  void CountProtocolType(ProtocolType protocol_type);
  void CountConnectionType(ConnectionType connection_type);

  base::TimeTicks wait_start_time_;
  base::TimeTicks connect_start_time_;
  base::TimeTicks connect_establish_time_;
  int received_bytes_;
  int received_counts_;
  int sent_bytes_;
  int sent_counts_;

  DISALLOW_COPY_AND_ASSIGN(SocketStreamMetrics);
};

}  // namespace net

#endif  // NET_SOCKET_STREAM_SOCKET_STREAM_METRICS_H_
