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
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread.h"
#include "net/dial/dial_http_server.h"
#include "net/dial/dial_service_handler.h"
#include "net/dial/dial_udp_server.h"

namespace net {

// DialService is part of an implementation of in-app DIAL.
// It starts up a UDP server to be used to respond to SSDP discovery requests,
// and an HTTP server to implement the DIAL commands.
// Clients register their DialServiceHandlers in order to get called back
// by the server when requests come in.
// Each registered DialServiceHandler should have a unique service path.
class NET_EXPORT DialService : public base::SupportsWeakPtr<DialService> {
 public:
  DialService();
  ~DialService();

  void Register(const scoped_refptr<DialServiceHandler>& handler);
  void Deregister(const scoped_refptr<DialServiceHandler>& handler);

  scoped_refptr<DialServiceHandler> GetHandler(const std::string& service_name,
                                               std::string* remaining_handler);

  const std::string& http_host_address() const;

  // Expose the DialHttpServer for unit tests.
  net::DialHttpServer* http_server() const { return http_server_.get(); }

 private:
  FRIEND_TEST_ALL_PREFIXES(DialServiceTest, GetHandler);
  FRIEND_TEST_ALL_PREFIXES(DialServiceTest, ReleasedHandler);

  // Called in DialService destructor.
  void Terminate();

  scoped_refptr<net::DialHttpServer> http_server_;
  std::unique_ptr<net::DialUdpServer> udp_server_;
  typedef std::map<std::string, scoped_refptr<DialServiceHandler> >
      ServiceHandlerMap;
  ServiceHandlerMap handlers_;
  std::string http_host_address_;
  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(DialService);
};

// Thread-safe, refcounted interface to DialService.
// Each function call will dispatch to the DialService's thread.
// Keeps a WeakPtr to the DialService so if it has been destroyed nothing
// happens.
class NET_EXPORT DialServiceProxy
    : public base::RefCountedThreadSafe<DialServiceProxy> {
 public:
  DialServiceProxy(const base::WeakPtr<DialService>& dial_service);
  void Register(const scoped_refptr<DialServiceHandler>& handler);
  void Deregister(const scoped_refptr<DialServiceHandler>& handler);
  std::string host_address() const { return host_address_; }

 private:
  friend class base::RefCountedThreadSafe<DialServiceProxy>;
  virtual ~DialServiceProxy();
  void OnRegister(const scoped_refptr<DialServiceHandler>& handler);
  void OnDeregister(const scoped_refptr<DialServiceHandler>& handler);

  base::WeakPtr<DialService> dial_service_;
  std::string host_address_;

  // Message loop to call DialService methods on.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(DialServiceProxy);
};

}  // namespace net

#endif  // SRC_DIAL_SERVICE_H_
