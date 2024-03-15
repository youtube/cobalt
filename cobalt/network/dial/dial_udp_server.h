// Copyright 2023 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_NETWORK_DIAL_DIAL_UDP_SERVER_H_
#define COBALT_NETWORK_DIAL_DIAL_UDP_SERVER_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread.h"
#include "net/base/net_export.h"
#include "net/server/http_server_request_info.h"
#include "net/socket/udp_socket.h"

namespace cobalt {
namespace network {

class UdpSocketFactory;

class NET_EXPORT DialUdpServer {
 public:
  DialUdpServer(const std::string& location_url,
                const std::string& server_agent);
  virtual ~DialUdpServer();

  virtual void DidRead(int rv);

  virtual void DidClose(net::UDPSocket* sock);

  void WriteComplete(scoped_refptr<net::WrappedIOBuffer>,
                     std::unique_ptr<std::string>, int rv);

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
  static bool IsValidMSearchRequest(const net::HttpServerRequestInfo& info);

  std::unique_ptr<net::UDPSocket> socket_;
  std::unique_ptr<UdpSocketFactory> factory_;

  // Value to pass in LOCATION: header
  std::string location_url_;

  // Value to pass in SERVER: header
  std::string server_agent_;

  base::Thread thread_;
  THREAD_CHECKER(thread_checker_);

  bool is_running_;

  scoped_refptr<net::IOBuffer> read_buf_;
  net::IPEndPoint client_address_;
};

}  // namespace network
}  // namespace cobalt

#endif  // COBALT_NETWORK_DIAL_DIAL_UDP_SERVER_H_
