// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "media/video/capture/fake_video_capture_device.h"
#include "media/video/capture/video_capture_device.h"
#include "media/video/capture/video_capture_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

#if defined(OS_MACOSX)
// Mac/QTKit will always give you the size you ask for and this case will fail.
#define MAYBE_AllocateBadSize DISABLED_AllocateBadSize
// We will always get ARGB from the Mac/QTKit implementation.
#define MAYBE_CaptureMjpeg DISABLED_CaptureMjpeg
#elif defined(OS_WIN)
#define MAYBE_AllocateBadSize AllocateBadSize
// Windows currently uses DirectShow to convert from MJPEG and a raw format is
// always delivered.
#define MAYBE_CaptureMjpeg DISABLED_CaptureMjpeg
#else
#define MAYBE_AllocateBadSize AllocateBadSize
#define MAYBE_CaptureMjpeg CaptureMjpeg
#endif

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;
using ::testing::AtLeast;

namespace media {

class MockFrameObserver : public media::VideoCaptureDevice::EventHandler {
 public:
  MOCK_METHOD0(OnErr, void());
  MOCK_METHOD4(OnFrameInfo, void(int width, int height, int frame_rate,
                                 VideoCaptureCapability::Format format));

  explicit MockFrameObserver(base::WaitableEvent* wait_event)
     : wait_event_(wait_event) {}

  virtual void OnError() override {
    OnErr();
  }

  virtual void OnFrameInfo(
      const VideoCaptureCapability& info) override {
    OnFrameInfo(info.width, info.height, info.frame_rate, info.color);
  }

  virtual void OnIncomingCapturedFrame(const uint8* data, int length,
                                       base::Time timestamp) override {
    wait_event_->Signal();
  }

 private:
  base::WaitableEvent* wait_event_;
};

class VideoCaptureDeviceTest : public testing::Test {
 public:
  VideoCaptureDeviceTest(): wait_event_(false, false) { }

  void PostQuitTask() {
    loop_->PostTask(FROM_HERE, MessageLoop::QuitClosure());
    loop_->Run();
  }

 protected:
  virtual void SetUp() {
    frame_observer_.reset(new MockFrameObserver(&wait_event_));
    loop_.reset(new MessageLoopForUI());
  }

  virtual void TearDown() {
  }

#if defined(OS_WIN)
  base::win::ScopedCOMInitializer initialize_com_;
#endif
  base::WaitableEvent wait_event_;
  scoped_ptr<MockFrameObserver> frame_observer_;
  VideoCaptureDevice::Names names_;
  scoped_ptr<MessageLoop> loop_;
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
    DVLOG(1) << "No camera available. Exiting test.";
    return;
  }

  scoped_ptr<VideoCaptureDevice> device(
      VideoCaptureDevice::Create(names_.front()));
  ASSERT_FALSE(device.get() == NULL);

  // Get info about the new resolution.
  EXPECT_CALL(*frame_observer_, OnFrameInfo(640, 480, 30, _))
      .Times(1);

  EXPECT_CALL(*frame_observer_, OnErr())
      .Times(0);

  device->Allocate(640, 480, 30, frame_observer_.get());
  device->Start();
  // Get captured video frames.
  PostQuitTask();
  EXPECT_TRUE(wait_event_.TimedWait(TestTimeouts::action_max_timeout()));
  device->Stop();
  device->DeAllocate();
}

TEST_F(VideoCaptureDeviceTest, Capture720p) {
  VideoCaptureDevice::GetDeviceNames(&names_);
  if (!names_.size()) {
    DVLOG(1) << "No camera available. Exiting test.";
    return;
  }

  scoped_ptr<VideoCaptureDevice> device(
      VideoCaptureDevice::Create(names_.front()));
  ASSERT_FALSE(device.get() == NULL);

  // Get info about the new resolution.
  // We don't care about the resulting resolution or frame rate as it might
  // be different from one machine to the next.
  EXPECT_CALL(*frame_observer_, OnFrameInfo(_, _, _, _))
      .Times(1);

  EXPECT_CALL(*frame_observer_, OnErr())
      .Times(0);

  device->Allocate(1280, 720, 30, frame_observer_.get());
  device->Start();
  // Get captured video frames.
  PostQuitTask();
  EXPECT_TRUE(wait_event_.TimedWait(TestTimeouts::action_max_timeout()));
  device->Stop();
  device->DeAllocate();
}

