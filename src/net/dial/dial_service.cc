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
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "net/base/net_errors.h"
#include "net/server/http_server_request_info.h"
#include "net/url_request/url_request.h"

namespace net {

namespace {
const char* kUdpServerAgent = "Cobalt/2.0 UPnP/1.1";
}  // namespace

DialService::DialService() {
  http_server_ = new DialHttpServer(this);
  udp_server_.reset(
      new DialUdpServer(http_server_->location_url(), kUdpServerAgent));

  // Compute HTTP local address and cache it.
  IPEndPoint addr;
  if (http_server_->GetLocalAddress(&addr) == net::OK) {
    http_host_address_ = addr.ToString();
  } else {
    DLOG(WARNING) << "Could not start Dial Server";
  }
}

DialService::~DialService() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  Terminate();
}

const std::string& DialService::http_host_address() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return http_host_address_;
}

void DialService::Terminate() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Note that we may not have the last ref to http_server_, so we can't control
  // where it gets destroyed. Ensure we stop it on the right thread.
  if (http_server_) {
    http_server_->Stop();
  }
  http_server_.reset();
  udp_server_.reset();
}

void DialService::Register(const scoped_refptr<DialServiceHandler>& handler) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  TRACE_EVENT0("net::dial", __FUNCTION__);
  DCHECK(handler);
  const std::string& path = handler->service_name();
  if (!path.empty()) {
    handlers_[path] = handler;
  }
}

void DialService::Deregister(const scoped_refptr<DialServiceHandler>& handler) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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

scoped_refptr<DialServiceHandler> DialService::GetHandler(
    const std::string& request_path,
    std::string* handler_path) {
  // This function should only be called by DialHttpServer, to find a handler
  // to respond to an incoming request.
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

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

DialServiceProxy::DialServiceProxy(
    const base::WeakPtr<DialService>& dial_service)
    : dial_service_(dial_service) {
  host_address_ = dial_service_->http_host_address();
  // Remember the message loop we were constructed on. We'll post all our tasks
  // there, to ensure thread safety when accessing dial_service_.
  task_runner_ = base::MessageLoop::current()->task_runner();
}

DialServiceProxy::~DialServiceProxy() {}

void DialServiceProxy::Register(
    const scoped_refptr<DialServiceHandler>& handler) {
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&DialServiceProxy::OnRegister, this, handler));
}

void DialServiceProxy::Deregister(
    const scoped_refptr<DialServiceHandler>& handler) {
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&DialServiceProxy::OnDeregister, this, handler));
}

void DialServiceProxy::OnRegister(
    const scoped_refptr<DialServiceHandler>& handler) {
  if (dial_service_) {
    dial_service_->Register(handler);
  }
}

void DialServiceProxy::OnDeregister(
    const scoped_refptr<DialServiceHandler>& handler) {
  if (dial_service_) {
    dial_service_->Deregister(handler);
  }
}

}  // namespace net
