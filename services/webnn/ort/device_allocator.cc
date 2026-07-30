// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/device_allocator.h"

#include "base/logging.h"
#include "base/strings/cstring_view.h"
#include "services/webnn/ort/environment.h"
#include "services/webnn/ort/ort_status.h"
#include "services/webnn/ort/platform_functions_ort.h"
#include "services/webnn/public/cpp/execution_providers_info.h"
#include "third_party/windows_app_sdk_headers/src/inc/abi/winml/winml/onnxruntime_c_api.h"

namespace webnn::ort {

namespace {

// Returns the host-accessible memory info from the EP device. Currently only
// OpenVINO EP is supported. Returns nullptr for unsupported EPs.
const OrtMemoryInfo* GetMemoryInfo(const OrtApi* ort_api,
                                   const OrtEpDevice* ep_device,
                                   base::cstring_view ep_name) {
  if (ep_name == kOpenVINOExecutionProvider) {
    return ort_api->EpDevice_MemoryInfo(ep_device,
                                        OrtDeviceMemoryType_HOST_ACCESSIBLE);
  }

  return nullptr;
}

}  // namespace

// static
scoped_refptr<DeviceAllocator> DeviceAllocator::Create(
    scoped_refptr<SessionOptions> session_options,
    scoped_refptr<Environment> env) {
  const OrtApi* ort_api = PlatformFunctions::GetInstance()->ort_api();
  const OrtEpDevice* first_selected_device =
      session_options->first_selected_device();

  const char* ep_name = ort_api->EpDevice_EpName(first_selected_device);
  // SAFETY: ORT guarantees that `ep_name` is valid and null-terminated.
  const OrtMemoryInfo* memory_info =
      GetMemoryInfo(ort_api, first_selected_device,
                    UNSAFE_BUFFERS(base::cstring_view(ep_name)));
  if (!memory_info) {
    return nullptr;
  }

  // TODO(crbug.com/519646879): Remove the trivial session once WinML ships
  // ORT 1.27+, which supports getting a shared allocator directly from
  // OrtEnv without creating a session.
  //
  // Trivial ONNX model that returns a single float constant.
  // Used to create a trivial session for obtaining a device allocator.
  // Model bytes are copied from onnxruntime-genai:
  // https://github.com/microsoft/onnxruntime-genai/blob/ded6e97789ca718d76ce58bba4a2b483b10045ee/src/models/model.cpp#L743
  static constexpr uint8_t kTrivialModel[] = {
      0x08, 0x0a, 0x12, 0x01, 0x61, 0x3a, 0x53, 0x0a, 0x38, 0x12, 0x06, 0x76,
      0x61, 0x6c, 0x75, 0x65, 0x73, 0x22, 0x08, 0x43, 0x6f, 0x6e, 0x73, 0x74,
      0x61, 0x6e, 0x74, 0x2a, 0x24, 0x0a, 0x05, 0x76, 0x61, 0x6c, 0x75, 0x65,
      0x2a, 0x18, 0x08, 0x01, 0x10, 0x01, 0x42, 0x0c, 0x63, 0x6f, 0x6e, 0x73,
      0x74, 0x5f, 0x74, 0x65, 0x6e, 0x73, 0x6f, 0x72, 0x4a, 0x04, 0x00, 0x00,
      0x00, 0x00, 0xa0, 0x01, 0x04, 0x12, 0x01, 0x62, 0x62, 0x14, 0x0a, 0x06,
      0x76, 0x61, 0x6c, 0x75, 0x65, 0x73, 0x12, 0x0a, 0x0a, 0x08, 0x08, 0x01,
      0x12, 0x04, 0x0a, 0x02, 0x08, 0x01, 0x42, 0x04, 0x0a, 0x00, 0x10, 0x15};

  ScopedOrtSession trivial_session;
  CHECK_STATUS(ort_api->CreateSessionFromArray(
      env->get(), kTrivialModel, sizeof(kTrivialModel), session_options->get(),
      ScopedOrtSession::Receiver(trivial_session).get()));
  CHECK(trivial_session.get());

  ScopedOrtAllocator device_allocator;
  if (ORT_CALL_FAILED(ort_api->CreateAllocator(
          trivial_session.get(), memory_info,
          ScopedOrtAllocator::Receiver(device_allocator).get()))) {
    return nullptr;
  }
  CHECK(device_allocator.get());

  return base::MakeRefCounted<DeviceAllocator>(
      base::PassKey<DeviceAllocator>(), std::move(env),
      std::move(trivial_session), std::move(device_allocator));
}

DeviceAllocator::DeviceAllocator(base::PassKey<DeviceAllocator>,
                                 scoped_refptr<Environment> env,
                                 ScopedOrtSession trivial_session,
                                 ScopedOrtAllocator device_allocator)
    : env_(std::move(env)),
      trivial_session_(std::move(trivial_session)),
      device_allocator_(std::move(device_allocator)) {}

DeviceAllocator::~DeviceAllocator() = default;

}  // namespace webnn::ort
