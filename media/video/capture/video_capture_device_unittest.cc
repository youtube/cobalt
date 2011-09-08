// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "media/video/capture/fake_video_capture_device.h"
#include "media/video/capture/video_capture_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;
using ::testing::AtLeast;

namespace media {

class MockFrameObserver: public media::VideoCaptureDevice::EventHandler {
 public:
  MOCK_METHOD0(OnErr, void());
  MOCK_METHOD3(OnFrameInfo, void(int width, int height, int frame_rate));

  explicit MockFrameObserver(base::WaitableEvent* wait_event)
     : wait_event_(wait_event) {}

  virtual void OnError() OVERRIDE {
    OnErr();
  }

  virtual void OnFrameInfo(
      const VideoCaptureDevice::Capability& info) OVERRIDE {
    OnFrameInfo(info.width, info.height, info.frame_rate);
  }

  virtual void OnIncomingCapturedFrame(const uint8* data, int length,
                                       base::Time timestamp) OVERRIDE {
    wait_event_->Signal();
  }

 private:
  base::WaitableEvent* wait_event_;
};

class VideoCaptureDeviceTest : public testing::Test {
 public:
  VideoCaptureDeviceTest(): wait_event_(false, false) { }

 protected:
  virtual void SetUp() {
    frame_observer_.reset(new MockFrameObserver(&wait_event_));
  }

  virtual void TearDown() {
  }

