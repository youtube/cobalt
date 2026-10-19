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

#include "cobalt/browser/h5vcc_platform_service/platform_service_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/free_deleter.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/elapsed_timer.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/render_frame_host.h"
#include "starboard/extension/platform_service.h"
#include "starboard/system.h"

namespace h5vcc_platform_service {

namespace {

// The limit is defined by the platform service extension, so that Cobalt and
// the platform service implementations agree on it.
constexpr uint64_t kMaxMessageLength =
    kCobaltExtensionPlatformServiceMaxMessageLength;

const CobaltExtensionPlatformServiceApi* g_api_for_testing = nullptr;

void RecordOpenMetrics(bool success, base::TimeDelta duration) {
  base::UmaHistogramBoolean("Cobalt.PlatformService.Open.Success", success);
  base::UmaHistogramMicrosecondsTimes("Cobalt.PlatformService.Open.DurationUs",
                                      duration);
}

void RecordSendMetrics(PlatformServiceSendStatus status,
                       base::TimeDelta duration) {
  base::UmaHistogramEnumeration("Cobalt.PlatformService.Send.Status", status);
  base::UmaHistogramMicrosecondsTimes("Cobalt.PlatformService.Send.DurationUs",
                                      duration);
}

class DocumentPlatformService
    : public content::DocumentService<mojom::PlatformService> {
 public:
  DocumentPlatformService(
      content::RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<mojom::PlatformService> receiver,
      std::unique_ptr<PlatformServiceImpl> impl)
      : DocumentService(render_frame_host, std::move(receiver)),
        impl_(std::move(impl)) {}

  void Send(base::span<const uint8_t> data, SendCallback callback) override {
    impl_->Send(data, std::move(callback));
  }

 private:
  std::unique_ptr<PlatformServiceImpl> impl_;
};

}  // namespace

// static
const CobaltExtensionPlatformServiceApi*
PlatformServiceImpl::GetPlatformServiceApi() {
  if (g_api_for_testing) {
    return g_api_for_testing;
  }
  static const CobaltExtensionPlatformServiceApi* s_api = []() {
    auto api = static_cast<const CobaltExtensionPlatformServiceApi*>(
        SbSystemGetExtension(kCobaltExtensionPlatformServiceName));
    if (!api) {
      LOG(WARNING) << "The platform service extension is not implemented on "
                   << "this platform";
    }
    return api;
  }();
  return s_api;
}

// static
void PlatformServiceImpl::SetPlatformServiceApiForTesting(
    const CobaltExtensionPlatformServiceApi* api) {
  g_api_for_testing = api;
}

// static
void PlatformServiceImpl::Create(
    content::RenderFrameHost& render_frame_host,
    const std::string& service_name,
    mojo::PendingRemote<mojom::PlatformServiceObserver> observer,
    mojo::PendingReceiver<mojom::PlatformService> receiver) {
  if (!render_frame_host.IsActive()) {
    return;
  }

  auto instance = base::WrapUnique(new PlatformServiceImpl(
      &render_frame_host, service_name, std::move(observer)));
  if (!instance->OpenStarboardService()) {
    LOG(ERROR) << "Failed to OpenStarboardService: " << service_name;
    return;
  }
  new DocumentPlatformService(render_frame_host, std::move(receiver),
                              std::move(instance));
}

// static
std::unique_ptr<PlatformServiceImpl> PlatformServiceImpl::CreateOffUiThread(
    const std::string& service_name,
    mojo::PendingRemote<mojom::PlatformServiceObserver> observer) {
  auto instance = base::WrapUnique(new PlatformServiceImpl(
      /*render_frame_host=*/nullptr, service_name, std::move(observer)));
  if (!instance->OpenStarboardService()) {
    LOG(ERROR) << "Failed to OpenStarboardService: " << service_name;
    return nullptr;
  }
  return instance;
}

// static
void PlatformServiceImpl::StarboardReceiveMessageCallback(void* context,
                                                          const void* data,
                                                          uint64_t length) {
  if (!context) {
    LOG(WARNING) << "StarboardReceiveMessageCallback has null context.";
    return;
  }

  if (length > kMaxMessageLength) {
    LOG(ERROR) << "Dropping a received message of " << length
               << " bytes, the limit is " << kMaxMessageLength << " bytes.";
    return;
  }

  if (length > 0 && !data) {
    LOG(ERROR) << "Dropping a received message with null data and a length of "
               << length << " bytes.";
    return;
  }

  PlatformServiceImpl* instance = static_cast<PlatformServiceImpl*>(context);

  const uint8_t* byte_data = static_cast<const uint8_t*>(data);
  std::vector<uint8_t> data_vector(byte_data, byte_data + length);

  instance->on_data_received_callback_.Run(std::move(data_vector));
}

PlatformServiceImpl::PlatformServiceImpl(
    content::RenderFrameHost* render_frame_host,
    const std::string& service_name,
    mojo::PendingRemote<mojom::PlatformServiceObserver> observer)
    : render_frame_host_(render_frame_host),
      service_name_(service_name),
      observer_(std::move(observer)) {
  on_data_received_callback_ = base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindRepeating(&PlatformServiceImpl::OnDataReceivedFromStarboard,
                          weak_factory_.GetWeakPtr()));
  observer_.set_disconnect_handler(base::BindOnce(
      [] { LOG(INFO) << "PlatformServiceObserver disconnected."; }));
  LOG(INFO) << "PlatformServiceImpl created for " << service_name_;
}

