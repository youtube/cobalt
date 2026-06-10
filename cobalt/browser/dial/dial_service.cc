// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/browser/dial/dial_service.h"

#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/strings/stringprintf.h"
#include "base/task/current_thread.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "cobalt/browser/dial/dial_device_description.h"
#include "cobalt/browser/dial/dial_http_server.h"
#include "cobalt/browser/dial/dial_udp_server.h"
#include "net/base/ip_endpoint.h"
#include "net/server/http_server_response_info.h"

namespace in_app_dial {

namespace {

// This class multiplexes RequestHandler requests between two different
// sequences: the blocking one where DialHttpServer runs and a non-blocking one
// where the actual RequestHandler implementation (e.g. DialServerImpl) runs.
//
// It must be created on the blocking sequence where DialHttpServer runs.
class RequestHandlerProxy final : public DialHttpServer::RequestHandler {
 public:
  RequestHandlerProxy()
      : blocking_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
    CHECK(base::CurrentIOThread::IsSet());
    DCHECK_CALLED_ON_VALID_SEQUENCE(blocking_sequence_checker_);
    DETACH_FROM_SEQUENCE(request_handler_sequence_checker_);
  }

  ~RequestHandlerProxy() = default;

  base::WeakPtr<RequestHandlerProxy> AsWeakPtrInBlockingTaskRunner() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(blocking_sequence_checker_);
    return blocking_weak_ptr_factory_.GetWeakPtr();
  }

  void RegisterHandler(base::WeakPtr<DialHttpServer::RequestHandler> handler,
                       scoped_refptr<base::SequencedTaskRunner> task_runner) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(blocking_sequence_checker_);
    request_handler_ = std::move(handler);
    request_handler_task_runner_ = std::move(task_runner);
    request_handler_weak_ptr_factory_.InvalidateWeakPtrs();
  }

  void ResetHandler() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(blocking_sequence_checker_);
    request_handler_.reset();
    request_handler_task_runner_.reset();
    request_handler_weak_ptr_factory_.InvalidateWeakPtrs();
  }

  void OnRequestHandledInHandlerTaskRunner(
      base::OnceCallback<void(std::optional<net::HttpServerResponseInfo>)>
          http_server_callback,
      std::optional<net::HttpServerResponseInfo> info) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(request_handler_sequence_checker_);
    DCHECK_EQ(base::SequencedTaskRunner::GetCurrentDefault(),
              request_handler_task_runner_);
    blocking_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(http_server_callback), std::move(info)));
  }

  // DialHttpServer::RequestHandler overrides.
  void HandleRequest(
      const std::string& method,
      const std::string& service_name,
      const std::string& path,
      const std::string& data,
      const std::string& host_with_port,
      base::OnceCallback<void(std::optional<net::HttpServerResponseInfo>)>
          callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(blocking_sequence_checker_);
    if (!request_handler_task_runner_) {
      // No handler has been registered. We do not check `request_handler_`
      // directly because that would bind it to the wrong sequence.
      std::move(callback).Run(std::nullopt);
      return;
    }
    request_handler_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &DialHttpServer::RequestHandler::HandleRequest, request_handler_,
            method, service_name, path, data, host_with_port,
            base::BindOnce(
                &RequestHandlerProxy::OnRequestHandledInHandlerTaskRunner,
                request_handler_weak_ptr_factory_.GetWeakPtr(),
                std::move(callback))));
  }

 private:
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  base::WeakPtr<DialHttpServer::RequestHandler> request_handler_;
  scoped_refptr<base::SequencedTaskRunner> request_handler_task_runner_;

  SEQUENCE_CHECKER(blocking_sequence_checker_);
  SEQUENCE_CHECKER(request_handler_sequence_checker_);

  base::WeakPtrFactory<RequestHandlerProxy> request_handler_weak_ptr_factory_{
      this};
  base::WeakPtrFactory<RequestHandlerProxy> blocking_weak_ptr_factory_{this};
};

}  // namespace

class DialService::BlockingHelper final {
 public:
  BlockingHelper() = default;
  ~BlockingHelper() { DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_); }

  void Start() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    dial_http_server_ = std::make_unique<DialHttpServer>(
        device_description_, handler_proxy_.AsWeakPtrInBlockingTaskRunner());

    const net::IPEndPoint http_server_address =
        dial_http_server_->GetLocalAddress();
    if (http_server_address.ToString().empty()) {
      LOG(WARNING) << "Unable to get an IP address from the HTTP server. The "
                      "UDP server will not be started.";
      return;
    }
    dial_udp_server_ = std::make_unique<DialUdpServer>(device_description_,
                                                       http_server_address);
  }

  void Stop() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    dial_udp_server_.reset();
    dial_http_server_.reset();
  }

  void Restart() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    Stop();
    Start();
  }

  void RegisterHandler(base::WeakPtr<DialHttpServer::RequestHandler> handler,
                       scoped_refptr<base::SequencedTaskRunner> task_runner) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    handler_proxy_.RegisterHandler(std::move(handler), std::move(task_runner));
  }

  void ResetHandler() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    handler_proxy_.ResetHandler();
  }

 private:
  RequestHandlerProxy handler_proxy_;

  const DialDeviceDescription device_description_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<DialHttpServer> dial_http_server_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<DialUdpServer> dial_udp_server_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

DialService::DialService() : io_thread_("dial_io_thread") {
  CHECK(io_thread_.StartWithOptions(
      base::Thread::Options(base::MessagePumpType::IO, /*size=*/0)));
  blocking_helper_ =
      base::SequenceBound<BlockingHelper>(io_thread_.task_runner());
}

DialService::~DialService() = default;

// static
DialService* DialService::GetInstance() {
  static base::NoDestructor<DialService> instance;
  return instance.get();
}

void DialService::Start() {
  blocking_helper_.AsyncCall(&BlockingHelper::Start);
}

void DialService::Stop() {
  blocking_helper_.AsyncCall(&BlockingHelper::Stop);
}

void DialService::RegisterHandler(
    base::WeakPtr<DialHttpServer::RequestHandler> handler,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  blocking_helper_.AsyncCall(&BlockingHelper::RegisterHandler)
      .WithArgs(std::move(handler), std::move(task_runner));
}

void DialService::ResetHandler() {
  blocking_helper_.AsyncCall(&BlockingHelper::ResetHandler);
}

}  // namespace in_app_dial
