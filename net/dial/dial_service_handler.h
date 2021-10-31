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

#ifndef SRC_DIAL_SERVICE_HANDLER_H_
#define SRC_DIAL_SERVICE_HANDLER_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "net/http/http_response_headers.h"

namespace net {

class HttpServerRequestInfo;

// DialUdpServer used to define HttpServerResponseInfo itself. The new net
// have its own HttpServerResponseInfo now and we can use that instead. It also
// makes connecting with other net components easier.
class HttpServerResponseInfo;

// Abstract class that provides a response to a request from the dial server.
// DialServiceHandlers should be Register()ed with a DialService object.
class DialServiceHandler
    : public base::RefCountedThreadSafe<DialServiceHandler> {
 public:
  typedef base::Callback<void(std::unique_ptr<HttpServerResponseInfo> response)>
      CompletionCB;
  // Called by the DialHttpServer to satisfy an incoming request.
  // It is expected that this will be handled asynchronously and the completion
  // callback must be called in all cases. If the request is handled
  // successfully, pass the response the server should send
  // back and a |result| of true. If the request cannot be handled, pass a NULL
  // |response| and a |result| of false.
  virtual void HandleRequest(const std::string& path,
                             const HttpServerRequestInfo& request,
                             const CompletionCB& completion_cb) = 0;
  // The name of the DIAL service this handler implements.
  virtual const std::string& service_name() const = 0;

 protected:
  friend class base::RefCountedThreadSafe<DialServiceHandler>;
  virtual ~DialServiceHandler() {}
};

}  // namespace net

#endif  // SRC_DIAL_SERVICE_HANDLER_H_
