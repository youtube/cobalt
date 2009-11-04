// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_THROTTLE_H_
#define NET_WEBSOCKETS_WEBSOCKET_THROTTLE_H_

#include "base/hash_tables.h"
#include "base/singleton.h"
#include "net/socket_stream/socket_stream_throttle.h"

namespace net {

// SocketStreamThrottle for WebSocket protocol.
// Implements the client-side requirements in the spec.
// http://tools.ietf.org/html/draft-hixie-thewebsocketprotocol
// 4.1 Handshake
//   1.   If the user agent already has a Web Socket connection to the
//        remote host (IP address) identified by /host/, even if known by
//        another name, wait until that connection has been established or
//        for that connection to have failed.
class WebSocketThrottle : public SocketStreamThrottle {
 public:
  virtual int OnStartOpenConnection(SocketStream* socket,
                                    CompletionCallback* callback);
  virtual int OnRead(SocketStream* socket, const char* data, int len,
                     CompletionCallback* callback);
  virtual int OnWrite(SocketStream* socket, const char* data, int len,
                      CompletionCallback* callback);
  virtual void OnClose(SocketStream* socket);

  static void Init();

 private:
  class WebSocketState;
  typedef std::deque<WebSocketState*> ConnectingQueue;
  typedef base::hash_map<std::string, ConnectingQueue*> ConnectingAddressMap;

  WebSocketThrottle();
  virtual ~WebSocketThrottle();
  friend struct DefaultSingletonTraits<WebSocketThrottle>;

  // Puts |socket| in |queue_| and queues for the destination addresses
  // of |socket|.  Also sets |state| as UserData of |socket|.
  // If other socket is using the same destination address, set |state| waiting.
  void PutInQueue(SocketStream* socket, WebSocketState* state);

  // Removes |socket| from |queue_| and queues for the destination addresses
  // of |socket|.  Also releases |state| from UserData of |socket|.
  void RemoveFromQueue(SocketStream* socket, WebSocketState* state);

  // Checks sockets waiting in |queue_| and check the socket is the front of
  // every queue for the destination addresses of |socket|.
  // If so, the socket can resume estabilshing connection, so wake up
  // the socket.
  void WakeupSocketIfNecessary();

  // Key: string of host's address.  Value: queue of sockets for the address.
  ConnectingAddressMap addr_map_;

  // Queue of sockets for websockets in opening state.
  ConnectingQueue queue_;
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_THROTTLE_H_
