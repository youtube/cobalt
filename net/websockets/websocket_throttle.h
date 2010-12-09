// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_THROTTLE_H_
#define NET_WEBSOCKETS_WEBSOCKET_THROTTLE_H_
#pragma once

#include <deque>
#include <string>

#include "base/hash_tables.h"

template <typename T> struct DefaultSingletonTraits;

namespace net {

class SocketStream;
class WebSocketJob;

// SocketStreamThrottle for WebSocket protocol.
// Implements the client-side requirements in the spec.
// http://tools.ietf.org/html/draft-hixie-thewebsocketprotocol
// 4.1 Handshake
//   1.   If the user agent already has a Web Socket connection to the
//        remote host (IP address) identified by /host/, even if known by
//        another name, wait until that connection has been established or
//        for that connection to have failed.
class WebSocketThrottle {
 public:
  // Returns the singleton instance.
  static WebSocketThrottle* GetInstance();

  // Puts |job| in |queue_| and queues for the destination addresses
  // of |job|.
  // If other job is using the same destination address, set |job| waiting.
  void PutInQueue(WebSocketJob* job);

  // Removes |job| from |queue_| and queues for the destination addresses
  // of |job|.
  void RemoveFromQueue(WebSocketJob* job);

  // Checks sockets waiting in |queue_| and check the socket is the front of
  // every queue for the destination addresses of |socket|.
  // If so, the socket can resume estabilshing connection, so wake up
  // the socket.
  void WakeupSocketIfNecessary();

 private:
  typedef std::deque<WebSocketJob*> ConnectingQueue;
  typedef base::hash_map<std::string, ConnectingQueue*> ConnectingAddressMap;

  WebSocketThrottle();
  virtual ~WebSocketThrottle();
  friend struct DefaultSingletonTraits<WebSocketThrottle>;

  // Key: string of host's address.  Value: queue of sockets for the address.
  ConnectingAddressMap addr_map_;

  // Queue of sockets for websockets in opening state.
  ConnectingQueue queue_;
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_THROTTLE_H_
