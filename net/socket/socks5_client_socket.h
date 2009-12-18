// Copyright (c) 2009 The Chromium Authors. All rights reserved.
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
#include "net/socket/client_socket.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

namespace net {

class LoadLog;

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
  // SOCKS5 supports three modes of specifying connection endpoints:
  //  (1) as an IPv4 address.
  //  (2) as an IPv6 address.
  //  (3) as a hostname string.
  //
  // To select mode (3), pass NULL for |host_resolver|.
  //
  // Otherwise if a non-NULL |host_resolver| is given, Connect() will first
  // try to resolve the hostname using |host_resolver|, and pass that
  // resolved address to the proxy server. If the resolve failed, Connect()
  // will fall-back to mode (3) and simply send the unresolved hosname string
  // to the SOCKS v5 proxy server.
  //
  // Passing NULL for |host_resolver| is the recommended default.
  SOCKS5ClientSocket(ClientSocket* transport_socket,
                     const HostResolver::RequestInfo& req_info,
                     HostResolver* host_resolver);

  // On destruction Disconnect() is called.
  virtual ~SOCKS5ClientSocket();

  // ClientSocket methods:

  // Does the SOCKS handshake and completes the protocol.
  virtual int Connect(CompletionCallback* callback, LoadLog* load_log);
  virtual void Disconnect();
  virtual bool IsConnected() const;
  virtual bool IsConnectedAndIdle() const;

  // Socket methods:
  virtual int Read(IOBuffer* buf, int buf_len, CompletionCallback* callback);
  virtual int Write(IOBuffer* buf, int buf_len, CompletionCallback* callback);

  virtual bool SetReceiveBufferSize(int32 size);
  virtual bool SetSendBufferSize(int32 size);

#if defined(OS_LINUX)
  virtual int GetPeerName(struct sockaddr* name, socklen_t* namelen);
#endif

 private:
  FRIEND_TEST(SOCKS5ClientSocketTest, IPv6Domain);
  FRIEND_TEST(SOCKS5ClientSocketTest, FailedDNS);
  FRIEND_TEST(SOCKS5ClientSocketTest, CompleteHandshake);

  enum State {
    STATE_RESOLVE_HOST,
    STATE_RESOLVE_HOST_COMPLETE,
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

  // State of the SOCKSv5 handshake. Before host resolution all connections
  // are kEndPointFailedDomain. If DNS lookup fails, we move to
  // kEndPointFailedDomain, otherwise the IPv4/IPv6 address as resolved.
  enum SocksEndPointAddressType {
    kEndPointUnresolved,
    kEndPointFailedDomain = 0x03,
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
  int DoResolveHost();
  int DoResolveHostComplete(int result);
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
  scoped_ptr<ClientSocket> transport_;

  State next_state_;
  SocksEndPointAddressType address_type_;

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

  // If non-NULL, we will use this host resolver to resolve DNS client-side
  // (and fall back to proxy-side resolving if it fails).
  // Otherwise, we will do proxy-side DNS resolving.
  scoped_ptr<SingleRequestHostResolver> host_resolver_;
  AddressList addresses_;
  HostResolver::RequestInfo host_request_info_;

  scoped_refptr<LoadLog> load_log_;

  DISALLOW_COPY_AND_ASSIGN(SOCKS5ClientSocket);
};

}  // namespace net

#endif  // NET_SOCKET_SOCKS5_CLIENT_SOCKET_H_
