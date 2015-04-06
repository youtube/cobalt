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

#ifndef SRC_DIAL_SERVICE_H_
#define SRC_DIAL_SERVICE_H_

#include <map>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread.h"
#include "net/dial/dial_http_server.h"
#include "net/dial/dial_service_handler.h"
#include "net/dial/dial_udp_server.h"

namespace net {

class NET_EXPORT DialService {
 public:
  static DialService* GetInstance();
  ~DialService();
  // Called at program exit or in DialService destructor.
  void Terminate();

  bool Register(DialServiceHandler* handler);
  bool Deregister(DialServiceHandler* handler);

  DialServiceHandler* GetHandler(const std::string& service_name,
                                 std::string* remaining_handler);

  bool is_running() const {
    return is_running_;
  }

  const std::string& http_host_address() const;

  scoped_refptr<base::MessageLoopProxy> message_loop_proxy() const;

  bool IsOnServiceThread() const {
    return thread_ && thread_->message_loop_proxy()->BelongsToCurrentThread();
  }

  // Expose the DialHttpServer for unit tests.
  scoped_refptr<net::DialHttpServer> http_server() const {
    return http_server_;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(DialServiceTest, GetHandler);
  FRIEND_TEST_ALL_PREFIXES(DialServiceTest, DestructedHandler);

  friend struct base::DefaultLazyInstanceTraits<DialService>;
  DialService();

  // Called on the dial_service thread's message loop.
  void OnInitialize();
  void OnTerminate();
  void OnRegister(DialServiceHandler*);
  void OnDeregister(DialServiceHandler*);

  scoped_ptr<base::Thread> thread_;
  scoped_refptr<net::DialHttpServer> http_server_;
  scoped_ptr<net::DialUdpServer> udp_server_;
  typedef std::map<std::string, DialServiceHandler*> ServiceHandlerMap;
  ServiceHandlerMap handlers_;
  std::string http_host_address_;
  bool is_running_;
};

} // namespace net

#endif // SRC_DIAL_SERVICE_H_
