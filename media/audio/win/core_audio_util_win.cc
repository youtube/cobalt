// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/core_audio_util_win.h"

#include <Audioclient.h>
#include <Functiondiscoverykeys_devpkey.h>

#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/windows_version.h"

using base::win::ScopedCoMem;

namespace media {

// Scoped PROPVARIANT class for automatically freeing a COM PROPVARIANT
// structure at the end of a scope.
class ScopedPropertyVariant {
 public:
  ScopedPropertyVariant() {
    PropVariantInit(&propvar_);
  }
  ~ScopedPropertyVariant() {
    PropVariantClear(&propvar_);
  }

  // Retrieves the pointer address.
  // Used to receive a PROPVARIANT as an out argument (and take ownership).
  PROPVARIANT* Receive() {
    DCHECK_EQ(propvar_.vt, VT_EMPTY);
    return &propvar_;
  }

  VARTYPE type() const {
    return propvar_.vt;
  }

  LPWSTR as_wide_string() const {
    DCHECK_EQ(type(), VT_LPWSTR);
    return propvar_.pwszVal;
  }

 private:
  PROPVARIANT propvar_;

  DISALLOW_COPY_AND_ASSIGN(ScopedPropertyVariant);
};

bool CoreAudioUtil::IsSupported() {
  // Microsoft does not plan to make the Core Audio APIs available for use
  // with earlier versions of Windows, including Microsoft Windows Server 2003,
  // Windows XP, Windows Millennium Edition, Windows 2000, and Windows 98.
  return (base::win::GetVersion() >= base::win::VERSION_VISTA);
}

int CoreAudioUtil::NumberOfActiveDevices(EDataFlow data_flow) {
  DCHECK(CoreAudioUtil::IsSupported());
  // Create the IMMDeviceEnumerator interface.
  ScopedComPtr<IMMDeviceEnumerator> device_enumerator =
      CreateDeviceEnumerator();
  if (!device_enumerator)
    return 0;

  // Generate a collection of active (present and not disabled) audio endpoint
  // devices for the specified data-flow direction.
  // This method will succeed even if all devices are disabled.
  ScopedComPtr<IMMDeviceCollection> collection;
  HRESULT hr = device_enumerator->EnumAudioEndpoints(data_flow,
                                                     DEVICE_STATE_ACTIVE,
                                                     collection.Receive());
  if (FAILED(hr)) {
    LOG(ERROR) << "IMMDeviceCollection::EnumAudioEndpoints: " << std::hex << hr;
    return 0;
  }

  // Retrieve the number of active audio devices for the specified direction
  UINT number_of_active_devices = 0;
  collection->GetCount(&number_of_active_devices);
  DVLOG(1) << ((data_flow == eCapture) ? "[in ] " : "[out] ")
           << "number of devices: " << number_of_active_devices;
  return static_cast<int>(number_of_active_devices);
}

ScopedComPtr<IMMDeviceEnumerator> CoreAudioUtil::CreateDeviceEnumerator() {
  DCHECK(CoreAudioUtil::IsSupported());
  ScopedComPtr<IMMDeviceEnumerator> device_enumerator;
  HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator),
                                NULL,
                                CLSCTX_INPROC_SERVER,
                                __uuidof(IMMDeviceEnumerator),
                                device_enumerator.ReceiveVoid());
  // CO_E_NOTINITIALIZED is the most likely reason for failure and if that
  // happens we might as well die here.
  CHECK(SUCCEEDED(hr));
  return device_enumerator;
}

ScopedComPtr<IMMDevice> CoreAudioUtil::CreateDefaultDevice(EDataFlow data_flow,
                                                           ERole role) {
  DCHECK(CoreAudioUtil::IsSupported());
  ScopedComPtr<IMMDevice> endpoint_device;

  // Create the IMMDeviceEnumerator interface.
  ScopedComPtr<IMMDeviceEnumerator> device_enumerator =
      CreateDeviceEnumerator();
  if (!device_enumerator)
    return endpoint_device;

  // Retrieve the default audio endpoint for the specified data-flow
  // direction and role.
  HRESULT hr = device_enumerator->GetDefaultAudioEndpoint(
      data_flow, role, endpoint_device.Receive());

  if (FAILED(hr)) {
    DVLOG(1) << "IMMDeviceEnumerator::GetDefaultAudioEndpoint: "
             << std::hex << hr;
    return endpoint_device;
  }

  // Verify that the audio endpoint device is active, i.e., that the audio
  // adapter that connects to the endpoint device is present and enabled.
  DWORD state = DEVICE_STATE_DISABLED;
  hr = endpoint_device->GetState(&state);
  if (SUCCEEDED(hr)) {
    if (!(state & DEVICE_STATE_ACTIVE)) {
      DVLOG(1) << "Selected endpoint device is not active";
      endpoint_device.Release();
    }
  }
  return endpoint_device;
}

