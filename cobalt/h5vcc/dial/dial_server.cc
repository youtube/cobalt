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

#include "cobalt/h5vcc/dial/dial_server.h"

#include "base/threading/thread_task_runner_handle.h"

#if defined(DIAL_SERVER)

#include <memory>
#include <utility>

#include "base/strings/string_util.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/network/dial/dial_service_handler.h"
#include "cobalt/network/network_module.h"
#include "cobalt/web/context.h"
#include "cobalt/web/environment_settings.h"
#include "net/server/http_server_request_info.h"

namespace cobalt {
namespace h5vcc {
namespace dial {

namespace {
DialServer::Method StringToMethod(const std::string& method) {
  if (base::LowerCaseEqualsASCII(method, "get")) {
    return DialServer::kGet;
  } else if (base::LowerCaseEqualsASCII(method, "post")) {
    return DialServer::kPost;
  } else if (base::LowerCaseEqualsASCII(method, "delete")) {
    return DialServer::kDelete;
  }
  NOTREACHED();
  return DialServer::kGet;
}

}  // namespace

class DialServer::ServiceHandler : public cobalt::network::DialServiceHandler {
 public:
  ServiceHandler(const base::WeakPtr<DialServer>& dial_server,
                 const std::string& service_name);

  // net::DialServiceHandler implementation.
  const std::string& service_name() const override { return service_name_; }
  void HandleRequest(const std::string& path,
                     const net::HttpServerRequestInfo& request,
                     const CompletionCB& completion_cb) override;

 private:
  ~ServiceHandler();
  void OnHandleRequest(const std::string& path,
                       const net::HttpServerRequestInfo& request,
                       const CompletionCB& completion_cb);

  // Keep a weak ptr to the dial server, so that it can be safely destroyed
  // while we are deregistering our handler from the DialService.
  base::WeakPtr<DialServer> dial_server_;
  std::string service_name_;
  // The message loop we should dispatch HandleRequest to. This is the
  // message loop we were constructed on.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

DialServer::DialServer(script::EnvironmentSettings* settings,
                       const std::string& service_name) {
  service_handler_ = new ServiceHandler(AsWeakPtr(), service_name);
  auto* web_settings =
      base::polymorphic_downcast<web::EnvironmentSettings*>(settings);
  DCHECK(web_settings);

  if (web_settings && web_settings->context()->network_module()) {
    dial_service_proxy_ =
        web_settings->context()->network_module()->dial_service_proxy();
  }
  if (dial_service_proxy_) {
    DLOG(INFO) << "DialServer running at: "
               << dial_service_proxy_->host_address();
    dial_service_proxy_->Register(service_handler_);
  }
}

DialServer::~DialServer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (dial_service_proxy_) {
    dial_service_proxy_->Deregister(service_handler_);
  }
}

bool DialServer::OnDelete(
    const std::string& path,
    const DialHttpRequestCallbackWrapper::ScriptValue& handler) {
  return AddHandler(kDelete, path, handler);
}

bool DialServer::OnGet(
    const std::string& path,
    const DialHttpRequestCallbackWrapper::ScriptValue& handler) {
  return AddHandler(kGet, path, handler);
}

bool DialServer::OnPost(
    const std::string& path,
    const DialHttpRequestCallbackWrapper::ScriptValue& handler) {
  return AddHandler(kPost, path, handler);
}

const std::string& DialServer::service_name() const {
  return service_handler_->service_name();
}

bool DialServer::AddHandler(
    Method method, const std::string& path,
    const DialHttpRequestCallbackWrapper::ScriptValue& handler) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  scoped_refptr<DialHttpRequestCallbackWrapper> callback =
      new DialHttpRequestCallbackWrapper(this, handler);

  std::pair<CallbackMap::iterator, bool> ret =
      callback_map_[method].insert(std::make_pair(path, callback));
  return ret.second;
}

// Transform the net request info into a JS version and run the script
// callback, if there is one registered. Return the response and status.
std::unique_ptr<net::HttpServerResponseInfo> DialServer::RunCallback(
    const std::string& path, const net::HttpServerRequestInfo& request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  std::unique_ptr<net::HttpServerResponseInfo> response;
  Method method = StringToMethod(request.method);
  CallbackMap::const_iterator it = callback_map_[method].find(path);
  if (it == callback_map_[method].end()) {
    // No handler for this method. Signal failure.
    return response;
  }

  scoped_refptr<DialHttpRequestCallbackWrapper> callback = it->second;
  scoped_refptr<DialHttpRequest> dial_request = new DialHttpRequest(
      path, request.method, request.data, dial_service_proxy_->host_address());
  scoped_refptr<DialHttpResponse> dial_response =
      new DialHttpResponse(path, request.method);
  script::CallbackResult<bool> ret =
      callback->callback.value().Run(dial_request, dial_response);
  if (ret.result) {
    response = dial_response->ToHttpServerResponseInfo();
  }
  return response;
}

DialServer::ServiceHandler::ServiceHandler(
    const base::WeakPtr<DialServer>& dial_server,
    const std::string& service_name)
    : dial_server_(dial_server), service_name_(service_name) {
  task_runner_ = base::ThreadTaskRunnerHandle::Get();
}

DialServer::ServiceHandler::~ServiceHandler() {}

void DialServer::ServiceHandler::HandleRequest(
    const std::string& path, const net::HttpServerRequestInfo& request,
    const CompletionCB& completion_cb) {
  // This gets called on the DialService/Network thread.
  // Post it to the WebModule thread.
  DCHECK_NE(base::ThreadTaskRunnerHandle::Get(), task_runner_);
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&ServiceHandler::OnHandleRequest, this, path,
                            request, completion_cb));
}

void DialServer::ServiceHandler::OnHandleRequest(
    const std::string& path, const net::HttpServerRequestInfo& request,
    const CompletionCB& completion_cb) {
  DCHECK_EQ(base::ThreadTaskRunnerHandle::Get(), task_runner_);
  if (!dial_server_) {
    completion_cb.Run(std::unique_ptr<net::HttpServerResponseInfo>());
    return;
  }

  std::unique_ptr<net::HttpServerResponseInfo> response =
      dial_server_->RunCallback(path, request);
  completion_cb.Run(std::move(response));
}

}  // namespace dial
}  // namespace h5vcc
}  // namespace cobalt
#endif  // defined(DIAL_SERVER)