  base::WaitableEvent wait_event_;
  scoped_ptr<MockFrameObserver> frame_observer_;
  VideoCaptureDevice::Names names_;
};

TEST_F(VideoCaptureDeviceTest, OpenInvalidDevice) {
  VideoCaptureDevice::Name device_name;
  device_name.device_name = "jibberish";
  device_name.unique_id = "jibberish";
  VideoCaptureDevice* device = VideoCaptureDevice::Create(device_name);
  EXPECT_TRUE(device == NULL);
}

TEST_F(VideoCaptureDeviceTest, CaptureVGA) {
  VideoCaptureDevice::GetDeviceNames(&names_);
  if (!names_.size()) {
    LOG(WARNING) << "No camera available. Exiting test.";
    return;
  }

  scoped_ptr<VideoCaptureDevice> device(
      VideoCaptureDevice::Create(names_.front()));
  ASSERT_FALSE(device.get() == NULL);

  // Get info about the new resolution.
  EXPECT_CALL(*frame_observer_, OnFrameInfo(640, 480, 30))
      .Times(1);

  EXPECT_CALL(*frame_observer_, OnErr())
      .Times(0);

  device->Allocate(640, 480, 30, frame_observer_.get());
  device->Start();
  // Wait for 3s or for captured frame.
  EXPECT_TRUE(wait_event_.TimedWait(base::TimeDelta::FromMilliseconds(
      TestTimeouts::action_max_timeout_ms())));
  device->Stop();
  device->DeAllocate();
}

TEST_F(VideoCaptureDeviceTest, Capture720p) {
  VideoCaptureDevice::GetDeviceNames(&names_);
  if (!names_.size()) {
    LOG(WARNING) << "No camera available. Exiting test.";
    return;
  }

  scoped_ptr<VideoCaptureDevice> device(
      VideoCaptureDevice::Create(names_.front()));
  ASSERT_FALSE(device.get() == NULL);

  // Get info about the new resolution.
  // We don't care about the resulting resolution or frame rate as it might
  // be different from one machine to the next.
  EXPECT_CALL(*frame_observer_, OnFrameInfo(_, _, _))
      .Times(1);

  EXPECT_CALL(*frame_observer_, OnErr())
      .Times(0);

  device->Allocate(1280, 720, 30, frame_observer_.get());
  device->Start();
  // Get captured video frames.
  EXPECT_TRUE(wait_event_.TimedWait(base::TimeDelta::FromMilliseconds(
      TestTimeouts::action_max_timeout_ms())));
  device->Stop();
  device->DeAllocate();
}

TEST_F(VideoCaptureDeviceTest, AllocateSameCameraTwice) {
  VideoCaptureDevice::GetDeviceNames(&names_);
  if (!names_.size()) {
    LOG(WARNING) << "No camera available. Exiting test.";
    return;
  }
  scoped_ptr<VideoCaptureDevice> device1(
      VideoCaptureDevice::Create(names_.front()));
  ASSERT_TRUE(device1.get() != NULL);

  scoped_ptr<VideoCaptureDevice> device2(
      VideoCaptureDevice::Create(names_.front()));
  ASSERT_TRUE(device2.get() != NULL);

  // 1. Get info about the new resolution on the first allocated camera
  EXPECT_CALL(*frame_observer_, OnFrameInfo(640, 480, 30));

  device1->Allocate(640, 480, 30, frame_observer_.get());

  // 2. Error when trying to allocate the same camera again.
  EXPECT_CALL(*frame_observer_, OnErr());
  device2->Allocate(640, 480, 30, frame_observer_.get());

  device1->DeAllocate();
  device2->DeAllocate();
}

TEST_F(VideoCaptureDeviceTest, AllocateBadSize) {
  VideoCaptureDevice::GetDeviceNames(&names_);
  if (!names_.size()) {
    LOG(WARNING) << "No camera available. Exiting test.";
    return;
  }
  scoped_ptr<VideoCaptureDevice> device(
      VideoCaptureDevice::Create(names_.front()));
  ASSERT_TRUE(device.get() != NULL);

  EXPECT_CALL(*frame_observer_, OnErr())
      .Times(0);

  // get info about the new resolution
  EXPECT_CALL(*frame_observer_, OnFrameInfo(640, 480 , _))
      .Times(AtLeast(1));

  device->Allocate(637, 472, 35, frame_observer_.get());
  device->DeAllocate();
}

TEST_F(VideoCaptureDeviceTest, ReAllocateCamera) {
  VideoCaptureDevice::GetDeviceNames(&names_);
  if (!names_.size()) {
    LOG(WARNING) << "No camera available. Exiting test.";
    return;
  }
  scoped_ptr<VideoCaptureDevice> device(
      VideoCaptureDevice::Create(names_.front()));
  ASSERT_TRUE(device.get() != NULL);
  EXPECT_CALL(*frame_observer_, OnErr())
      .Times(0);
  // get info about the new resolution
  EXPECT_CALL(*frame_observer_, OnFrameInfo(640, 480, _));

  EXPECT_CALL(*frame_observer_, OnFrameInfo(320, 240, _));

  device->Allocate(640, 480, 30, frame_observer_.get());
  device->Start();
  // Nothing shall happen.
  device->Allocate(1280, 1024, 30, frame_observer_.get());
  device->DeAllocate();
  // Allocate new size 320, 240
  device->Allocate(320, 240, 30, frame_observer_.get());

  device->Start();
  // Get captured video frames.
  EXPECT_TRUE(wait_event_.TimedWait(base::TimeDelta::FromMilliseconds(
      TestTimeouts::action_max_timeout_ms())));
  device->Stop();
  device->DeAllocate();
}

TEST_F(VideoCaptureDeviceTest, DeAllocateCameraWhileRunning) {
  VideoCaptureDevice::GetDeviceNames(&names_);
  if (!names_.size()) {
    LOG(WARNING) << "No camera available. Exiting test.";
    return;
  }
  scoped_ptr<VideoCaptureDevice> device(
      VideoCaptureDevice::Create(names_.front()));
  ASSERT_TRUE(device.get() != NULL);

  EXPECT_CALL(*frame_observer_, OnErr())
      .Times(0);
  // Get info about the new resolution.
  EXPECT_CALL(*frame_observer_, OnFrameInfo(640, 480, 30));

  device->Allocate(640, 480, 30, frame_observer_.get());

  device->Start();
  // Get captured video frames.
  EXPECT_TRUE(wait_event_.TimedWait(base::TimeDelta::FromMilliseconds(
      TestTimeouts::action_max_timeout_ms())));
  device->DeAllocate();
}

TEST_F(VideoCaptureDeviceTest, TestFakeCapture) {
  VideoCaptureDevice::Names names;

  FakeVideoCaptureDevice::GetDeviceNames(&names);

  ASSERT_GT(static_cast<int>(names.size()), 0);

  scoped_ptr<VideoCaptureDevice> device(
      FakeVideoCaptureDevice::Create(names.front()));
  ASSERT_TRUE(device.get() != NULL);

  // Get info about the new resolution.
  EXPECT_CALL(*frame_observer_, OnFrameInfo(640, 480, 30))
      .Times(1);

  EXPECT_CALL(*frame_observer_, OnErr())
      .Times(0);

  device->Allocate(640, 480, 30, frame_observer_.get());

  device->Start();
  EXPECT_TRUE(wait_event_.TimedWait(base::TimeDelta::FromMilliseconds(
      TestTimeouts::action_max_timeout_ms())));
  device->Stop();
  device->DeAllocate();
}

};  // namespace media