ScopedComPtr<IMMDevice> CoreAudioUtil::CreateDevice(
    const std::string& device_id) {
  DCHECK(CoreAudioUtil::IsSupported());
  ScopedComPtr<IMMDevice> endpoint_device;

  // Create the IMMDeviceEnumerator interface.
  ScopedComPtr<IMMDeviceEnumerator> device_enumerator =
      CreateDeviceEnumerator();
  if (!device_enumerator)
    return endpoint_device;

  // Retrieve an audio device specified by an endpoint device-identification
  // string.
  HRESULT hr = device_enumerator->GetDevice(UTF8ToUTF16(device_id).c_str(),
                                            endpoint_device.Receive());
  DVLOG_IF(1, FAILED(hr)) << "IMMDeviceEnumerator::GetDevice: "
                          << std::hex << hr;
  return endpoint_device;
}

HRESULT CoreAudioUtil::GetDeviceName(IMMDevice* device, AudioDeviceName* name) {
  DCHECK(CoreAudioUtil::IsSupported());
  DCHECK(device);
  AudioDeviceName device_name;

  // Retrieve unique name of endpoint device.
  // Example: "{0.0.1.00000000}.{8db6020f-18e3-4f25-b6f5-7726c9122574}".
  ScopedCoMem<WCHAR> endpoint_device_id;
  HRESULT hr = device->GetId(&endpoint_device_id);
  if (FAILED(hr))
    return hr;
  WideToUTF8(endpoint_device_id, wcslen(endpoint_device_id),
             &device_name.unique_id);

  // Retrieve user-friendly name of endpoint device.
  // Example: "Microphone (Realtek High Definition Audio)".
  ScopedComPtr<IPropertyStore> properties;
  hr = device->OpenPropertyStore(STGM_READ, properties.Receive());
  if (FAILED(hr))
    return hr;
  ScopedPropertyVariant friendly_name;
  hr = properties->GetValue(PKEY_Device_FriendlyName, friendly_name.Receive());
  if (FAILED(hr))
    return hr;
  if (friendly_name.as_wide_string()) {
    WideToUTF8(friendly_name.as_wide_string(),
               wcslen(friendly_name.as_wide_string()),
               &device_name.device_name);
  }

  *name = device_name;
  DVLOG(1) << "friendly name: " << device_name.device_name;
  DVLOG(1) << "unique id    : " << device_name.unique_id;
  return hr;
}

std::string CoreAudioUtil::GetFriendlyName(const std::string& device_id) {
  DCHECK(CoreAudioUtil::IsSupported());

  ScopedComPtr<IMMDevice> audio_device = CreateDevice(device_id);
  if (!audio_device)
    return std::string();

  AudioDeviceName device_name;
  HRESULT hr = GetDeviceName(audio_device, &device_name);
  if (FAILED(hr))
    return std::string();

  return device_name.device_name;
}

bool CoreAudioUtil::DeviceIsDefault(EDataFlow flow,
                                    ERole role,
                                    std::string device_id) {
  DCHECK(CoreAudioUtil::IsSupported());
  ScopedComPtr<IMMDevice> device = CreateDefaultDevice(flow, role);
  if (!device)
    return false;

  ScopedCoMem<WCHAR> default_device_id;
  HRESULT hr = device->GetId(&default_device_id);
  if (FAILED(hr))
    return false;

  std::string str_default;
  WideToUTF8(default_device_id, wcslen(default_device_id), &str_default);
  if (device_id.compare(str_default) != 0)
    return false;
  return true;
}

EDataFlow CoreAudioUtil::GetDataFlow(IMMDevice* device) {
  DCHECK(CoreAudioUtil::IsSupported());
  DCHECK(device);

  ScopedComPtr<IMMEndpoint> endpoint;
  HRESULT hr = device->QueryInterface(endpoint.Receive());
  if (FAILED(hr)) {
    DVLOG(1) << "IMMDevice::QueryInterface: " << std::hex << hr;
    return eAll;
  }

  EDataFlow data_flow;
  hr = endpoint->GetDataFlow(&data_flow);
  if (FAILED(hr)) {
    DVLOG(1) << "IMMEndpoint::GetDataFlow: " << std::hex << hr;
    return eAll;
  }
  return data_flow;
}

ScopedComPtr<IAudioClient> CoreAudioUtil::CreateClient(
    IMMDevice* audio_device) {
  DCHECK(CoreAudioUtil::IsSupported());
  ScopedComPtr<IAudioClient> audio_client;

  // Creates and activates an IAudioClient COM object given the selected
  // endpoint device.
  HRESULT hr = audio_device->Activate(__uuidof(IAudioClient),
                                      CLSCTX_INPROC_SERVER,
                                      NULL,
                                      audio_client.ReceiveVoid());
  DVLOG_IF(1, FAILED(hr)) << "IMMDevice::Activate: " << std::hex << hr;
  return audio_client;
}

}  // namespace media
