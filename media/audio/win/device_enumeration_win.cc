// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <MMDeviceAPI.h>
#include <mmsystem.h>
#include <Functiondiscoverykeys_devpkey.h>  // MMDeviceAPI.h must come first

#include "media/audio/win/audio_manager_win.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_comptr.h"

using media::AudioDeviceNames;
using base::win::ScopedComPtr;
using base::win::ScopedCoMem;

bool GetInputDeviceNamesWin(AudioDeviceNames* device_names) {
  // It is assumed that this method is called from a COM thread, i.e.,
  // CoInitializeEx() is not called here again to avoid STA/MTA conflicts.
  ScopedComPtr<IMMDeviceEnumerator> enumerator;
  HRESULT hr =  CoCreateInstance(__uuidof(MMDeviceEnumerator),
                                 NULL,
                                 CLSCTX_INPROC_SERVER,
                                 __uuidof(IMMDeviceEnumerator),
                                 enumerator.ReceiveVoid());
  if (FAILED(hr)) {
    LOG(WARNING) << "Failed to create IMMDeviceEnumerator: " << std::hex << hr;
    return false;
  }

  // Generate a collection of active audio capture endpoint devices.
  // This method will succeed even if all devices are disabled.
  ScopedComPtr<IMMDeviceCollection> collection;
  hr = enumerator->EnumAudioEndpoints(eCapture,
                                      DEVICE_STATE_ACTIVE,
                                      collection.Receive());
  if (FAILED(hr))
    return false;

  // Retrieve the number of active capture devices.
  UINT number_of_active_devices = 0;
  collection->GetCount(&number_of_active_devices);
  if (number_of_active_devices == 0)
    return true;

  media::AudioDeviceName device;

  // Loop over all active capture devices and add friendly name and
  // unique ID to the |device_names| list.
  for (UINT i = 0; i < number_of_active_devices; ++i) {
    // Retrieve unique name of endpoint device.
    // Example: "{0.0.1.00000000}.{8db6020f-18e3-4f25-b6f5-7726c9122574}".
    ScopedComPtr<IMMDevice> audio_device;
    ScopedCoMem<WCHAR> endpoint_device_id;
    hr = collection->Item(i, audio_device.Receive());
    if (FAILED(hr))
      continue;
    audio_device->GetId(&endpoint_device_id);

    // Store the unique name.
    device.unique_id = WideToUTF8(static_cast<WCHAR*>(endpoint_device_id));

    // Retrieve user-friendly name of endpoint device.
    // Example: "Microphone (Realtek High Definition Audio)".
    ScopedComPtr<IPropertyStore> properties;
    hr = audio_device->OpenPropertyStore(STGM_READ, properties.Receive());
    if (SUCCEEDED(hr)) {
      PROPVARIANT friendly_name;
      PropVariantInit(&friendly_name);
      hr = properties->GetValue(PKEY_Device_FriendlyName, &friendly_name);

      // Store the user-friendly name.
      if (SUCCEEDED(hr) &&
          friendly_name.vt == VT_LPWSTR && friendly_name.pwszVal) {
        device.device_name = WideToUTF8(friendly_name.pwszVal);
      }
      PropVariantClear(&friendly_name);
    }

    // Add combination of user-friendly and unique name to the output list.
    device_names->push_back(device);
  }

  return true;
}

bool GetInputDeviceNamesWinXP(AudioDeviceNames* device_names) {
  // Retrieve the number of active waveform input devices.
  UINT number_of_active_devices = waveInGetNumDevs();
  if (number_of_active_devices == 0)
    return true;

  media::AudioDeviceName device;
  WAVEINCAPS capabilities;
  MMRESULT err = MMSYSERR_NOERROR;

  // Loop over all active capture devices and add friendly name and
  // unique ID to the |device_names| list. Note that, for Wave on XP,
  // the "unique" name will simply be a copy of the friendly name since
  // there is no safe method to retrieve a unique device name on XP.
  for (UINT i = 0; i < number_of_active_devices; ++i) {
    // Retrieve the capabilities of the specified waveform-audio input device.
    err = waveInGetDevCaps(i,  &capabilities, sizeof(capabilities));
    if (err != MMSYSERR_NOERROR)
      continue;

    if (capabilities.szPname != NULL) {
      // Store the user-friendly name. Max length is MAXPNAMELEN(=32)
      // characters and the name cane be truncated on XP.
      // Example: "Microphone (Realtek High Defini".
      device.device_name = WideToUTF8(capabilities.szPname);

      // Store the "unique" name (we use same as friendly name on Windows XP).
      device.unique_id = WideToUTF8(capabilities.szPname);

      // Add combination of user-friendly and unique name to the output list.
      device_names->push_back(device);
    }
  }

  return true;
}
