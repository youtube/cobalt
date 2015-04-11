// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dial/dial_udp_server.h"

#include <arpa/inet.h>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_util.h"
#include "net/dial/dial_system_config.h"
#include "net/dial/dial_udp_socket_factory.h"
#include "net/server/http_server.h"
#include "net/server/http_server_request_info.h"

namespace net {

namespace { // anonymous

const char* kDialStRequest = "urn:dial-multiscreen-org:service:dial:1";

// Get the INADDR_ANY address.
IPEndPoint GetAddressForAllInterfaces(unsigned short port) {
  SockaddrStorage any_addr;
  struct sockaddr_in *in = (struct sockaddr_in *)any_addr.addr;
  in->sin_family = AF_INET;
  in->sin_port = htons(port);
  in->sin_addr.s_addr = INADDR_ANY;

  IPEndPoint addr;
  ignore_result(addr.FromSockAddr(any_addr.addr, any_addr.addr_len));
  return addr;
}

}  // namespace

DialUdpServer::DialUdpServer(const std::string& location_url,
                             const std::string& server_agent)
  : factory_(new DialUdpSocketFactory()),
    location_url_(location_url),
    server_agent_(server_agent),
    thread_("dial_udp_server"),
    is_running_(false) {
  DCHECK(!location_url_.empty());
  thread_.StartWithOptions(base::Thread::Options(MessageLoop::TYPE_IO, 0));
  Start();
}

DialUdpServer::~DialUdpServer() {
  Stop();
}

void DialUdpServer::CreateAndBind() {
  DCHECK_EQ(thread_.message_loop(), MessageLoop::current());
  socket_ = factory_->CreateAndBind(GetAddressForAllInterfaces(1900), this);
  DLOG(INFO) << "Starting the Dial UDP Server";
  DCHECK(socket_);
}

void DialUdpServer::Shutdown() {
  DCHECK_EQ(thread_.message_loop(), MessageLoop::current());
  location_url_.clear();
  socket_ = NULL;
}

void DialUdpServer::Start() {
  DCHECK(!is_running_);
  is_running_ = true;
  thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &DialUdpServer::CreateAndBind, base::Unretained(this)));
}

void DialUdpServer::Stop() {
  DCHECK(is_running_);
  thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &DialUdpServer::Shutdown, base::Unretained(this)));
}

void DialUdpServer::DidClose(UDPListenSocket* server) {
  DLOG(INFO) << "Stopping the Dial UDP Server";
}

void DialUdpServer::DidRead(UDPListenSocket* server,
                            const char* data,
                            int len,
                            const IPEndPoint* address) {
  DCHECK_EQ(thread_.message_loop(), MessageLoop::current());
  if (!socket_) {
    return;
  }
  std::string st_request_id;
  // If M-Search request was valid, send response. Else, keep quiet.
  if (ParseSearchRequest(std::string(data, len))) {
    std::string response = ConstructSearchResponse();
    socket_->SendTo(*address, response);
  }
}


// Parse a request to make sure it is a M-Search.
bool DialUdpServer::ParseSearchRequest(const std::string& request) {
  HttpServerRequestInfo info;
  if (!HttpServer::ParseHeaders(request, &info)) {
    DVLOG(1) << "Failed parsing SSDP headers: " << request;
    return false;
  }

  if (!IsValidMSearchRequest(info)) {
    return false;
  }

  std::string st_request = info.GetHeaderValue("ST");
  ignore_result(TrimWhitespaceASCII(st_request, TRIM_ALL, &st_request));

  if (st_request != kDialStRequest) {
    DVLOG(1) << "Received incorrect ST headers: " << st_request;
    return false;
  }

  // The User-Agent header is supposed to be case-insensitive, but
  // the map that holds it has a case-sensitive search.
  // I could be more careful, but hey, it's a DVLOG anyway.
  DVLOG(1) << "Dial User-Agent: " << info.GetHeaderValue("USER-AGENT");
  DVLOG(1) << "Dial User-Agent: " << info.GetHeaderValue("User-Agent");

  return true;
}

bool DialUdpServer::IsValidMSearchRequest(const HttpServerRequestInfo& info) {
  if (info.method != "M-SEARCH") {
    DVLOG(1) << "Invalid M-Search: SSDP method incorrect.";
    return false;
  }

  if (info.path != "*") {
    DVLOG(1) << "Invalid M-Search: SSDP path incorrect.";
    return false;
  }

  if (!info.data.empty()) {
    DVLOG(1) << "Invalid M-Search: SSDP request contains a body.";
    return false;
  }

  return true;
}

// Since we are constructing a response from user-generated string,
// ensure all user-generated strings pass through StringPrintf.
const std::string DialUdpServer::ConstructSearchResponse() const {
  DCHECK(!location_url_.empty());

  std::string ret("HTTP/1.1 200 OK\r\n");
  ret.append(base::StringPrintf("LOCATION: %s\r\n", location_url_.c_str()));
  ret.append("CACHE-CONTROL: max-age=1800\r\n");
  ret.append("EXT:\r\n");
  ret.append("BOOTID.UPNP.ORG: 1\r\n");
  ret.append("CONFIGID.UPNP.ORG: 1\r\n");
  ret.append(base::StringPrintf("SERVER: %s\r\n", server_agent_.c_str()));
  ret.append(base::StringPrintf("ST: %s\r\n", kDialStRequest));
  ret.append(base::StringPrintf("USN: uuid:%s::%s\r\n",
                                DialSystemConfig::GetInstance()->model_uuid(),
                                kDialStRequest));
  ret.append("\r\n");
  return ret;
}

} // namespace net

