// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/fake_video_capture_device.h"

#include <string>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/stringprintf.h"

namespace media {

static const int kFakeCaptureTimeoutMs = 100;
enum { kNumberOfFakeDevices = 2 };

void FakeVideoCaptureDevice::GetDeviceNames(Names* const device_names) {
  // Empty the name list.
  device_names->erase(device_names->begin(), device_names->end());

  for (int n = 0; n < kNumberOfFakeDevices; n++) {
    Name name;
    name.unique_id = StringPrintf("/dev/video%d", n);
    name.device_name = StringPrintf("fake_device_%d", n);
    device_names->push_back(name);
  }
}

VideoCaptureDevice* FakeVideoCaptureDevice::Create(const Name& device_name) {
  for (int n = 0; n < kNumberOfFakeDevices; ++n) {
    std::string possible_id = StringPrintf("/dev/video%d", n);
    if (device_name.unique_id.compare(possible_id) == 0) {
      return new FakeVideoCaptureDevice(device_name);
    }
  }
  return NULL;
}

FakeVideoCaptureDevice::FakeVideoCaptureDevice(const Name& device_name)
    : device_name_(device_name),
      observer_(NULL),
      state_(kIdle),
      capture_thread_("CaptureThread") {
}

FakeVideoCaptureDevice::~FakeVideoCaptureDevice() {
  // Check if the thread is running.
  // This means that the device have not been DeAllocated properly.
  DCHECK(!capture_thread_.IsRunning());
}

void FakeVideoCaptureDevice::Allocate(int width,
                                       int height,
                                       int frame_rate,
                                       EventHandler* observer) {
  if (state_ != kIdle) {
    return;  // Wrong state.
  }

  observer_ = observer;
  Capability current_settings;
  current_settings.color = kI420;
  if (width > 320) {  // VGA
    current_settings.width = 640;
    current_settings.height = 480;
    current_settings.frame_rate = 30;
  } else {  // QVGA
    current_settings.width = 320;
    current_settings.height = 240;
    current_settings.frame_rate = 30;
  }

  size_t fake_frame_size =
      current_settings.width * current_settings.height * 3 / 2;
  fake_frame_.reset(new uint8[fake_frame_size]);
  memset(fake_frame_.get(), 0, fake_frame_size);

  state_ = kAllocated;
  observer_->OnFrameInfo(current_settings);
}

void FakeVideoCaptureDevice::Start() {
  if (state_ != kAllocated) {
      return;  // Wrong state.
  }
  state_ = kCapturing;
  capture_thread_.Start();
  capture_thread_.message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&FakeVideoCaptureDevice::OnCaptureTask,
                 base::Unretained(this)));
}

void FakeVideoCaptureDevice::Stop() {
  if (state_ != kCapturing) {
      return;  // Wrong state.
  }
  capture_thread_.Stop();
  state_ = kAllocated;
}

void FakeVideoCaptureDevice::DeAllocate() {
  if (state_ != kAllocated && state_ != kCapturing) {
      return;  // Wrong state.
  }
  capture_thread_.Stop();
  state_ = kIdle;
}

const VideoCaptureDevice::Name& FakeVideoCaptureDevice::device_name() {
  return device_name_;
}

void FakeVideoCaptureDevice::OnCaptureTask() {
  if (state_ != kCapturing) {
    return;
  }
  // Give the captured frame to the observer.
  observer_->OnIncomingCapturedFrame(fake_frame_.get(),
                                     sizeof(fake_frame_.get()),
                                     base::Time::Now());
  // Reschedule next CaptureTask.
  capture_thread_.message_loop()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&FakeVideoCaptureDevice::OnCaptureTask,
                   base::Unretained(this)),
        kFakeCaptureTimeoutMs);
}

}  // namespace media
