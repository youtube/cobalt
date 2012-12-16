// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/audio_device_listener_win.h"

#include <Audioclient.h>

#include "base/logging.h"
#include "base/system_monitor/system_monitor.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/windows_version.h"
#include "media/audio/audio_util.h"
#include "media/audio/win/core_audio_util_win.h"

using base::win::ScopedCoMem;

namespace media {

AudioDeviceListenerWin::AudioDeviceListenerWin(const base::Closure& listener_cb)
    : listener_cb_(listener_cb) {
  CHECK(CoreAudioUtil::IsSupported());

  ScopedComPtr<IMMDeviceEnumerator> device_enumerator(
      CoreAudioUtil::CreateDeviceEnumerator());
  if (!device_enumerator)
    return;

  HRESULT hr = device_enumerator->RegisterEndpointNotificationCallback(this);
  if (FAILED(hr)) {
    DLOG(ERROR)  << "RegisterEndpointNotificationCallback failed: "
                 << std::hex << hr;
    return;
  }

  device_enumerator_ = device_enumerator;

  ScopedComPtr<IMMDevice> device =
      CoreAudioUtil::CreateDefaultDevice(eRender, eConsole);
  if (!device) {
    // Most probable reason for ending up here is that all audio devices are
    // disabled or unplugged.
    DVLOG(1)  << "CoreAudioUtil::CreateDefaultDevice failed. No device?";
    return;
  }

  AudioDeviceName device_name;
  hr = CoreAudioUtil::GetDeviceName(device, &device_name);
  if (FAILED(hr)) {
    DVLOG(1)  << "Failed to retrieve the device id: " << std::hex << hr;
    return;
  }
  default_render_device_id_ = device_name.unique_id;
}

AudioDeviceListenerWin::~AudioDeviceListenerWin() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (device_enumerator_) {
    HRESULT hr =
        device_enumerator_->UnregisterEndpointNotificationCallback(this);
    DLOG_IF(ERROR, FAILED(hr)) << "UnregisterEndpointNotificationCallback() "
                               << "failed: " << std::hex << hr;
  }
}

STDMETHODIMP_(ULONG) AudioDeviceListenerWin::AddRef() {
  return 1;
}

STDMETHODIMP_(ULONG) AudioDeviceListenerWin::Release() {
  return 1;
}

STDMETHODIMP AudioDeviceListenerWin::QueryInterface(REFIID iid, void** object) {
  if (iid == IID_IUnknown || iid == __uuidof(IMMNotificationClient)) {
    *object = static_cast<IMMNotificationClient*>(this);
    return S_OK;
  }

  *object = NULL;
  return E_NOINTERFACE;
}

STDMETHODIMP AudioDeviceListenerWin::OnPropertyValueChanged(
    LPCWSTR device_id, const PROPERTYKEY key) {
  // TODO(dalecurtis): We need to handle changes for the current default device
  // here.  It's tricky because this method may be called many (20+) times for
  // a single change like sample rate.  http://crbug.com/153056
  return S_OK;
}

STDMETHODIMP AudioDeviceListenerWin::OnDeviceAdded(LPCWSTR device_id) {
  // We don't care when devices are added.
  return S_OK;
}

STDMETHODIMP AudioDeviceListenerWin::OnDeviceRemoved(LPCWSTR device_id) {
  // We don't care when devices are removed.
  return S_OK;
}

STDMETHODIMP AudioDeviceListenerWin::OnDeviceStateChanged(LPCWSTR device_id,
                                                          DWORD new_state) {
  if (new_state != DEVICE_STATE_ACTIVE && new_state != DEVICE_STATE_NOTPRESENT)
    return S_OK;

  base::SystemMonitor* monitor = base::SystemMonitor::Get();
  if (monitor)
    monitor->ProcessDevicesChanged(base::SystemMonitor::DEVTYPE_AUDIO_CAPTURE);

  return S_OK;
}

STDMETHODIMP AudioDeviceListenerWin::OnDefaultDeviceChanged(
    EDataFlow flow, ERole role, LPCWSTR new_default_device_id) {
  // Only listen for output device changes right now...
  if (flow != eConsole && role != eRender)
    return S_OK;

  // If no device is now available, |new_default_device_id| will be NULL.
  std::string new_device_id = "";
  if (new_default_device_id)
    new_device_id = WideToUTF8(new_default_device_id);

  // Only fire a state change event if the device has actually changed.
  // TODO(dalecurtis): This still seems to fire an extra event on my machine for
  // an unplug event (probably others too); e.g., we get two transitions to a
  // new default device id.
  if (new_device_id.compare(default_render_device_id_) == 0)
    return S_OK;

  default_render_device_id_ = new_device_id;
  listener_cb_.Run();

  return S_OK;
}

}  // namespace media
