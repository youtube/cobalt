// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SOCKS5_CLIENT_SOCKET_H_
#define NET_SOCKET_SOCKS5_CLIENT_SOCKET_H_

#include <string>

#include "base/logging.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "googleurl/src/gurl.h"
#include "net/base/address_list.h"
#include "net/base/completion_callback.h"
#include "net/base/host_resolver.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/socket/client_socket.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

namespace net {

class ClientSocketHandle;
class BoundNetLog;

// This ClientSocket is used to setup a SOCKSv5 handshake with a socks proxy.
// Currently no SOCKSv5 authentication is supported.
class SOCKS5ClientSocket : public ClientSocket {
 public:
  // Takes ownership of the |transport_socket|, which should already be
  // connected by the time Connect() is called.
  //
  // |req_info| contains the hostname and port to which the socket above will
  // communicate to via the SOCKS layer.
  //
  // Although SOCKS 5 supports 3 different modes of addressing, we will
  // always pass it a hostname. This means the DNS resolving is done
  // proxy side.
  SOCKS5ClientSocket(ClientSocketHandle* transport_socket,
                     const HostResolver::RequestInfo& req_info);

  // Deprecated constructor (http://crbug.com/37810) that takes a ClientSocket.
  SOCKS5ClientSocket(ClientSocket* transport_socket,
                     const HostResolver::RequestInfo& req_info);

  // On destruction Disconnect() is called.
  virtual ~SOCKS5ClientSocket();

  // ClientSocket methods:

  // Does the SOCKS handshake and completes the protocol.
  virtual int Connect(CompletionCallback* callback);
  virtual void Disconnect();
  virtual bool IsConnected() const;
  virtual bool IsConnectedAndIdle() const;
  virtual const BoundNetLog& NetLog() const { return net_log_; }

  // Socket methods:
  virtual int Read(IOBuffer* buf, int buf_len, CompletionCallback* callback);
  virtual int Write(IOBuffer* buf, int buf_len, CompletionCallback* callback);

  virtual bool SetReceiveBufferSize(int32 size);
  virtual bool SetSendBufferSize(int32 size);

  virtual int GetPeerAddress(AddressList* address) const;

 private:
  enum State {
    STATE_GREET_WRITE,
    STATE_GREET_WRITE_COMPLETE,
    STATE_GREET_READ,
    STATE_GREET_READ_COMPLETE,
    STATE_HANDSHAKE_WRITE,
    STATE_HANDSHAKE_WRITE_COMPLETE,
    STATE_HANDSHAKE_READ,
    STATE_HANDSHAKE_READ_COMPLETE,
    STATE_NONE,
  };

  // Addressing type that can be specified in requests or responses.
  enum SocksEndPointAddressType {
    kEndPointDomain = 0x03,
    kEndPointResolvedIPv4 = 0x01,
    kEndPointResolvedIPv6 = 0x04,
  };

  static const unsigned int kGreetReadHeaderSize;
  static const unsigned int kWriteHeaderSize;
  static const unsigned int kReadHeaderSize;
  static const uint8 kSOCKS5Version;
  static const uint8 kTunnelCommand;
  static const uint8 kNullByte;

  void DoCallback(int result);
  void OnIOComplete(int result);

  int DoLoop(int last_io_result);
  int DoHandshakeRead();
  int DoHandshakeReadComplete(int result);
  int DoHandshakeWrite();
  int DoHandshakeWriteComplete(int result);
  int DoGreetRead();
  int DoGreetReadComplete(int result);
  int DoGreetWrite();
  int DoGreetWriteComplete(int result);

  // Writes the SOCKS handshake buffer into |handshake|
  // and return OK on success.
  int BuildHandshakeWriteBuffer(std::string* handshake) const;

  CompletionCallbackImpl<SOCKS5ClientSocket> io_callback_;

  // Stores the underlying socket.
  scoped_ptr<ClientSocketHandle> transport_;

  State next_state_;

  // Stores the callback to the layer above, called on completing Connect().
  CompletionCallback* user_callback_;

  // This IOBuffer is used by the class to read and write
  // SOCKS handshake data. The length contains the expected size to
  // read or write.
  scoped_refptr<IOBuffer> handshake_buf_;

  // While writing, this buffer stores the complete write handshake data.
  // While reading, it stores the handshake information received so far.
  std::string buffer_;

  // This becomes true when the SOCKS handshake has completed and the
  // overlying connection is free to communicate.
  bool completed_handshake_;

  // These contain the bytes sent / received by the SOCKS handshake.
  size_t bytes_sent_;
  size_t bytes_received_;

  size_t read_header_size;

  HostResolver::RequestInfo host_request_info_;

  BoundNetLog net_log_;

  DISALLOW_COPY_AND_ASSIGN(SOCKS5ClientSocket);
};

}  // namespace net

#endif  // NET_SOCKET_SOCKS5_CLIENT_SOCKET_H_
