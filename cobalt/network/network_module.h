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

#ifndef NETWORK_NETWORK_MODULE_H_
#define NETWORK_NETWORK_MODULE_H_

#include "base/message_loop_proxy.h"
#include "base/object_watcher_shell.h"
#include "base/threading/thread.h"
#include "cobalt/network/network_delegate.h"
#include "cobalt/network/url_request_context.h"
#include "cobalt/network/url_request_context_getter.h"
#include "googleurl/src/gurl.h"

namespace base {
class WaitableEvent;
}  // namespace base

namespace cobalt {

namespace storage {
class StorageManager;
}  // namespace storage

namespace network {

// NetworkModule wraps various networking-related components such as
// a URL request context.
// This is owned by BrowserModule.
class NetworkModule {
 public:
  explicit NetworkModule(storage::StorageManager* storage_manager);
  ~NetworkModule();

  URLRequestContext* url_request_context() const {
    return url_request_context_.get();
  }
  NetworkDelegate* network_delegate() const {
    return network_delegate_.get();
  }
  scoped_refptr<URLRequestContextGetter> url_request_context_getter() const {
    return url_request_context_getter_;
  }
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy() const {
    return thread_->message_loop_proxy();
  }
  storage::StorageManager* storage_manager() const { return storage_manager_; }

 private:
  void OnCreate(base::WaitableEvent* creation_event);

  storage::StorageManager* storage_manager_;
  scoped_ptr<base::Thread> thread_;
  scoped_ptr<base::ObjectWatchMultiplexer> object_watch_multiplexer_;
  scoped_ptr<URLRequestContext> url_request_context_;
  scoped_refptr<URLRequestContextGetter> url_request_context_getter_;
  scoped_ptr<NetworkDelegate> network_delegate_;

  DISALLOW_COPY_AND_ASSIGN(NetworkModule);
};

}  // namespace network
}  // namespace cobalt

#endif  // NETWORK_NETWORK_MODULE_H_
