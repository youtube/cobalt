// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/linux/shared/echo_service.h"

#include <pthread.h>
#include <unistd.h>

#include <memory>
#include <string>

#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/common/time.h"
#include "starboard/configuration.h"
#include "starboard/linux/shared/platform_service.h"
#include "starboard/shared/starboard/application.h"

namespace starboard {
namespace {

// To simulate some expensive work that is done asynchronously.
constexpr long kAsyncResponseDelaySec = 5;

struct ThreadArgs {
  PlatformServiceImpl* platform_service_instance;
  std::string response;
};

// This platform service is for testing only and simply echoes data sent to it
// through Send(). It responds synchronously by returning the data from Send()
// and asynchronously using its receive_callback.
typedef struct EchoServiceImpl : public PlatformServiceImpl {
  pthread_mutex_t m;
  pthread_cond_t cv;
  bool should_shutdown;
  std::vector<pthread_t> workers;

  EchoServiceImpl(void* context, ReceiveMessageCallback receive_callback)
      : PlatformServiceImpl(context, receive_callback), should_shutdown(false) {
    pthread_mutex_init(&m, NULL);
    pthread_cond_init(&cv, NULL);
  }

  // Default constructor.
  EchoServiceImpl() = default;

  ~EchoServiceImpl() { CleanUpWorkers(); }

  static void* AsyncResponseTrampoline(void* args) {
    std::unique_ptr<ThreadArgs> thread_args(static_cast<ThreadArgs*>(args));

    EchoServiceImpl* echo_service_instance =
        static_cast<EchoServiceImpl*>(thread_args->platform_service_instance);
    echo_service_instance->RespondAsynchronously(thread_args->response);

    return nullptr;
  }

  void StartAsyncResponseThread(const std::string& response) {
    pthread_t id;
    auto args = std::make_unique<ThreadArgs>();
    args->platform_service_instance = this;
    args->response = response;
    pthread_create(&id, NULL, &EchoServiceImpl::AsyncResponseTrampoline,
                   args.release());
    workers.push_back(id);
  }

  void RespondAsynchronously(const std::string& response) {
    int64_t work_time_us =
        starboard::CurrentPosixTime() + kAsyncResponseDelaySec * 1'000'000;
    struct timespec work_time = {0};
    work_time.tv_sec = work_time_us / 1'000'000;
    work_time.tv_nsec = (work_time_us % 1'000'000) * 1000;

    pthread_mutex_lock(&m);
    while (!should_shutdown) {
      int ret = pthread_cond_timedwait(&cv, &m, &work_time);
      if (ret == ETIMEDOUT) {
        break;
      }
    }

    if (should_shutdown) {
      pthread_mutex_unlock(&m);
      return;
    }

    pthread_mutex_unlock(&m);
    receive_callback(context, static_cast<const void*>(response.c_str()),
                     response.length());
  }

  void CleanUpWorkers() {
    pthread_mutex_lock(&m);

    if (should_shutdown) {  // Cleanup was already done
      pthread_mutex_unlock(&m);
      return;
    }

    should_shutdown = true;
    pthread_cond_broadcast(&cv);
    pthread_mutex_unlock(&m);

    for (pthread_t worker : workers) {
      pthread_join(worker, nullptr);
    }

    workers.clear();
  }

} EchoServiceImpl;

bool Has(const char* name) {
  // Check if platform has service name.
  return strcmp(name, kEchoServiceName) == 0;
}

PlatformServiceImpl* Open(void* context,
                          ReceiveMessageCallback receive_callback) {
  SB_DCHECK(context);
  SB_LOG(INFO) << "Open() service created: " << kEchoServiceName;

  return new EchoServiceImpl(context, receive_callback);
}

void Close(PlatformServiceImpl* service) {
  // Function Close shouldn't manually delete PlatformServiceImpl pointer,
  // because it is managed by unique_ptr in Platform Service.
  SB_LOG(INFO)
      << kEchoServiceName
      << " Perform actions before gracefully shutting down the service";

  static_cast<EchoServiceImpl*>(service)->CleanUpWorkers();
}

void* Send(PlatformServiceImpl* service,
           void* data,
           uint64_t length,
           uint64_t* output_length,
           bool* invalid_state) {
  SB_DCHECK(service);
  SB_DCHECK(data);
  SB_DCHECK(output_length);

  // For simplicity, this service assumes the data sent to it represents a
  // string. If there's a desire to use the service to test binary data
  // payloads that are not necessarily strings then it would be better to use
  // a C-style char array or std::vector<uint8_t>.
  std::string message(static_cast<const char*>(data), length);

  SB_LOG(INFO) << "Send() message: " << message;

  std::string sync_response = std::string("Synchronous echo: ") + message;
  std::string async_response = std::string("Asynchronous echo: ") + message;

  // Initiate the async response.
  // Rather than manage a thread pool we just create a new worker thread for
  // each Send() request. This should be fine since this service is only used
  // for testing.
  static_cast<EchoServiceImpl*>(service)->StartAsyncResponseThread(
      async_response);

  // Return the sync response.
  *output_length = sync_response.length();
  auto ptr = malloc(*output_length);
  if (!ptr) {
    SB_LOG(ERROR) << "Failed to allocate memory for sync response";
    *output_length = 0;
    return nullptr;
  }
  sync_response.copy(reinterpret_cast<char*>(ptr), sync_response.length());
  return ptr;
}

const CobaltPlatformServiceApi kGetEchoServiceApi = {
    kEchoServiceName,
    1,  // API version that's implemented.
    &Has,
    &Open,
    &Close,
    &Send};

}  // namespace

const void* GetEchoServiceApi() {
  return &kGetEchoServiceApi;
}

}  // namespace starboard
