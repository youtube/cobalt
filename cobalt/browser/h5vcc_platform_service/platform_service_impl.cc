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

#include "cobalt/browser/h5vcc_platform_service/platform_service_impl.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "starboard/extension/platform_service.h"
#include "starboard/system.h"

namespace h5vcc_platform_service {

namespace {

const CobaltExtensionPlatformServiceApi* GetPlatformServiceApi() {
  static const CobaltExtensionPlatformServiceApi* s_api = []() {
    auto api = static_cast<const CobaltExtensionPlatformServiceApi*>(
        SbSystemGetExtension(kCobaltExtensionPlatformServiceName));
    if (!api) {
      LOG(WARNING) << "The extension is not implemented on this platform";
    }
    return api;
  }();
  return s_api;
}

}  // namespace

// static
void PlatformServiceImpl::Create(
    content::RenderFrameHost& render_frame_host,
    const std::string& service_name,
    mojo::PendingRemote<mojom::PlatformServiceObserver> observer,
    mojo::PendingReceiver<mojom::PlatformService> receiver) {
  if (!render_frame_host.IsActive()) {
    return;
  }

  PlatformServiceImpl* instance =
      new PlatformServiceImpl(render_frame_host, service_name,
                              std::move(observer), std::move(receiver));
  if (!instance->OpenStarboardService()) {
    LOG(ERROR) << "Failed to OpenStarboardService: " << service_name;
  }
}

// static
void PlatformServiceImpl::StarboardReceiveMessageCallback(void* context,
                                                          const void* data,
                                                          uint64_t length) {
  if (!context) {
    LOG(WARNING) << "StarboardReceiveMessageCallback has null context.";
    return;
  }
  PlatformServiceImpl* instance = static_cast<PlatformServiceImpl*>(context);

  const uint8_t* byte_data = static_cast<const uint8_t*>(data);
  std::vector<uint8_t> data_vector(byte_data, byte_data + length);

  // content::DocumentService is bound to the UI thread, via the RFH.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&PlatformServiceImpl::OnDataReceivedFromStarboard,
                     instance->weak_factory_.GetWeakPtr(),
                     std::move(data_vector)));
}

PlatformServiceImpl::PlatformServiceImpl(
    content::RenderFrameHost& render_frame_host,
    const std::string& service_name,
    mojo::PendingRemote<mojom::PlatformServiceObserver> observer,
    mojo::PendingReceiver<mojom::PlatformService> receiver)
    : DocumentService(render_frame_host, std::move(receiver)),
      service_name_(service_name),
      observer_(std::move(observer)) {
  observer_.set_disconnect_handler(base::BindOnce(
      [] { LOG(INFO) << "PlatformServiceObserver disconnected."; }));
  LOG(INFO) << "PlatformServiceImpl created for " << service_name_;
}

PlatformServiceImpl::~PlatformServiceImpl() {
  LOG(INFO) << "Destroying PlatformServiceImpl for " << service_name_;

  if (CobaltExtensionPlatformServiceIsValid(platform_service_)) {
    const CobaltExtensionPlatformServiceApi* api = GetPlatformServiceApi();
    if (api) {
      api->Close(platform_service_);
    }
  }
}

bool PlatformServiceImpl::OpenStarboardService() {
  const CobaltExtensionPlatformServiceApi* api = GetPlatformServiceApi();
  if (!api) {
    LOG(WARNING) << "The extension is not implemented on this platform";
    return false;
  }

  CobaltExtensionPlatformService service =
      api->Open(this, service_name_.c_str(),
                &PlatformServiceImpl::StarboardReceiveMessageCallback);

  if (!CobaltExtensionPlatformServiceIsValid(service)) {
    LOG(ERROR) << "Failed to open Starboard service: " << service_name_;
    return false;
  }

  platform_service_ = service;
  return true;
}

void PlatformServiceImpl::Send(const std::vector<uint8_t>& data,
                               SendCallback callback) {
  const CobaltExtensionPlatformServiceApi* api = GetPlatformServiceApi();
  if (!api) {
    LOG(WARNING) << "PlatformService is not implemented on this platform.";
    std::move(callback).Run({});  // Return empty vector on error
    return;
  }

  uint64_t output_length = 0;
  std::vector<uint8_t> mutable_data = data;
  void* data_ptr = mutable_data.empty() ? nullptr : mutable_data.data();
  uint64_t data_length = mutable_data.size();
  bool invalid_state = false;

  void* response_ptr = api->Send(platform_service_, data_ptr, data_length,
                                 &output_length, &invalid_state);

  if (invalid_state) {
    LOG(ERROR) << "Send failed: Starboard service in invalid state for "
               << service_name_;
    if (response_ptr) {
      free(response_ptr);
    }
    std::move(callback).Run({});  // Return empty vector on error
    return;
  }

  std::vector<uint8_t> response_data;
  if (response_ptr && output_length > 0) {
    const uint8_t* response_bytes = static_cast<const uint8_t*>(response_ptr);
    response_data =
        std::vector<uint8_t>(response_bytes, response_bytes + output_length);
  }

  if (response_ptr) {
    free(response_ptr);
  }

  std::move(callback).Run(response_data);
}

void PlatformServiceImpl::OnDataReceivedFromStarboard(
    const std::vector<uint8_t>& data) {
  if (!observer_.is_bound()) {
    LOG(WARNING)
        << "Starboard data received, but observer is not bound for service "
        << service_name_;
    return;
  }
  if (!render_frame_host().IsActive()) {
    LOG(WARNING)
        << "Starboard data received, but RFH is not active for service "
        << service_name_;
    return;
  }

  observer_->OnDataReceived(data);
}

}  // namespace h5vcc_platform_service
