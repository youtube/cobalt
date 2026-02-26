//
// Copyright 2020 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#include "third_party/starboard/rdk/shared/libcobalt.h"

#include <cstring>
#include <mutex>

#include "starboard/common/semaphore.h"
#include "starboard/common/once.h"

#include "third_party/starboard/rdk/shared/rdkservices.h"
#include "third_party/starboard/rdk/shared/application_rdk.h"

using namespace third_party::starboard::rdk::shared;

namespace
{

struct APIContext
{
  APIContext()
    : mutex_()
  { }

  void OnInitialize()
  {
    std::lock_guard lock(mutex_);
    SB_CHECK(nullptr != Application::Get());
    state_ = kRunning;
    condition_.notify_all();
  }

  void OnTeardown()
  {
    std::lock_guard lock(mutex_);
    state_ = kStopped;
  }

  void SendLink(const char* link)
  {
    std::unique_lock lock(mutex_);
    if (WaitForApp(lock) == kRunning) {
      Application::Get()->Link(link);
    }
  }

  void RequestFreeze() {
    RequestAndWait(&Application::Freeze);
  }

  void RequestFocus() {
    RequestAndWait(&Application::Focus);
  }

  void RequestBlur() {
    RequestAndWait(&Application::Blur);
  }

  void RequestQuit()
  {
    std::lock_guard lock(mutex_);
    stop_request_cb_ = nullptr;
    stop_request_cb_data_ = nullptr;
    if (state_ == kRunning)
        Application::Get()->Stop(0);
  }

  void SetStopRequestHandler(SbRdkCallbackFunc cb, void* user_data)
  {
    std::lock_guard lock(mutex_);
    stop_request_cb_ = cb;
    stop_request_cb_data_ = user_data;
  }

  void RequestStop()
  {
    SbRdkCallbackFunc cb;
    void* user_data;
    int should_invoke_default = 1;

    std::unique_lock lock(mutex_);
    cb = stop_request_cb_;
    user_data = stop_request_cb_data_;
    lock.unlock();

    if (cb) {
      should_invoke_default = cb(user_data);
    }

    if (should_invoke_default) {
      RequestQuit();
    }
  }

  void SetConcealRequestHandler(SbRdkCallbackFunc cb, void* user_data)
  {
    std::lock_guard lock(mutex_);
    conceal_request_cb_ = cb;
    conceal_request_cb_data_ = user_data;
  }

  void RequestConceal()
  {
    SbRdkCallbackFunc cb;
    void* user_data;
    int should_invoke_default = 1;

    std::unique_lock lock(mutex_);
    cb = conceal_request_cb_;
    user_data = conceal_request_cb_data_;
    lock.unlock();

    if (cb) {
      should_invoke_default = cb(user_data);
    }

    if (should_invoke_default) {
      std::lock_guard lock(mutex_);
      if (state_ == kRunning) {
        Application::Get()->Conceal(NULL, NULL);
      }
    }
  }

  void SetCobaltExitStrategy(const char* strategy)
  {
    if (state_ == kRunning) {
      SB_LOG(WARNING) << "Ignore exit strategy change, app is already running.";
      return;
    }

    // Supported values(src/cobalt/extension/configuration.h): stop, suspend, noexit.
    if (strncmp(strategy, "suspend", 7) == 0)
      exit_strategy_ = "suspend";
    else if (strncmp(strategy, "noexit", 6) == 0)
      exit_strategy_ = "noexit";
    else
      exit_strategy_ = "stop";
  }

  const char* GetCobaltExitStrategy()
  {
    return exit_strategy_.c_str();
  }

private:
  enum State {
    kUninitialized,
    kRunning,
    kStopped
  };

  State WaitForApp(std::unique_lock<std::mutex>& lock)
  {
    while ( state_ == kUninitialized )
      condition_.wait(lock);

    return state_;
  }

