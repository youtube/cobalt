// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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
#ifndef COBALT_H5VCC_DIAL_DIAL_SERVER_H_
#define COBALT_H5VCC_DIAL_DIAL_SERVER_H_

#if defined(DIAL_SERVER)

#include <map>
#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread_checker.h"
#include "cobalt/h5vcc/dial/dial_http_request.h"
#include "cobalt/h5vcc/dial/dial_http_response.h"
#include "cobalt/script/callback_function.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/wrappable.h"
#include "net/dial/dial_service.h"

namespace cobalt {
namespace h5vcc {
namespace dial {

// Template boilerplate for wrapping a script callback safely.
template <typename T>
class ScriptCallbackWrapper
    : public base::RefCountedThreadSafe<ScriptCallbackWrapper<T> > {
 public:
  typedef script::ScriptValue<T> ScriptValue;
  ScriptCallbackWrapper(script::Wrappable* wrappable, const ScriptValue& func)
      : callback(wrappable, func) {}
  typename ScriptValue::Reference callback;
};

class DialServer : public script::Wrappable,
                   public base::SupportsWeakPtr<DialServer> {
 public:
  enum Method {
    kDelete,
    kGet,
    kPost,
    kMethodCount,
  };

  typedef script::CallbackFunction<bool(const scoped_refptr<DialHttpRequest>&,
                                        const scoped_refptr<DialHttpResponse>&)>
      DialHttpRequestCallback;
  typedef ScriptCallbackWrapper<DialHttpRequestCallback>
      DialHttpRequestCallbackWrapper;

  DialServer(script::EnvironmentSettings* environment_settings,
             const std::string& service_name);

  // Register a Javascript callback for various DIAL methods.
  bool OnDelete(const std::string& path,
                const DialHttpRequestCallbackWrapper::ScriptValue& handler);
  bool OnGet(const std::string& path,
             const DialHttpRequestCallbackWrapper::ScriptValue& handler);
  bool OnPost(const std::string& path,
              const DialHttpRequestCallbackWrapper::ScriptValue& handler);
  const std::string& service_name() const;

  std::unique_ptr<net::HttpServerResponseInfo> RunCallback(
      const std::string& path, const net::HttpServerRequestInfo& request);

  DEFINE_WRAPPABLE_TYPE(DialServer);

 private:
  class ServiceHandler;

  ~DialServer();
  bool AddHandler(Method method, const std::string& path,
                  const DialHttpRequestCallbackWrapper::ScriptValue& handler);

  typedef std::map<std::string, scoped_refptr<DialHttpRequestCallbackWrapper> >
      CallbackMap;
  THREAD_CHECKER(thread_checker_);
  CallbackMap callback_map_[kMethodCount];

  scoped_refptr<net::DialServiceProxy> dial_service_proxy_;
  scoped_refptr<ServiceHandler> service_handler_;

  DISALLOW_COPY_AND_ASSIGN(DialServer);
};

}  // namespace dial
}  // namespace h5vcc
}  // namespace cobalt
#endif  // defined(DIAL_SERVER)

#endif  // COBALT_H5VCC_DIAL_DIAL_SERVER_H_
