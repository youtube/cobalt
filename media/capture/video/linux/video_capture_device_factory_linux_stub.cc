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

#include "base/notreached.h"
#include "media/capture/video/linux/video_capture_device_factory_linux.h"

namespace media {

// TODO: b/412467942 - Cobalt: Make media changes which can
// be upstreamed to Chromium.
// Note: This is a stub implementation. The actual implementation is in
// video_capture_device_factory_linux.cc which is not included in the cobalt build.
// Stub symbols have been provided here to satisfy the linker and build cobalt

VideoCaptureDeviceFactoryLinux::VideoCaptureDeviceFactoryLinux(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
  NOTIMPLEMENTED();
}

VideoCaptureDeviceFactoryLinux::~VideoCaptureDeviceFactoryLinux() {
  NOTIMPLEMENTED();
}

VideoCaptureErrorOrDevice VideoCaptureDeviceFactoryLinux::CreateDevice(
    const VideoCaptureDeviceDescriptor& device_descriptor) {
  NOTIMPLEMENTED();
  return VideoCaptureErrorOrDevice(
      VideoCaptureError::kVideoCaptureControllerInvalid);
}

void VideoCaptureDeviceFactoryLinux::GetDevicesInfo(
    GetDevicesInfoCallback callback) {
  NOTIMPLEMENTED();
}

}  // namespace media