  void RequestAndWait(void (Application::*action)(void*, Application::EventHandledCallback)) {
    std::unique_lock lock(mutex_);
    if (WaitForApp(lock) == kRunning) {
      starboard::Semaphore sem;
      (Application::Get()->*action)(
        &sem,
        [](void* ctx) {
          reinterpret_cast<starboard::Semaphore*>(ctx)->Put();
        });
      lock.unlock();
      sem.Take();
    }
    else {
      lock.unlock();
    }
  }

  State state_ { kUninitialized };
  std::mutex mutex_;
  std::condition_variable condition_;
  SbRdkCallbackFunc stop_request_cb_ { nullptr };
  void* stop_request_cb_data_ { nullptr };
  SbRdkCallbackFunc conceal_request_cb_ { nullptr };
  void* conceal_request_cb_data_ { nullptr };
  std::string exit_strategy_ { "stop" };
};

SB_ONCE_INITIALIZE_FUNCTION(APIContext, GetContext);

}  // namespace

namespace third_party {
namespace starboard {
namespace rdk {
namespace shared {
namespace libcobalt_api {

void Initialize()
{
  GetContext()->OnInitialize();
}

void Teardown()
{
  GetContext()->OnTeardown();
}

}  // namespace libcobalt_api
}  // namespace shared
}  // namespace rdk
}  // namespace starboard
}  // namespace third_party

extern "C" {

void SbRdkHandleDeepLink(const char* link) {
  GetContext()->SendLink(link);
}

void SbRdkSuspend() {
  GetContext()->RequestFreeze();
}

void SbRdkResume() {
  GetContext()->RequestFocus();
}

void SbRdkPause() {
  GetContext()->RequestBlur();
}

void SbRdkUnpause() {
  GetContext()->RequestFocus();
}

void SbRdkQuit() {
  GetContext()->RequestQuit();
}

void SbRdkSetSetting(const char* key, const char* json) {
  if (!key || key[0] == '\0' || !json)
    return;

  if (strcmp(key, "accessibility") == 0) {
    Accessibility::SetSettings(json);
  }
  else if (strcmp(key, "systemproperties") == 0) {
    SystemProperties::SetSettings(json);
  }
  else if (strcmp(key, "advertisingid") == 0) {
    AdvertisingId::SetSettings(json);
  }
}

int SbRdkGetSetting(const char* key, char** out_json) {
  if (!key || key[0] == '\0' || !out_json || *out_json != nullptr)
    return -1;

  bool result = false;
  std::string tmp;

  if (strcmp(key, "accessibility") == 0) {
    result = Accessibility::GetSettings(tmp);
  }
  else if (strcmp(key, "systemproperties") == 0) {
    result = SystemProperties::GetSettings(tmp);
  }
  else if (strcmp(key, "advertisingid") == 0) {
    result = AdvertisingId::GetSettings(tmp);
  }

  if (result && !tmp.empty()) {
    char *out = (char*)malloc(tmp.size() + 1);
    memcpy(out, tmp.c_str(), tmp.size());
    out[tmp.size()] = '\0';
    *out_json = out;
    return 0;
  }

  return -1;
}

void SbRdkSetStopRequestHandler(SbRdkCallbackFunc cb, void* user_data) {
  GetContext()->SetStopRequestHandler(cb, user_data);
}

void SbRdkRequestStop() {
  GetContext()->RequestStop();
}

void SbRdkSetConcealRequestHandler(SbRdkCallbackFunc cb, void* user_data) {
  GetContext()->SetConcealRequestHandler(cb, user_data);
}

void SbRdkRequestConceal() {
  GetContext()->RequestConceal();
}

void SbRdkSetCobaltExitStrategy(const char* strategy) {
  GetContext()->SetCobaltExitStrategy(strategy);
}

const char* SbRdkGetCobaltExitStrategy() {
  return GetContext()->GetCobaltExitStrategy();
}

}  // extern "C"
