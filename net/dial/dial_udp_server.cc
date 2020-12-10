// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dial/dial_udp_server.h"

#if defined(STARBOARD)
#include "starboard/client_porting/poem/inet_poem.h"
#else
#include <arpa/inet.h>
#endif
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/hash.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_string_util.h"
#include "net/dial/dial_system_config.h"
#include "net/dial/dial_udp_socket_factory.h"
#include "net/server/http_server.h"
#include "net/server/http_server_request_info.h"
#include "starboard/types.h"

namespace net {

namespace {  // anonymous

const char* kDialStRequest = "urn:dial-multiscreen-org:service:dial:1";
const int kReadBufferSize = 500 * 1024;

// Get the INADDR_ANY address.
IPEndPoint GetAddressForAllInterfaces(unsigned short port) {
#if defined(STARBOARD)
  return IPEndPoint::GetForAllInterfaces(port);
#else
  SockaddrStorage any_addr;
  struct sockaddr_in* in = (struct sockaddr_in*)any_addr.addr;
  in->sin_family = AF_INET;
  in->sin_port = htons(port);
  in->sin_addr.s_addr = INADDR_ANY;

  IPEndPoint addr;
  ignore_result(addr.FromSockAddr(any_addr.addr, any_addr.addr_len));
  return addr;
#endif  // !defined(STARBOARD)
}

}  // namespace

DialUdpServer::DialUdpServer(const std::string& location_url,
                             const std::string& server_agent)
    : factory_(new DialUdpSocketFactory()),
      location_url_(location_url),
      server_agent_(server_agent),
      thread_("dial_udp_server"),
      is_running_(false),
      read_buf_(new IOBuffer(kReadBufferSize)) {
  DCHECK(!location_url_.empty());
  DETACH_FROM_THREAD(thread_checker_);
  thread_.StartWithOptions(
      base::Thread::Options(base::MessageLoop::TYPE_IO, 0));
  Start();
}

DialUdpServer::~DialUdpServer() {
  Stop();
}

void DialUdpServer::CreateAndBind() {
  DCHECK_EQ(thread_.message_loop(), base::MessageLoop::current());
  // The thread_checker_ will bind to thread_ from this point on.
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  socket_ = factory_->CreateAndBind(GetAddressForAllInterfaces(1900));
  if (!socket_) {
    DLOG(WARNING) << "Failed to bind socket for Dial UDP Server";
    return;
  }

  AcceptAndProcessConnection();
}

void DialUdpServer::Shutdown() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  location_url_.clear();
  socket_.reset();
  factory_.reset();
  is_running_ = false;
}

void DialUdpServer::Start() {
  DCHECK(!is_running_);
  is_running_ = true;
  thread_.message_loop()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&DialUdpServer::CreateAndBind, base::Unretained(this)));
}

void DialUdpServer::Stop() {
  DCHECK(is_running_);
  thread_.message_loop()->task_runner()->PostBlockingTask(
      FROM_HERE, base::Bind(&DialUdpServer::Shutdown,
                            base::Unretained(this)));
  thread_.Stop();
}

void DialUdpServer::AcceptAndProcessConnection() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!is_running_ || !socket_.get()) {
    return;
  }
  auto err_code = socket_->RecvFrom(
      read_buf_.get(), kReadBufferSize, &client_address_,
      base::Bind(&DialUdpServer::DidRead, base::Unretained(this)));
  if (err_code > 0) {
    // RecvFrom can also return the number of received bytes right away as well.
    DidRead(err_code);
  } else if (err_code != ERR_IO_PENDING) {
    DCHECK(err_code == OK) << "RecvFrom returned bad error code: " << err_code;
  }
  // otherwise, RecvFrom returned -1 and will execute DidRead when any data is
  // received.
}

void DialUdpServer::DidClose(UDPSocket* server) {}

