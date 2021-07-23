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

#include "cobalt/h5vcc/h5vcc_platform_service.h"

#include <utility>
#include <vector>

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/dom_settings.h"
#include "starboard/common/string.h"
#include "starboard/configuration.h"

namespace cobalt {
namespace h5vcc {

// static
scoped_refptr<H5vccPlatformService> H5vccPlatformService::Open(
    script::EnvironmentSettings* settings, const std::string service_name,
    const ReceiveCallbackArg& receive_callback) {
  DCHECK(settings);
  dom::DOMSettings* dom_settings =
      base::polymorphic_downcast<dom::DOMSettings*>(settings);
  auto* global_environment = dom_settings->global_environment();
  DCHECK(global_environment);

  const ExtPlatformServiceApi* platform_service_api =
      static_cast<const ExtPlatformServiceApi*>(
          SbSystemGetExtension(kCobaltExtensionPlatformServiceName));
  if (!platform_service_api) {
    SB_DLOG(WARNING) << "PlatformService is not implemented on this platform.";
    return NULL;
  }
  scoped_refptr<H5vccPlatformService> service = new H5vccPlatformService(
      global_environment, platform_service_api, receive_callback);
  char* service_name_c_str = new char[kMaxNameLength];
  memset(service_name_c_str, 0, kMaxNameLength);
  strncpy(service_name_c_str, service_name.c_str(), kMaxNameLength);

  ExtPlatformService platform_service = platform_service_api->Open(
      service, service_name_c_str, &H5vccPlatformService::Receive);
  if (!platform_service) {
    return NULL;
  }
  service->ext_service_ = platform_service;
  return service;
}

H5vccPlatformService::H5vccPlatformService(
    script::GlobalEnvironment* environment,
    const ExtPlatformServiceApi* platform_service_api,
    const ReceiveCallbackArg& receive_callback)
    : environment_(environment),
      platform_service_api_(platform_service_api),
      receive_callback_(this, receive_callback),
      main_message_loop_(base::MessageLoop::current()->task_runner()),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          weak_this_(weak_ptr_factory_.GetWeakPtr())) {
  DCHECK(platform_service_api_);
  DCHECK(main_message_loop_);
}

H5vccPlatformService::~H5vccPlatformService() {
  if (IsOpen()) {
    LOG(WARNING) << "Closing service due to destruction";
    Close();
  }
}

// static
bool H5vccPlatformService::Has(const std::string& service_name) {
  const ExtPlatformServiceApi* platform_service_api =
      static_cast<const ExtPlatformServiceApi*>(
          SbSystemGetExtension(kCobaltExtensionPlatformServiceName));
  if (!platform_service_api) {
    DLOG(WARNING) << "PlatformService is not implemented on this platform.";
    return false;
  }
  return platform_service_api->Has(service_name.c_str());
}

script::Handle<script::ArrayBuffer> H5vccPlatformService::Send(
    const script::Handle<script::ArrayBuffer>& data,
    script::ExceptionState* exception_state) {
  if (!IsOpen()) {
    dom::DOMException::Raise(dom::DOMException::kInvalidStateErr,
                             "Closed service should not Send.",
                             exception_state);
    return script::ArrayBuffer::New(environment_, 0);
  }
  uint64_t output_length = 0;
  bool invalid_state = 0;

  // Make sure the data pointer is not null as the platform service may not
  // handle that properly.
  void* data_ptr = data->Data();
  size_t data_length = data->ByteLength();
  if (data_ptr == nullptr) {
    // If the data length is 0, then it's okay to point to a static array.
    DCHECK(data_length == 0);
    static int null_data = 0;
    data_ptr = &null_data;
    data_length = 0;
  }

  void* output_data = platform_service_api_->Send(
      ext_service_, data_ptr, data_length, &output_length, &invalid_state);
  if (invalid_state) {
    dom::DOMException::Raise(dom::DOMException::kInvalidStateErr,
                             "Service unable to accept data currently.",
                             exception_state);
    SbMemoryDeallocate(output_data);
    return script::ArrayBuffer::New(environment_, 0);
  }
  auto output_buffer =
      script::ArrayBuffer::New(environment_, output_data, output_length);
  // Deallocate |output_data| which has been copied into the ArrayBuffer.
  SbMemoryDeallocate(output_data);
  return output_buffer;
}

// static
void H5vccPlatformService::Receive(void* context, const void* data,
                                   uint64_t length) {
  DCHECK(context) << "Platform should not call Receive with NULL context";
  // Make a copy of the data before potentially crossing thread boundaries,
  // since we don't have any guarantee of the lifetime of the given data.
  const uint8_t* input_data = static_cast<const uint8_t*>(data);
  std::vector<uint8_t> internal_data(input_data, input_data + length);
  static_cast<H5vccPlatformService*>(context)->ReceiveInternal(
      std::move(internal_data));
}

void H5vccPlatformService::ReceiveInternal(std::vector<uint8_t> data) {
  // ReceiveInternal may be called by another thread.
  if (!main_message_loop_->BelongsToCurrentThread()) {
    main_message_loop_->PostTask(
        FROM_HERE, base::Bind(&H5vccPlatformService::ReceiveInternal,
                              weak_this_, std::move(data)));
    return;
  }
  DCHECK(main_message_loop_->BelongsToCurrentThread());
  if (!IsOpen()) {
    LOG(ERROR) << "Closed service cannot Receive.";
    return;
  }
  script::Handle<script::ArrayBuffer> data_array_buffer;
  if (data.empty()) {
    data_array_buffer = script::ArrayBuffer::New(environment_, 0);
  } else {
    data_array_buffer =
        script::ArrayBuffer::New(environment_, data.data(), data.size());
  }

  const scoped_refptr<H5vccPlatformService>& service(this);
  receive_callback_.value().Run(service, data_array_buffer);
}

void H5vccPlatformService::Close() {
  DCHECK(main_message_loop_->BelongsToCurrentThread());

  if (!IsOpen()) {
    LOG(ERROR) << "Cannot close service that is not open.";
    return;
  }

  platform_service_api_->Close(ext_service_);
  ext_service_ = kCobaltExtensionPlatformServiceInvalid;
}

bool H5vccPlatformService::IsOpen() {
  return CobaltExtensionPlatformServiceIsValid(ext_service_);
}

}  // namespace h5vcc
}  // namespace cobalt
