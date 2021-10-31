// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DIAL_DIAL_UDP_SERVER_H
#define NET_DIAL_DIAL_UDP_SERVER_H

#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread.h"
#include "net/base/net_export.h"
#include "net/socket/udp_socket.h"

namespace net {

class IPEndPoint;
class UdpSocketFactory;
class HttpServerRequestInfo;

class NET_EXPORT DialUdpServer {
 public:
  DialUdpServer(const std::string& location_url,
                const std::string& server_agent);
  virtual ~DialUdpServer();

  virtual void DidRead(int rv);

  virtual void DidClose(UDPSocket* sock);

  void WriteComplete(scoped_refptr<WrappedIOBuffer>,
                     std::unique_ptr<std::string>,
                     int rv);

 private:
  FRIEND_TEST_ALL_PREFIXES(DialUdpServerTest, ParseSearchRequest);

  void Start();
  void Stop();

  // Create the listen socket. Runs on a separate thread.
  void CreateAndBind();
  void Shutdown();
  void AcceptAndProcessConnection();

  // Construct the appropriate search response.
  std::string ConstructSearchResponse() const;

  // Parse a request to make sure it is a M-Search.
  static bool ParseSearchRequest(const std::string& request);

  // Is the valid SSDP request a valid M-Search request too ?
  static bool IsValidMSearchRequest(const HttpServerRequestInfo& info);

  std::unique_ptr<UDPSocket> socket_;
  std::unique_ptr<UdpSocketFactory> factory_;

  // Value to pass in LOCATION: header
  std::string location_url_;

  // Value to pass in SERVER: header
  std::string server_agent_;

  base::Thread thread_;
  THREAD_CHECKER(thread_checker_);

  bool is_running_;

  scoped_refptr<IOBuffer> read_buf_;
  IPEndPoint client_address_;
};

}  // namespace net

#endif  // NET_DIAL_DIAL_UDP_SERVER_H