void DialUdpServer::DidRead(int bytes_read) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!socket_ || !read_buf_.get() || !read_buf_->data()) {
    return;
  }
  if (bytes_read <= 0) {
    LOG(WARNING) << "Dial server socket read error: " << bytes_read;
    return;
  }
  // If M-Search request was valid, send response. Else, keep quiet.
  if (ParseSearchRequest(std::string(read_buf_->data()))) {
    auto response = std::make_unique<std::string>();
    *response = std::move(ConstructSearchResponse());
    // Using the fake IOBuffer to avoid another copy.
    scoped_refptr<WrappedIOBuffer> fake_buffer =
        new WrappedIOBuffer(response->data());
    // After optimization, some compiler will dereference and get response size
    // later than passing response.
    auto response_size = response->size();
    int result = socket_->SendTo(
        fake_buffer.get(), response_size, client_address_,
        base::Bind(&DialUdpServer::WriteComplete, base::Unretained(this),
                   fake_buffer, base::Passed(&response)));
    if (result == ERR_IO_PENDING) {
      // WriteComplete is responsible for posting the next callback to accept
      // connection.
      return;
    } else if (result < 0) {
      LOG(ERROR) << "UDPSocket SendTo error: " << result;
    }
  }

  // Register a watcher on the message loop and wait for the next dial message.
  // If we call AcceptAndProcessConnection directly, the function could call
  // DidRead and quickly increase stack size or even loop infinitely if the
  // socket can always provide messages through RecvFrom.
  thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&DialUdpServer::AcceptAndProcessConnection,
                            base::Unretained(this)));
}

void DialUdpServer::WriteComplete(scoped_refptr<WrappedIOBuffer>,
                                  std::unique_ptr<std::string>,
                                  int rv) {
  if (rv < 0) {
    LOG(ERROR) << "UDPSocket completion callback error: " << rv;
  }
  thread_.task_runner()->PostTask(
      FROM_HERE, base::Bind(&DialUdpServer::AcceptAndProcessConnection,
                            base::Unretained(this)));
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

  // TODO[Starboard]: verify that this st header is present in dial search
  // request. The header name used to be "ST".
  std::string st_request = info.GetHeaderValue("st");
  ignore_result(TrimWhitespaceASCII(st_request, base::TRIM_ALL, &st_request));

  if (st_request != kDialStRequest) {
    DVLOG(1) << "Received incorrect ST headers: " << st_request;
    return false;
  }

  DVLOG(1) << "Dial User-Agent: " << info.GetHeaderValue("user-agent");

  return true;
}

bool DialUdpServer::IsValidMSearchRequest(const HttpServerRequestInfo& info) {
  if (info.method != "M-SEARCH") {
    DVLOG(1) << "Invalid M-Search: SSDP method incorrect. Received method: "
             << info.method;
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
std::string DialUdpServer::ConstructSearchResponse() const {
  DCHECK(!location_url_.empty());

  std::string ret("HTTP/1.1 200 OK\r\n");
  ret.append(base::StringPrintf("LOCATION: %s\r\n", location_url_.c_str()));
  ret.append("CACHE-CONTROL: max-age=1800\r\n");
  ret.append("EXT:\r\n");
  ret.append("BOOTID.UPNP.ORG: 1\r\n");
  // CONFIGID represents the state of the device description. Can be any
  // non-negative integer from 0 to 2^24 - 1.
  // DIAL clients like the Cast extension will cache the response based on this,
  // so we ensure each location change has a different config id.
  // This way when the app restarts with a new IP or port, the client updates
  // its cache of DIAL devices accordingly.
  uint32 location_hash = base::Hash(location_url_) >> 8;
  ret.append(base::StringPrintf("CONFIGID.UPNP.ORG: %u\r\n", location_hash));
  ret.append(base::StringPrintf("SERVER: %s\r\n", server_agent_.c_str()));
  ret.append(base::StringPrintf("ST: %s\r\n", kDialStRequest));
  ret.append(base::StringPrintf("USN: uuid:%s::%s\r\n",
                                DialSystemConfig::GetInstance()->model_uuid(),
                                kDialStRequest));
  ret.append("\r\n");
  return std::move(ret);
}

}  // namespace net
