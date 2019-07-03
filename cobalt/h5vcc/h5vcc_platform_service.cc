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
#if SB_API_VERSION < SB_EXTENSIONS_API_VERSION
  SB_DLOG(WARNING)
      << "PlatformService not implemented in this version of Starboard.";
  return NULL;
#else   // SB_API_VERSION < SB_EXTENSIONS_API_VERSION
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
  SbStringCopy(service_name_c_str, service_name.c_str(), kMaxNameLength);

  ExtPlatformService platform_service = platform_service_api->Open(
      service, service_name_c_str, &H5vccPlatformService::Receive);
  if (!platform_service) {
    return NULL;
  }
  service->ext_service_ = platform_service;
  return service;
#endif  // SB_API_VERSION < SB_EXTENSIONS_API_VERSION
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
#if SB_API_VERSION < SB_EXTENSIONS_API_VERSION
  DLOG(WARNING)
      << "PlatformService not implemented in this version of Starboard.";
  return false;
#else   // SB_API_VERSION < SB_EXTENSIONS_API_VERSION
  const ExtPlatformServiceApi* platform_service_api =
      static_cast<const ExtPlatformServiceApi*>(
          SbSystemGetExtension(kCobaltExtensionPlatformServiceName));
  if (!platform_service_api) {
    DLOG(WARNING) << "PlatformService is not implemented on this platform.";
    return false;
  }
  return platform_service_api->Has(service_name.c_str());
#endif  // SB_API_VERSION < SB_EXTENSIONS_API_VERSION
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
  void* output_data = platform_service_api_->Send(
      ext_service_, data->Data(), data->ByteLength(), &output_length,
      &invalid_state);
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
void H5vccPlatformService::Receive(void* context, void* data, uint64_t length) {
  DCHECK(context) << "Platform should not call Receive with NULL context";
  static_cast<H5vccPlatformService*>(context)->ReceiveInternal(data, length);
}

void H5vccPlatformService::ReceiveInternal(void* data, uint64_t length) {
  // ReceiveInternal may be called by another thread.
  if (!main_message_loop_->BelongsToCurrentThread()) {
    main_message_loop_->PostTask(
        FROM_HERE, base::Bind(&H5vccPlatformService::ReceiveInternal,
                              weak_this_, data, length));
    return;
  }
  DCHECK(main_message_loop_->BelongsToCurrentThread());
  if (!IsOpen()) {
    LOG(ERROR) << "Closed service cannot Receive.";
    return;
  }
  script::Handle<script::ArrayBuffer> data_array_buffer;
  if (length) {
    data_array_buffer = script::ArrayBuffer::New(environment_, data, length);
  } else {
    data_array_buffer = script::ArrayBuffer::New(environment_, 0);
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
