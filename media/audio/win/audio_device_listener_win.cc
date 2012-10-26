// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/audio_device_listener_win.h"

#include <Audioclient.h>

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/windows_version.h"
#include "media/audio/audio_util.h"

using base::win::ScopedCoMem;

namespace media {

// TODO(henrika): Move to CoreAudioUtil class.
static ScopedComPtr<IMMDeviceEnumerator> CreateDeviceEnumerator() {
  ScopedComPtr<IMMDeviceEnumerator> device_enumerator;
  HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator),
                                NULL,
                                CLSCTX_INPROC_SERVER,
                                __uuidof(IMMDeviceEnumerator),
                                device_enumerator.ReceiveVoid());
  DLOG_IF(ERROR, FAILED(hr)) << "CoCreateInstance(IMMDeviceEnumerator): "
                             << std::hex << hr;
  return device_enumerator;
}

AudioDeviceListenerWin::AudioDeviceListenerWin(const base::Closure& listener_cb)
    : listener_cb_(listener_cb) {
  CHECK(media::IsWASAPISupported());

  device_enumerator_ = CreateDeviceEnumerator();
  if (!device_enumerator_)
  return;

  HRESULT hr = device_enumerator_->RegisterEndpointNotificationCallback(this);
  if (FAILED(hr)) {
    DLOG(ERROR)  << "RegisterEndpointNotificationCallback failed: "
                 << std::hex << hr;
    device_enumerator_ = NULL;
    return;
  }

  ScopedComPtr<IMMDevice> endpoint_render_device;
  hr = device_enumerator_->GetDefaultAudioEndpoint(
      eRender, eConsole, endpoint_render_device.Receive());
  // This will fail if there are no audio devices currently plugged in, so we
  // still want to keep our endpoint registered.
  if (FAILED(hr)) {
    DVLOG(1) << "GetDefaultAudioEndpoint() failed.  No devices?  Error: "
             << std::hex << hr;
    return;
  }

  ScopedCoMem<WCHAR> render_device_id;
  hr = endpoint_render_device->GetId(&render_device_id);
  if (FAILED(hr)) {
    DLOG(ERROR) << "GetId() failed: " << std::hex << hr;
    return;
  }

  default_render_device_id_ = WideToUTF8(static_cast<WCHAR*>(render_device_id));
  DVLOG(1) << "Default render device: " << default_render_device_id_;
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