PlatformServiceImpl::~PlatformServiceImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(INFO) << "Destroying PlatformServiceImpl for " << service_name_;

  if (CobaltExtensionPlatformServiceIsValid(platform_service_)) {
    const CobaltExtensionPlatformServiceApi* api = GetPlatformServiceApi();
    if (api) {
      api->Close(platform_service_);
    }
  }
}

bool PlatformServiceImpl::OpenStarboardService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::ElapsedTimer timer;
  const CobaltExtensionPlatformServiceApi* api = GetPlatformServiceApi();
  if (!api) {
    LOG(WARNING) << "The platform service extension is not implemented on this "
                 << "platform";
    RecordOpenMetrics(false, timer.Elapsed());
    return false;
  }

  CobaltExtensionPlatformService service =
      api->Open(this, service_name_.c_str(),
                &PlatformServiceImpl::StarboardReceiveMessageCallback);

  if (!CobaltExtensionPlatformServiceIsValid(service)) {
    LOG(ERROR) << "Failed to open Starboard service: " << service_name_;
    RecordOpenMetrics(false, timer.Elapsed());
    return false;
  }

  platform_service_ = service;
  RecordOpenMetrics(true, timer.Elapsed());
  return true;
}

void PlatformServiceImpl::Send(base::span<const uint8_t> data,
                               SendCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::ElapsedTimer timer;
  if (data.size() > kMaxMessageLength) {
    LOG(ERROR) << "Rejecting a message of " << data.size()
               << " bytes, the limit is " << kMaxMessageLength << " bytes, for "
               << service_name_;
    RecordSendMetrics(PlatformServiceSendStatus::kMessageTooLarge,
                      timer.Elapsed());
    std::move(callback).Run(std::nullopt, "Message too large");
    return;
  }

  const CobaltExtensionPlatformServiceApi* api = GetPlatformServiceApi();
  if (!api) {
    LOG(WARNING) << "The platform service extension is not implemented on this "
                 << "platform";
    RecordSendMetrics(PlatformServiceSendStatus::kExtensionNotImplemented,
                      timer.Elapsed());
    std::move(callback).Run(std::nullopt,
                            "Platform service extension not implemented");
    return;
  }

  if (!CobaltExtensionPlatformServiceIsValid(platform_service_)) {
    LOG(ERROR) << "Rejecting Send for unopened or invalid service "
               << service_name_;
    RecordSendMetrics(PlatformServiceSendStatus::kInvalidServiceHandle,
                      timer.Elapsed());
    std::move(callback).Run(std::nullopt,
                            "Platform service is not open or valid");
    return;
  }

  uint64_t output_length = 0;
  bool invalid_state = false;

  const std::unique_ptr<uint8_t, base::FreeDeleter> response_ptr(
      static_cast<uint8_t*>(api->Send(platform_service_, data.data(),
                                      data.size(), &output_length,
                                      &invalid_state)));

  if (invalid_state) {
    std::string error_message = "Starboard service in invalid state";
    if (output_length > 0 && response_ptr) {
      error_message = std::string(reinterpret_cast<char*>(response_ptr.get()),
                                  base::checked_cast<size_t>(output_length));
    }
    LOG(ERROR) << "Send failed: " << error_message << " for " << service_name_;
    RecordSendMetrics(PlatformServiceSendStatus::kStarboardInvalidState,
                      timer.Elapsed());
    std::move(callback).Run(std::nullopt, error_message);
    return;
  }

  if (output_length == 0) {
    RecordSendMetrics(PlatformServiceSendStatus::kSuccess, timer.Elapsed());
    std::move(callback).Run(mojo_base::BigBuffer(), std::nullopt);
    return;
  }

  if (!response_ptr) {
    LOG(ERROR) << "Send failed: null response with non-zero length for "
               << service_name_;
    RecordSendMetrics(PlatformServiceSendStatus::kNullResponseData,
                      timer.Elapsed());
    std::move(callback).Run(std::nullopt, "Failed to retrieve response data.");
    return;
  }

  RecordSendMetrics(PlatformServiceSendStatus::kSuccess, timer.Elapsed());
  std::move(callback).Run(
      mojo_base::BigBuffer(base::span<const uint8_t>(
          response_ptr.get(), base::checked_cast<size_t>(output_length))),
      std::nullopt);
}

void PlatformServiceImpl::OnDataReceivedFromStarboard(
    const std::vector<uint8_t>& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!observer_.is_bound()) {
    LOG(WARNING)
        << "Starboard data received, but observer is not bound for service "
        << service_name_;
    return;
  }
  if (render_frame_host_ && !render_frame_host_->IsActive()) {
    LOG(WARNING)
        << "Starboard data received, but RFH is not active for service "
        << service_name_;
    return;
  }

  observer_->OnDataReceived(data);
}

}  // namespace h5vcc_platform_service
