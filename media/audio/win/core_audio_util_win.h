// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Utility methods for the Core Audio API on Windows.
// Always ensure that Core Audio is supported before using these methods.
// Use media::CoreAudioIsSupported() for this purpose.

#ifndef MEDIA_AUDIO_WIN_CORE_AUDIO_UTIL_WIN_H_
#define MEDIA_AUDIO_WIN_CORE_AUDIO_UTIL_WIN_H_

#include <audioclient.h>
#include <mmdeviceapi.h>
#include <string>

#include "base/basictypes.h"
#include "base/win/scoped_comptr.h"
#include "media/audio/audio_device_name.h"
#include "media/base/media_export.h"

using base::win::ScopedComPtr;

namespace media {

class MEDIA_EXPORT CoreAudioUtil {
 public:
  // Returns true if Windows Core Audio is supported.
  static bool IsSupported();

  // The Windows Multimedia Device (MMDevice) API enables audio clients to
  // discover audio endpoint devices and determine their capabilities.

  // Number of active audio devices in the specified flow data flow direction.
  // Set |data_flow| to eAll to retrive the total number of active audio
  // devices.
  static int NumberOfActiveDevices(EDataFlow data_flow);

  // Creates an IMMDeviceEnumerator interface which provides methods for
  // enumerating audio endpoint devices.
  static ScopedComPtr<IMMDeviceEnumerator> CreateDeviceEnumerator();

  // Create a default endpoint device that is specified by a data-flow
  // direction and role, e.g. default render device.
  static ScopedComPtr<IMMDevice> CreateDefaultDevice(
      EDataFlow data_flow, ERole role);

  // Create an endpoint device that is specified by a unique endpoint device-
  // identification string.
  static ScopedComPtr<IMMDevice> CreateDevice(const std::string& device_id);

  // Returns the unique ID and user-friendly name of a given endpoint device.
  // Example: "{0.0.1.00000000}.{8db6020f-18e3-4f25-b6f5-7726c9122574}", and
  //          "Microphone (Realtek High Definition Audio)".
  static HRESULT GetDeviceName(IMMDevice* device, AudioDeviceName* name);

  // Gets the user-friendly name of the endpoint devcice which is represented
  // by a uniqe id in |device_id|.
  static std::string GetFriendlyName(const std::string& device_id);

  // Returns true if the provided uniqe |device_id| correspinds to the current
  // default device for the specified by a data-flow direction and role.
  static bool DeviceIsDefault(
      EDataFlow flow, ERole role, std::string device_id);

  // Query if the audio device is a rendering device or a capture device.
  static EDataFlow GetDataFlow(IMMDevice* device);

  // The Windows Audio Session API (WASAPI) enables client applications to
  // manage the flow of audio data between the application and an audio endpoint
  // device.

  // Create an IAudioClient interface for an existing IMMDevice given by
  // |audio_device|. Flow direction and role is define by the |audio_device|.
  // The IAudioClient interface enables a client to create and initialize an
  // audio stream between an audio application and the audio engine (for a
  // shared-mode stream) or the hardware buffer of an audio endpoint device
  // (for an exclusive-mode stream).
  static ScopedComPtr<IAudioClient> CreateClient(IMMDevice* audio_device);

 private:
  CoreAudioUtil() {}
  ~CoreAudioUtil() {}
  DISALLOW_COPY_AND_ASSIGN(CoreAudioUtil);
};

}  // namespace media

#endif  // MEDIA_AUDIO_WIN_CORE_AUDIO_UTIL_WIN_H_
