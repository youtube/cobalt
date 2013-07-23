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
  // DialService is lazy, so we can start in the constructor
  // and always have a messageloop.
  thread_->StartWithOptions(base::Thread::Options(MessageLoop::TYPE_IO, 0));
}

DialService::~DialService() {
  DLOG_IF(FATAL, is_running()) << "Dial server is still running when "
                                  "destroying the service.";
}

MessageLoop* DialService::GetMessageLoop() {
  DCHECK(thread_->IsRunning());
  return thread_->message_loop();
}

std::string DialService::GetHttpHostAddress() const {
  IPEndPoint addr;
  ignore_result(GetLocalAddress(&addr));
  return addr.ToString();
}

void DialService::StartService() {
  if (is_running_) {
    return; // Already running
  }

  GetMessageLoop()->PostTask(FROM_HERE,
      base::Bind(&DialService::SpinUpServices, base::Unretained(this)));
}

void DialService::StopService() {
  GetMessageLoop()->PostTask(FROM_HERE,
      base::Bind(&DialService::SpinDownServices, base::Unretained(this)));
}

// Syncrhonized call to stop the service.
void DialService::Terminate() {
  // Should be called from a different thread.
  DCHECK(!CurrentThreadIsValid());

  if (thread_) {
    thread_->Stop();
    thread_.reset();
  }

  DLOG_IF(WARNING, http_server_ || udp_server_)
      << "Force Terminating Dial Server.";

  if (http_server_.get()) {
    http_server_->Stop();
    http_server_ = NULL;
  }
  if (udp_server_.get()) {
    udp_server_->Stop();
    udp_server_.reset();
  }
}

void DialService::SpinUpServices() {
  DCHECK(CurrentThreadIsValid());

  // Do nothing.
  if (is_running_) return;

  DCHECK(!http_server_.get());
  DCHECK(!udp_server_.get());

  http_server_ = new DialHttpServer();
  udp_server_.reset(new DialUdpServer());

  // Create servers
  if (!http_server_->Start())
    return;

  if (!udp_server_->Start(http_server_->location_url(), kUdpServerAgent)) {
    // switch off the http_server too.
    http_server_->Stop();
    return;
  }

  DLOG(INFO) << "Dial Server is now running.";
  is_running_ = true;
}

void DialService::SpinDownServices() {
  DCHECK(CurrentThreadIsValid());

  // Do nothing
  if (!is_running_) return;

  DLOG(INFO) << "Dial Server is shutting down.";
  bool http_ret = http_server_->Stop();
  bool udp_ret = udp_server_->Stop();

  // running is false when both Stops() return true;
  is_running_ = !(http_ret && udp_ret);

  http_server_ = NULL;
  udp_server_.reset();

  // Check that both are in same state.
  DCHECK(http_ret == udp_ret);
}

bool DialService::Register(DialServiceHandler* handler) {
  if (!handler)
    return false;

  const std::string& path = handler->service_name();
  if (path.empty())
    return false;

  GetMessageLoop()->PostTask(FROM_HERE,
      base::Bind(&DialService::AddToHandlerMap, base::Unretained(this),
                 base::Unretained(handler)));
  return true;
}

void DialService::AddToHandlerMap(DialServiceHandler* handler) {
  const std::string& path = handler->service_name();

  DCHECK(CurrentThreadIsValid());
  DCHECK(!path.empty());
  DCHECK(handler);

  // Don't replace.
  std::pair<ServiceHandlerMap::iterator, bool> it =
      handlers_.insert(std::make_pair(path, handler));
  VLOG(1) << "Attempt to insert handler for path: " << path
             << (it.second ? " succeeded" : " failed");

  if (it.second && !is_running()) {
    SpinUpServices();
  }
}

bool DialService::Deregister(DialServiceHandler* handler) {
  if (handler == NULL || handler->service_name().empty()) {
    return false;
  }

  GetMessageLoop()->PostTask(FROM_HERE,
      base::Bind(&DialService::RemoveFromHandlerMap, base::Unretained(this),
                 base::Unretained(handler)));
  return true;
}

void DialService::RemoveFromHandlerMap(DialServiceHandler* handler) {
  // At this point, |handler| might already been deleted, so just remove the
  // reference to it in |handlers_|.
  DCHECK(CurrentThreadIsValid());
  DCHECK(handler);

  for (ServiceHandlerMap::iterator it = handlers_.begin();
      it != handlers_.end(); ++it) {
    if (it->second == handler) {
      handlers_.erase(it);
      break;
    }
  }

  if (handlers_.empty() && is_running()) {
    SpinDownServices();
  }
}

DialServiceHandler* DialService::GetHandler(const std::string& request_path,
                                            std::string* handler_path) {

  DCHECK(CurrentThreadIsValid());
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

  // for the remaining portion, extract it out as the handler path.
  *handler_path = path.substr(pos).as_string();

  // If the |handler_path| is empty, that means the request is "/apps/Foo", the
  // semantic equivalent of "/apps/Foo/". If we keep the |handler_path| empty,
  // somehow the JS does not catch it. So for now forcing it to "/" instead.
  // TODO: Figure out the reason and eliminate this logic.
  if (handler_path->empty()) {
    *handler_path = std::string("/");
  }

  // at this point, it gotta start with '/'
  DCHECK_EQ('/', (*handler_path)[0]);

  // This should not be NULL at this point.
  DCHECK(it->second);
  return it->second;
}

} // namespace net

