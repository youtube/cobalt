// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/win/scoped_com_initializer.h"
#include "media/audio/win/core_audio_util_win.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::win::ScopedCOMInitializer;

namespace media {

class CoreAudioUtilWinTest : public ::testing::Test {
 protected:
  // The test runs on a COM thread in the multithreaded apartment (MTA).
  // If we don't initialize the COM library on a thread before using COM,
  // all function calls will return CO_E_NOTINITIALIZED.
  CoreAudioUtilWinTest()
      : com_init_(ScopedCOMInitializer::kMTA) {
    DCHECK(com_init_.succeeded());
  }
  virtual ~CoreAudioUtilWinTest() {}

  bool CanRunAudioTest() {
    bool core_audio = CoreAudioUtil::IsSupported();
    if (!core_audio)
      return false;
    int capture_devices = CoreAudioUtil::NumberOfActiveDevices(eCapture);
    int render_devices = CoreAudioUtil::NumberOfActiveDevices(eRender);
    return ((capture_devices > 0) && (render_devices > 0));
  }

  ScopedCOMInitializer com_init_;
};

TEST_F(CoreAudioUtilWinTest, NumberOfActiveDevices) {
  if (!CanRunAudioTest())
    return;

  int render_devices = CoreAudioUtil::NumberOfActiveDevices(eRender);
  EXPECT_GT(render_devices, 0);
  int capture_devices = CoreAudioUtil::NumberOfActiveDevices(eCapture);
  EXPECT_GT(capture_devices, 0);
  int total_devices = CoreAudioUtil::NumberOfActiveDevices(eAll);
  EXPECT_EQ(total_devices, render_devices + capture_devices);
}

TEST_F(CoreAudioUtilWinTest, CreateDeviceEnumerator) {
  if (!CanRunAudioTest())
    return;

  ScopedComPtr<IMMDeviceEnumerator> enumerator =
      CoreAudioUtil::CreateDeviceEnumerator();
  EXPECT_TRUE(enumerator);
}

TEST_F(CoreAudioUtilWinTest, CreateDefaultDevice) {
  if (!CanRunAudioTest())
    return;

  struct {
    EDataFlow flow;
    ERole role;
  } data[] = {
    {eRender, eConsole},
    {eRender, eCommunications},
    {eRender, eMultimedia},
    {eCapture, eConsole},
    {eCapture, eCommunications},
    {eCapture, eMultimedia}
  };

  // Create default devices for all flow/role combinations above.
  ScopedComPtr<IMMDevice> audio_device;
  for (int i = 0; i < arraysize(data); ++i) {
    audio_device =
        CoreAudioUtil::CreateDefaultDevice(data[i].flow, data[i].role);
    EXPECT_TRUE(audio_device);
    EXPECT_EQ(data[i].flow, CoreAudioUtil::GetDataFlow(audio_device));
  }

  // Only eRender and eCapture are allowed as flow parameter.
  audio_device = CoreAudioUtil::CreateDefaultDevice(eAll, eConsole);
  EXPECT_FALSE(audio_device);
}

TEST_F(CoreAudioUtilWinTest, CreateDevice) {
  if (!CanRunAudioTest())
    return;

  // Get name and ID of default device used for playback.
  ScopedComPtr<IMMDevice> default_render_device =
      CoreAudioUtil::CreateDefaultDevice(eRender, eConsole);
  AudioDeviceName default_render_name;
  EXPECT_TRUE(SUCCEEDED(CoreAudioUtil::GetDeviceName(default_render_device,
                                                     &default_render_name)));

  // Use the uniqe ID as input to CreateDevice() and create a corresponding
  // IMMDevice.
  ScopedComPtr<IMMDevice> audio_device =
      CoreAudioUtil::CreateDevice(default_render_name.unique_id);
  EXPECT_TRUE(audio_device);

  // Verify that the two IMMDevice interfaces represents the same endpoint
  // by comparing their unique IDs.
  AudioDeviceName device_name;
  EXPECT_TRUE(SUCCEEDED(CoreAudioUtil::GetDeviceName(audio_device,
                                                     &device_name)));
  EXPECT_EQ(default_render_name.unique_id, device_name.unique_id);
}

TEST_F(CoreAudioUtilWinTest, GetDefaultDeviceName) {
  if (!CanRunAudioTest())
    return;

  struct {
    EDataFlow flow;
    ERole role;
  } data[] = {
    {eRender, eConsole},
    {eRender, eCommunications},
    {eCapture, eConsole},
    {eCapture, eCommunications}
  };

  // Get name and ID of default devices for all flow/role combinations above.
  ScopedComPtr<IMMDevice> audio_device;
  AudioDeviceName device_name;
  for (int i = 0; i < arraysize(data); ++i) {
    audio_device =
        CoreAudioUtil::CreateDefaultDevice(data[i].flow, data[i].role);
    EXPECT_TRUE(SUCCEEDED(CoreAudioUtil::GetDeviceName(audio_device,
                                                       &device_name)));
    EXPECT_FALSE(device_name.device_name.empty());
    EXPECT_FALSE(device_name.unique_id.empty());
  }
}

TEST_F(CoreAudioUtilWinTest, GetFriendlyName) {
  if (!CanRunAudioTest())
    return;

  // Get name and ID of default device used for recording.
  ScopedComPtr<IMMDevice> audio_device =
      CoreAudioUtil::CreateDefaultDevice(eCapture, eConsole);
  AudioDeviceName device_name;
  HRESULT hr = CoreAudioUtil::GetDeviceName(audio_device, &device_name);
  EXPECT_TRUE(SUCCEEDED(hr));

  // Use unique ID as input to GetFriendlyName() and compare the result
  // with the already obtained friendly name for the default capture device.
  std::string friendly_name = CoreAudioUtil::GetFriendlyName(
      device_name.unique_id);
  EXPECT_EQ(friendly_name, device_name.device_name);

  // Same test as above but for playback.
  audio_device = CoreAudioUtil::CreateDefaultDevice(eRender, eConsole);
  hr = CoreAudioUtil::GetDeviceName(audio_device, &device_name);
  EXPECT_TRUE(SUCCEEDED(hr));
  friendly_name = CoreAudioUtil::GetFriendlyName(device_name.unique_id);
  EXPECT_EQ(friendly_name, device_name.device_name);
}

TEST_F(CoreAudioUtilWinTest, DeviceIsDefault) {
  if (!CanRunAudioTest())
    return;

  // Verify that the default render device is correctly identified as a
  // default device.
  ScopedComPtr<IMMDevice> audio_device =
      CoreAudioUtil::CreateDefaultDevice(eRender, eConsole);
  AudioDeviceName name;
  EXPECT_TRUE(SUCCEEDED(CoreAudioUtil::GetDeviceName(audio_device, &name)));
  const std::string id = name.unique_id;
  EXPECT_TRUE(CoreAudioUtil::DeviceIsDefault(eRender, eConsole, id));
  EXPECT_FALSE(CoreAudioUtil::DeviceIsDefault(eCapture, eConsole, id));
}

TEST_F(CoreAudioUtilWinTest, CreateClient) {
  if (!CanRunAudioTest())
    return;

  EDataFlow data[] = {eRender, eCapture};
  ScopedComPtr<IMMDevice> device;

  for (int i = 0; i < arraysize(data); ++i) {
    device = CoreAudioUtil::CreateDefaultDevice(data[i], eConsole);
    EXPECT_TRUE(device);
    EXPECT_EQ(data[i], CoreAudioUtil::GetDataFlow(device));
  }
}

}  // namespace media
