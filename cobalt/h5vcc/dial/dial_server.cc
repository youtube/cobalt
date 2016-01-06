/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/h5vcc/dial/dial_server.h"

#if defined(DIAL_SERVER)

#include <utility>

#include "base/string_util.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/network/network_module.h"
#include "net/server/http_server_request_info.h"

namespace cobalt {
namespace h5vcc {
namespace dial {

namespace {
DialServer::Method StringToMethod(const std::string& method) {
  if (LowerCaseEqualsASCII(method, "get")) {
    return DialServer::kGet;
  } else if (LowerCaseEqualsASCII(method, "post")) {
    return DialServer::kPost;
  } else if (LowerCaseEqualsASCII(method, "delete")) {
    return DialServer::kDelete;
  }
  NOTREACHED();
  return DialServer::kGet;
}

}  // namespace

DialServer::DialServer(script::EnvironmentSettings* settings,
                       const std::string& service_name)
    : service_name_(service_name) {
  message_loop_proxy_ = base::MessageLoopProxy::current();
  dom::DOMSettings* dom_settings =
      base::polymorphic_downcast<dom::DOMSettings*>(settings);
  DCHECK(dom_settings);

  if (dom_settings->network_module() && dom_settings->network_module()) {
    dial_service_ = dom_settings->network_module()->dial_service();
  }
  if (dial_service_) {
    dial_service_->Register(this);
  }
}

DialServer::~DialServer() {
  if (dial_service_) {
    dial_service_->Deregister(this);
  }
}

bool DialServer::OnDelete(
    const std::string& path,
    const DialHttpRequestCallbackWrapper::ScriptObject& handler) {
  return AddHandler(kDelete, path, handler);
}

bool DialServer::OnGet(
    const std::string& path,
    const DialHttpRequestCallbackWrapper::ScriptObject& handler) {
  return AddHandler(kGet, path, handler);
}

bool DialServer::OnPost(
    const std::string& path,
    const DialHttpRequestCallbackWrapper::ScriptObject& handler) {
  return AddHandler(kPost, path, handler);
}

// We dispatch the request to a JS handler, and call the completion CB
// when finished.
void DialServer::HandleRequest(const std::string& path,
                               const net::HttpServerRequestInfo& request,
                               const CompletionCB& completion_cb) {
  // This gets called on the DialService thread.
  // Post it to the browser thread.
  DCHECK_NE(base::MessageLoopProxy::current(), message_loop_proxy_);
  Method method = StringToMethod(request.method);
  message_loop_proxy_->PostTask(
      FROM_HERE, base::Bind(&DialServer::OnHandleRequest, this, method, path,
                            request, completion_cb));
}

bool DialServer::AddHandler(
    Method method, const std::string& path,
    const DialHttpRequestCallbackWrapper::ScriptObject& handler) {
  DCHECK_EQ(base::MessageLoopProxy::current(), message_loop_proxy_);
  scoped_refptr<DialHttpRequestCallbackWrapper> callback =
      new DialHttpRequestCallbackWrapper(this, handler);

  std::pair<CallbackMap::iterator, bool> ret =
      callback_map_[method].insert(std::make_pair(path, callback));
  return ret.second;
}

void DialServer::OnHandleRequest(Method method, const std::string& path,
                                 const net::HttpServerRequestInfo& request,
                                 const CompletionCB& completion_cb) {
  DCHECK_EQ(base::MessageLoopProxy::current(), message_loop_proxy_);
  DCHECK(dial_service_);

  CallbackMap::const_iterator it = callback_map_[method].find(path);
  if (it == callback_map_[method].end()) {
    // No handler for this method. Signal failure.
    completion_cb.Run(scoped_ptr<net::HttpServerResponseInfo>(), false);
  }

  // Transform the net request info into a JS version and run the script
  // callback. Run the completion callback when finished.
  scoped_refptr<DialHttpRequestCallbackWrapper> callback = it->second;
  scoped_refptr<DialHttpRequest> dial_request = new DialHttpRequest(
      path, request.method, request.data, dial_service_->http_host_address());
  scoped_refptr<DialHttpResponse> dial_response =
      new DialHttpResponse(path, request.method);
  script::CallbackResult<bool> ret =
      callback->callback.value().Run(dial_request, dial_response);
  scoped_ptr<net::HttpServerResponseInfo> response =
      dial_response->ToHttpServerResponseInfo();
  completion_cb.Run(response.Pass(), ret.result);
}

}  // namespace dial
}  // namespace h5vcc
}  // namespace cobalt
#endif  // defined(DIAL_SERVER)
