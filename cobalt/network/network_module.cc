/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/network/network_module.h"

#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "cobalt/network/network_system.h"

namespace cobalt {
namespace network {

namespace {
// Do-nothing callback, intended to release these resources
// on the IO thread. Called during NetworkModule destruction.
void OnDestroy(scoped_ptr<URLRequestContext> url_request_context,
               scoped_ptr<NetworkDelegate> network_delegate) {}
}  // namespace

NetworkModule::NetworkModule() : io_thread_("IO Thread") {
  PlatformInit();

  // Launch the IO thread.
  base::Thread::Options io_thread_options;
  io_thread_options.message_loop_type = MessageLoop::TYPE_IO;
  io_thread_options.stack_size = 256 * 1024;
  io_thread_.StartWithOptions(io_thread_options);

  base::WaitableEvent creation_event(true, false);
  // Run Network module startup on IO thread,
  // so the network delegate and URL request context are
  // constructed on that thread.
  message_loop_proxy()->PostTask(
      FROM_HERE, base::Bind(&NetworkModule::OnCreate, base::Unretained(this),
                            &creation_event));
  // Wait for OnCreate() to run, so we can be sure our members
  // have been constructed.
  creation_event.Wait();
  DCHECK(url_request_context_);
  url_request_context_getter_ =
      new network::URLRequestContextGetter(
          url_request_context_.get(), &io_thread_);
}

NetworkModule::~NetworkModule() {
  message_loop_proxy()->PostTask(
      FROM_HERE, base::Bind(
          &OnDestroy,
          base::Passed(&url_request_context_),
          base::Passed(&network_delegate_)));
  PlatformShutdown();
}

void NetworkModule::OnCreate(base::WaitableEvent* creation_event) {
  DCHECK(message_loop_proxy()->BelongsToCurrentThread());

  url_request_context_.reset(new URLRequestContext());
  NetworkDelegate::Options net_options;
  // TODO(***REMOVED***): Specify any override to the cookie settings
  // in net_options.
  network_delegate_.reset(new NetworkDelegate(net_options));
  url_request_context_->set_network_delegate(network_delegate_.get());
  creation_event->Signal();
}

}  // namespace network
}  // namespace cobalt