TEST_F(VideoCaptureDeviceTest, MAYBE_AllocateBadSize) {
  VideoCaptureDevice::GetDeviceNames(&names_);
  if (!names_.size()) {
    DVLOG(1) << "No camera available. Exiting test.";
    return;
  }
  scoped_ptr<VideoCaptureDevice> device(
      VideoCaptureDevice::Create(names_.front()));
  ASSERT_TRUE(device.get() != NULL);

  EXPECT_CALL(*frame_observer_, OnErr())
      .Times(0);

  // get info about the new resolution
  EXPECT_CALL(*frame_observer_, OnFrameInfo(640, 480 , _, _))
      .Times(AtLeast(1));

  device->Allocate(637, 472, 35, frame_observer_.get());
  device->DeAllocate();
}

TEST_F(VideoCaptureDeviceTest, ReAllocateCamera) {
  VideoCaptureDevice::GetDeviceNames(&names_);
  if (!names_.size()) {
    DVLOG(1) << "No camera available. Exiting test.";
    return;
  }
  scoped_ptr<VideoCaptureDevice> device(
      VideoCaptureDevice::Create(names_.front()));
  ASSERT_TRUE(device.get() != NULL);
  EXPECT_CALL(*frame_observer_, OnErr())
      .Times(0);
  // get info about the new resolution
  EXPECT_CALL(*frame_observer_, OnFrameInfo(640, 480, _, _));

  EXPECT_CALL(*frame_observer_, OnFrameInfo(320, 240, _, _));

  device->Allocate(640, 480, 30, frame_observer_.get());
  device->Start();
  // Nothing shall happen.
  device->Allocate(1280, 1024, 30, frame_observer_.get());
  device->DeAllocate();
  // Allocate new size 320, 240
  device->Allocate(320, 240, 30, frame_observer_.get());

  device->Start();
  // Get captured video frames.
  PostQuitTask();
  EXPECT_TRUE(wait_event_.TimedWait(TestTimeouts::action_max_timeout()));
  device->Stop();
  device->DeAllocate();
}

TEST_F(VideoCaptureDeviceTest, DeAllocateCameraWhileRunning) {
  VideoCaptureDevice::GetDeviceNames(&names_);
  if (!names_.size()) {
    DVLOG(1) << "No camera available. Exiting test.";
    return;
  }
  scoped_ptr<VideoCaptureDevice> device(
      VideoCaptureDevice::Create(names_.front()));
  ASSERT_TRUE(device.get() != NULL);

  EXPECT_CALL(*frame_observer_, OnErr())
      .Times(0);
  // Get info about the new resolution.
  EXPECT_CALL(*frame_observer_, OnFrameInfo(640, 480, 30, _));

  device->Allocate(640, 480, 30, frame_observer_.get());

  device->Start();
  // Get captured video frames.
  PostQuitTask();
  EXPECT_TRUE(wait_event_.TimedWait(TestTimeouts::action_max_timeout()));
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
  EXPECT_CALL(*frame_observer_, OnFrameInfo(640, 480, 30, _))
      .Times(1);

  EXPECT_CALL(*frame_observer_, OnErr())
      .Times(0);

  device->Allocate(640, 480, 30, frame_observer_.get());

  device->Start();
  EXPECT_TRUE(wait_event_.TimedWait(TestTimeouts::action_max_timeout()));
  device->Stop();
  device->DeAllocate();
}

// Start the camera in 720p to capture MJPEG instead of a raw format.
TEST_F(VideoCaptureDeviceTest, MAYBE_CaptureMjpeg) {
  VideoCaptureDevice::GetDeviceNames(&names_);
  if (!names_.size()) {
    DVLOG(1) << "No camera available. Exiting test.";
    return;
  }
  scoped_ptr<VideoCaptureDevice> device(
      VideoCaptureDevice::Create(names_.front()));
  ASSERT_TRUE(device.get() != NULL);

  EXPECT_CALL(*frame_observer_, OnErr())
      .Times(0);
  // Verify we get MJPEG from the device. Not all devices can capture 1280x720
  // @ 30 fps, so we don't care about the exact resolution we get.
  EXPECT_CALL(*frame_observer_,
              OnFrameInfo(_, _, _, VideoCaptureCapability::kMJPEG));

  device->Allocate(1280, 720, 30, frame_observer_.get());

  device->Start();
  // Get captured video frames.
  PostQuitTask();
  EXPECT_TRUE(wait_event_.TimedWait(TestTimeouts::action_max_timeout()));
  device->DeAllocate();
}

};  // namespace media
