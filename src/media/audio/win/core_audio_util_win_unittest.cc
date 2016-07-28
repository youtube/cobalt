// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_handle.h"
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

TEST_F(CoreAudioUtilWinTest, CreateDefaultClient) {
  if (!CanRunAudioTest())
    return;

  EDataFlow data[] = {eRender, eCapture};

  for (int i = 0; i < arraysize(data); ++i) {
    ScopedComPtr<IAudioClient> client;
    client = CoreAudioUtil::CreateDefaultClient(data[i], eConsole);
    EXPECT_TRUE(client);
  }
}

TEST_F(CoreAudioUtilWinTest, CreateClient) {
  if (!CanRunAudioTest())
    return;

  EDataFlow data[] = {eRender, eCapture};

  for (int i = 0; i < arraysize(data); ++i) {
    ScopedComPtr<IMMDevice> device;
    ScopedComPtr<IAudioClient> client;
    device = CoreAudioUtil::CreateDefaultDevice(data[i], eConsole);
    EXPECT_TRUE(device);
    EXPECT_EQ(data[i], CoreAudioUtil::GetDataFlow(device));
    client = CoreAudioUtil::CreateClient(device);
    EXPECT_TRUE(client);
  }
}

TEST_F(CoreAudioUtilWinTest, GetSharedModeMixFormat) {
  if (!CanRunAudioTest())
    return;

  ScopedComPtr<IMMDevice> device;
  ScopedComPtr<IAudioClient> client;
  device = CoreAudioUtil::CreateDefaultDevice(eRender, eConsole);
  EXPECT_TRUE(device);
  client = CoreAudioUtil::CreateClient(device);
  EXPECT_TRUE(client);

  // Perform a simple sanity test of the aquired format structure.
  WAVEFORMATPCMEX format;
  EXPECT_TRUE(SUCCEEDED(CoreAudioUtil::GetSharedModeMixFormat(client,
                                                              &format)));
  EXPECT_GE(format.Format.nChannels, 1);
  EXPECT_GE(format.Format.nSamplesPerSec, 8000u);
  EXPECT_GE(format.Format.wBitsPerSample, 16);
  EXPECT_GE(format.Samples.wValidBitsPerSample, 16);
  EXPECT_EQ(format.Format.wFormatTag, WAVE_FORMAT_EXTENSIBLE);
}

TEST_F(CoreAudioUtilWinTest, GetDevicePeriod) {
  if (!CanRunAudioTest())
    return;

  EDataFlow data[] = {eRender, eCapture};

  // Verify that the device periods are valid for the default render and
  // capture devices.
  for (int i = 0; i < arraysize(data); ++i) {
    ScopedComPtr<IAudioClient> client;
    REFERENCE_TIME shared_time_period = 0;
    REFERENCE_TIME exclusive_time_period = 0;
    client = CoreAudioUtil::CreateDefaultClient(data[i], eConsole);
    EXPECT_TRUE(client);
    EXPECT_TRUE(SUCCEEDED(CoreAudioUtil::GetDevicePeriod(
        client, AUDCLNT_SHAREMODE_SHARED, &shared_time_period)));
    EXPECT_GT(shared_time_period, 0);
    EXPECT_TRUE(SUCCEEDED(CoreAudioUtil::GetDevicePeriod(
        client, AUDCLNT_SHAREMODE_EXCLUSIVE, &exclusive_time_period)));
    EXPECT_GT(exclusive_time_period, 0);
    EXPECT_LE(exclusive_time_period, shared_time_period);
  }
}

TEST_F(CoreAudioUtilWinTest, GetPreferredAudioParameters) {
  if (!CanRunAudioTest())
    return;

  EDataFlow data[] = {eRender, eCapture};

  // Verify that the preferred audio parameters are OK for the default render
  // and capture devices.
  for (int i = 0; i < arraysize(data); ++i) {
    ScopedComPtr<IAudioClient> client;
    AudioParameters params;
    client = CoreAudioUtil::CreateDefaultClient(data[i], eConsole);
    EXPECT_TRUE(client);
    EXPECT_TRUE(SUCCEEDED(CoreAudioUtil::GetPreferredAudioParameters(client,
                                                                     &params)));
    EXPECT_TRUE(params.IsValid());
  }
}

