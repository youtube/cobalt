// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_H5VCC_H5VCC_PLATFORM_SERVICE_H_
#define COBALT_H5VCC_H5VCC_PLATFORM_SERVICE_H_

#include <map>
#include <string>
#include <vector>

#include "base/message_loop/message_loop.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "cobalt/dom/dom_exception.h"
#include "cobalt/extension/platform_service.h"
#include "cobalt/script/array_buffer.h"
#include "cobalt/script/callback_function.h"
#include "cobalt/script/global_environment.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace h5vcc {

class H5vccPlatformService : public script::Wrappable {
 public:
  using ExtPlatformService = CobaltExtensionPlatformService;
  using ExtPlatformServiceApi = CobaltExtensionPlatformServiceApi;

  using ReceiveCallback = script::CallbackFunction<void(
      const scoped_refptr<H5vccPlatformService>&,
      const script::Handle<script::ArrayBuffer>&)>;
  using ReceiveCallbackArg = script::ScriptValue<ReceiveCallback>;
  using ReceiveCallbackReference = ReceiveCallbackArg::Reference;

  H5vccPlatformService(script::GlobalEnvironment* environment,
                       const ExtPlatformServiceApi* platform_service_api,
                       const ReceiveCallbackArg& receive_callback);

  ~H5vccPlatformService();

  static bool Has(const std::string& service_name);

  static scoped_refptr<H5vccPlatformService> Open(
      script::EnvironmentSettings* settings, const std::string service_name,
      const ReceiveCallbackArg& receive_callback);

  script::Handle<script::ArrayBuffer> Send(
      const script::Handle<script::ArrayBuffer>& data,
      script::ExceptionState* exception_state);

  void Close();

  DEFINE_WRAPPABLE_TYPE(H5vccPlatformService);

 private:
  static constexpr size_t kMaxNameLength = 1024;

  void ReceiveInternal(std::vector<uint8_t> data);

  static void Receive(void* context, const void* data, uint64_t length);

  bool IsOpen();

  script::GlobalEnvironment* environment_;
  const ExtPlatformServiceApi* platform_service_api_;
  const ReceiveCallbackReference receive_callback_;
  // The main message loop.
  scoped_refptr<base::SingleThreadTaskRunner> const main_message_loop_;

  base::WeakPtrFactory<H5vccPlatformService> weak_ptr_factory_;
  // We construct a WeakPtr upon H5vccPlatformService's construction in order to
  // associate the WeakPtr with the constructing thread.
  base::WeakPtr<H5vccPlatformService> weak_this_;

  ExtPlatformService ext_service_ = kCobaltExtensionPlatformServiceInvalid;
};

}  // namespace h5vcc
}  // namespace cobalt

#endif  // COBALT_H5VCC_H5VCC_PLATFORM_SERVICE_H_
