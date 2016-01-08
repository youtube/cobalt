/*
 * Copyright 2012 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "net/dial/dial_service.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/string_piece.h"
#include "base/stringprintf.h"
#include "net/server/http_server_request_info.h"
#include "net/url_request/url_request.h"

namespace net {

namespace {
const char* kUdpServerAgent = "Steel/2.0 UPnP/1.1";
}  // namespace

DialService::DialService() {
  message_loop_proxy_ = base::MessageLoopProxy::current();

  http_server_ = new DialHttpServer(this);
  udp_server_.reset(new DialUdpServer(http_server_->location_url(),
                                      kUdpServerAgent));

  // Compute HTTP local address and cache it.
  IPEndPoint addr;
  http_server_->GetLocalAddress(&addr);
  http_host_address_ = addr.ToString();

  DLOG(INFO) << "Dial Server is now running on " << http_host_address_;
}

DialService::~DialService() {
  Terminate();
}

const std::string& DialService::http_host_address() const {
  return http_host_address_;
}

void DialService::Terminate() {
  DCHECK_EQ(base::MessageLoopProxy::current(), message_loop_proxy_);
  http_server_ = NULL;
  udp_server_.reset();
}

void DialService::Register(DialServiceHandler* handler) {
  if (base::MessageLoopProxy::current() != message_loop_proxy_) {
    message_loop_proxy_->PostTask(
        FROM_HERE,
        base::Bind(&DialService::Register, base::Unretained(this), handler));
    return;
  }

  TRACE_EVENT0("net::dial", __FUNCTION__);
  DCHECK(handler);
  const std::string& path = handler->service_name();
  if (!path.empty()) {
    handlers_[path] = handler;
  }
}

void DialService::Deregister(DialServiceHandler* handler) {
  if (base::MessageLoopProxy::current() != message_loop_proxy_) {
    message_loop_proxy_->PostTask(
        FROM_HERE,
        base::Bind(&DialService::Deregister, base::Unretained(this), handler));
    return;
  }

  TRACE_EVENT0("net::dial", __FUNCTION__);
  DCHECK(handler);
  for (ServiceHandlerMap::iterator it = handlers_.begin();
      it != handlers_.end(); ++it) {
    if (it->second == handler) {
      handlers_.erase(it);
      break;
    }
  }
}

DialServiceHandler* DialService::GetHandler(const std::string& request_path,
                                            std::string* handler_path) {
  // This function should only be called by DialHttpServer, to find a handler
  // to respond to an incoming request.
  DCHECK_EQ(base::MessageLoopProxy::current(), message_loop_proxy_);
  DCHECK(handler_path != NULL);
  TRACE_EVENT0("net::dial", __FUNCTION__);

  VLOG(1) << "Requesting Handler for path: " << request_path;
  base::StringPiece path(request_path);

  // remove '/apps/'
  const base::StringPiece kUrlPrefix("/apps/");
  if (!path.starts_with(kUrlPrefix)) {
    return NULL;
  }
  path = path.substr(kUrlPrefix.size());

  // find the next '/', and extract the portion in between.
  size_t pos = path.find_first_of('/');
  std::string service_path = path.substr(0, pos).as_string();

  // sanity check further, then extract the data.
  DCHECK_EQ(std::string::npos, service_path.find('/'));
  ServiceHandlerMap::const_iterator it = handlers_.find(service_path);
  if (it == handlers_.end()) {
    return NULL;
  }
  DCHECK(it->second);

  // for the remaining portion, extract it out as the handler path.
  *handler_path = path.substr(pos).as_string();

  // If the |handler_path| is empty, that means the request is "/apps/Foo", the
  // semantic equivalent of "/apps/Foo/". If we keep the |handler_path| empty,
  // somehow the JS does not catch it. So for now forcing it to "/" instead.
  if (handler_path->empty()) {
    *handler_path = std::string("/");
  }

  DCHECK_EQ('/', handler_path->at(0));

  return it->second;
}

}  // namespace net
