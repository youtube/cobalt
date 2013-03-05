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
#include "net/server/http_server_request_info.h"
#include "net/url_request/url_request.h"

namespace net {

static const std::string kUdpServerAgent = "Steel/2.0 UPnP/1.1";

DialService::DialService(LBWebViewHost* host)
    : host_(host)
    , thread_(new base::Thread("dial_service"))
    , http_server_(new DialHttpServer())
    , udp_server_(new DialUdpServer())
    , is_running_(false) {
}

void DialService::StartService() {
  if (is_running_) {
    return; // Already running
  }

  if (!thread_->IsRunning()) {
    thread_->StartWithOptions(base::Thread::Options(MessageLoop::TYPE_IO, 0));
  }
  thread_->message_loop()->PostTask(FROM_HERE,
      base::Bind(&DialService::SpinUpServices, this));
}

void DialService::StopService() {
  thread_->message_loop()->PostTask(FROM_HERE,
      base::Bind(&DialService::SpinDownServices, this));
}

void DialService::SpinUpServices() {
  DCHECK(CurrentThreadIsValid());

  // Do nothing.
  if (is_running_) return;

  // Create servers
  if (!http_server_->Start())
    return;

  if (!udp_server_->Start(http_server_->location_url(), kUdpServerAgent)) {
    // switch off the http_server too.
    http_server_->Stop();
    return;
  }

  is_running_ = true;
}

void DialService::SpinDownServices() {
  DCHECK(CurrentThreadIsValid());

  // Do nothing
  if (!is_running_) return;

  bool http_ret = http_server_->Stop();
  bool udp_ret = udp_server_->Stop();

  // running is false when both Stops() return true;
  is_running_ = !(http_ret && udp_ret);

  // Check that both are in same state.
  DCHECK(http_ret == udp_ret);
}

} // namespace net