TEST_F(CoreAudioUtilWinTest, SharedModeInitialize) {
  if (!CanRunAudioTest())
    return;

  ScopedComPtr<IAudioClient> client;
  client = CoreAudioUtil::CreateDefaultClient(eRender, eConsole);
  EXPECT_TRUE(client);

  WAVEFORMATPCMEX format;
  EXPECT_TRUE(SUCCEEDED(CoreAudioUtil::GetSharedModeMixFormat(client,
                                                              &format)));

  // Perform a shared-mode initialization without event-driven buffer handling.
  size_t endpoint_buffer_size = 0;
  HRESULT hr = CoreAudioUtil::SharedModeInitialize(client, &format, NULL,
                                                   &endpoint_buffer_size);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_GT(endpoint_buffer_size, 0u);

  // It is only possible to create a client once.
  hr = CoreAudioUtil::SharedModeInitialize(client, &format, NULL,
                                           &endpoint_buffer_size);
  EXPECT_FALSE(SUCCEEDED(hr));
  EXPECT_EQ(hr, AUDCLNT_E_ALREADY_INITIALIZED);

  // Verify that it is possible to reinitialize the client after releasing it.
  client = CoreAudioUtil::CreateDefaultClient(eRender, eConsole);
  EXPECT_TRUE(client);
  hr = CoreAudioUtil::SharedModeInitialize(client, &format, NULL,
                                           &endpoint_buffer_size);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_GT(endpoint_buffer_size, 0u);

  // Use a non-supported format and verify that initialization fails.
  // A simple way to emulate an invalid format is to use the shared-mode
  // mixing format and modify the preferred sample.
  client = CoreAudioUtil::CreateDefaultClient(eRender, eConsole);
  EXPECT_TRUE(client);
  format.Format.nSamplesPerSec = format.Format.nSamplesPerSec + 1;
  EXPECT_FALSE(CoreAudioUtil::IsFormatSupported(
                  client, AUDCLNT_SHAREMODE_SHARED, &format));
  hr = CoreAudioUtil::SharedModeInitialize(client, &format, NULL,
                                           &endpoint_buffer_size);
  EXPECT_TRUE(FAILED(hr));
  EXPECT_EQ(hr, E_INVALIDARG);

  // Finally, perform a shared-mode initialization using event-driven buffer
  // handling. The event handle will be signaled when an audio buffer is ready
  // to be processed by the client (not verified here).
  // The event handle should be in the nonsignaled state.
  base::win::ScopedHandle event_handle(::CreateEvent(NULL, TRUE, FALSE, NULL));
  client = CoreAudioUtil::CreateDefaultClient(eRender, eConsole);
  EXPECT_TRUE(client);
  EXPECT_TRUE(SUCCEEDED(CoreAudioUtil::GetSharedModeMixFormat(client,
                                                              &format)));
  EXPECT_TRUE(CoreAudioUtil::IsFormatSupported(
                  client, AUDCLNT_SHAREMODE_SHARED, &format));
  hr = CoreAudioUtil::SharedModeInitialize(client, &format, event_handle.Get(),
                                           &endpoint_buffer_size);
  EXPECT_TRUE(SUCCEEDED(hr));
  EXPECT_GT(endpoint_buffer_size, 0u);
}

TEST_F(CoreAudioUtilWinTest, CreateRenderAndCaptureClients) {
  if (!CanRunAudioTest())
    return;

  EDataFlow data[] = {eRender, eCapture};

  WAVEFORMATPCMEX format;
  size_t endpoint_buffer_size = 0;

  for (int i = 0; i < arraysize(data); ++i) {
    ScopedComPtr<IAudioClient> client;
    ScopedComPtr<IAudioRenderClient> render_client;
    ScopedComPtr<IAudioCaptureClient> capture_client;

    client = CoreAudioUtil::CreateDefaultClient(data[i], eConsole);
    EXPECT_TRUE(client);
    EXPECT_TRUE(SUCCEEDED(CoreAudioUtil::GetSharedModeMixFormat(client,
                                                                &format)));
    if (data[i] == eRender) {
      // It is not possible to create a render client using an unitialized
      // client interface.
      render_client = CoreAudioUtil::CreateRenderClient(client);
      EXPECT_FALSE(render_client);

      // Do a proper initialization and verify that it works this time.
      CoreAudioUtil::SharedModeInitialize(client, &format, NULL,
                                          &endpoint_buffer_size);
      render_client = CoreAudioUtil::CreateRenderClient(client);
      EXPECT_TRUE(render_client);
      EXPECT_GT(endpoint_buffer_size, 0u);
    } else if (data[i] == eCapture) {
      // It is not possible to create a capture client using an unitialized
      // client interface.
      capture_client = CoreAudioUtil::CreateCaptureClient(client);
      EXPECT_FALSE(capture_client);

      // Do a proper initialization and verify that it works this time.
      CoreAudioUtil::SharedModeInitialize(client, &format, NULL,
                                          &endpoint_buffer_size);
      capture_client = CoreAudioUtil::CreateCaptureClient(client);
      EXPECT_TRUE(capture_client);
      EXPECT_GT(endpoint_buffer_size, 0u);
    }
  }
}

//

}  // namespace media
