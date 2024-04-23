// Copyright 2023 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_NETWORK_DIAL_DIAL_SERVICE_HANDLER_H_
#define COBALT_NETWORK_DIAL_DIAL_SERVICE_HANDLER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/functional/callback_forward.h"
#include "net/http/http_response_headers.h"
#include "net/server/http_server_request_info.h"
#include "net/server/http_server_response_info.h"

namespace cobalt {
namespace network {

// Abstract class that provides a response to a request from the dial server.
// DialServiceHandlers should be Register()ed with a DialService object.
class DialServiceHandler
    : public base::RefCountedThreadSafe<DialServiceHandler> {
 public:
  typedef base::Callback<void(
      std::unique_ptr<net::HttpServerResponseInfo> response)>
      CompletionCB;
  // Called by the DialHttpServer to satisfy an incoming request.
  // It is expected that this will be handled asynchronously and the completion
  // callback must be called in all cases. If the request is handled
  // successfully, pass the response the server should send
  // back and a |result| of true. If the request cannot be handled, pass a NULL
  // |response| and a |result| of false.
  virtual void HandleRequest(const std::string& path,
                             const net::HttpServerRequestInfo& request,
                             const CompletionCB& completion_cb) = 0;
  // The name of the DIAL service this handler implements.
  virtual const std::string& service_name() const = 0;

 protected:
  friend class base::RefCountedThreadSafe<DialServiceHandler>;
  virtual ~DialServiceHandler() {}
};

}  // namespace network
}  // namespace cobalt

#endif  // COBALT_NETWORK_DIAL_DIAL_SERVICE_HANDLER_H_
