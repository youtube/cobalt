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

#include "dial_service.h"

#include "base/bind.h"
#include "base/stringprintf.h"
#include "base/string_piece.h"
#include "net/server/http_server_request_info.h"
#include "net/url_request/url_request.h"

namespace net {

static const std::string kUdpServerAgent = "Steel/2.0 UPnP/1.1";

namespace {
static base::LazyInstance<DialService> g_instance =
    LAZY_INSTANCE_INITIALIZER;
}

// static
DialService* DialService::GetInstance() {
  return g_instance.Pointer();
}

DialService::DialService()
    : thread_(new base::Thread("dial_service"))
    , http_server_(NULL)
    , udp_server_(NULL)
    , is_running_(false) {
  thread_->StartWithOptions(base::Thread::Options(MessageLoop::TYPE_IO, 0));
  message_loop_proxy()->PostTask(FROM_HERE,
      base::Bind(&DialService::OnInitialize, base::Unretained(this)));
}

DialService::~DialService() {
  Terminate();
}

scoped_refptr<base::MessageLoopProxy> DialService::message_loop_proxy() const {
  DCHECK(thread_->IsRunning());
  return thread_->message_loop_proxy();
}

const std::string& DialService::http_host_address() const {
    DCHECK_EQ(is_running(), true);
    return http_host_address_;
}

void DialService::Terminate() {
  DCHECK(!IsOnServiceThread());
  if (!is_running_) {
    return;
  }
  is_running_ = false;
  message_loop_proxy()->PostTask(FROM_HERE,
      base::Bind(&DialService::OnTerminate, base::Unretained(this)));

  // Stop() will wait for all pending tasks.
  thread_->Stop();
  thread_.reset();
}

bool DialService::Register(DialServiceHandler* handler) {
  DCHECK(!IsOnServiceThread());

  message_loop_proxy()->PostTask(FROM_HERE,
      base::Bind(&DialService::OnRegister, base::Unretained(this),
                 base::Unretained(handler)));
  return true;
}

bool DialService::Deregister(DialServiceHandler* handler) {
  DCHECK(!IsOnServiceThread());

  message_loop_proxy()->PostTask(FROM_HERE,
      base::Bind(&DialService::OnDeregister, base::Unretained(this),
                 base::Unretained(handler)));
  return true;
}

void DialService::OnInitialize() {
  DCHECK(IsOnServiceThread());
  DCHECK_EQ(is_running_, false);
  DCHECK(!http_server_.get());
  DCHECK(!udp_server_.get());

  // Create servers
  http_server_ = new DialHttpServer();
  if (!http_server_->Start()) {
    http_server_ = NULL;
    DLOG(ERROR) << "HTTP server failed to start.";
    return;
  }

  udp_server_.reset(new DialUdpServer());
  if (!udp_server_->Start(http_server_->location_url(), kUdpServerAgent)) {
    DLOG(ERROR) << "UDP server failed to start.";
    // switch off the http_server too.
    http_server_->Stop();
    http_server_ = NULL;
    udp_server_.reset();
    return;
  }

  // Compute HTTP local address and cache it.
  IPEndPoint addr;
  http_server_->GetLocalAddress(&addr);
  http_host_address_ = addr.ToString();

  DLOG(INFO) << "Dial Server is now running on " << http_host_address_;
  is_running_ = true;
}

void DialService::OnTerminate() {
  DCHECK(IsOnServiceThread());
  if (http_server_) {
    http_server_->Stop();
    http_server_ = NULL;
  }
  if (udp_server_) {
    udp_server_->Stop();
    udp_server_.reset();
  }
}

void DialService::OnRegister(DialServiceHandler* handler) {
  DCHECK(IsOnServiceThread());
  DCHECK(handler);
  const std::string& path = handler->service_name();
  if (!path.empty()) {
    handlers_[path] = handler;
  }
}

void DialService::OnDeregister(DialServiceHandler* handler) {
  DCHECK(IsOnServiceThread());
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

  DCHECK(IsOnServiceThread());
  DCHECK(handler_path != NULL);

  VLOG(1) << "Requesting Handler for path: " << request_path;
  base::StringPiece path(request_path);

  // remove '/apps/'
  const base::StringPiece kUrlPrefix("/apps/");
  if (!path.starts_with(kUrlPrefix)) return NULL;
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
  // TODO: Figure out the reason and eliminate this logic.
  if (handler_path->empty()) {
    *handler_path = std::string("/");
  }

  DCHECK_EQ('/', handler_path->at(0));

  return it->second;
}

} // namespace net

