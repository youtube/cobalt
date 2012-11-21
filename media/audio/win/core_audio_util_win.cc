// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/core_audio_util_win.h"

#include <Audioclient.h>
#include <Functiondiscoverykeys_devpkey.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "media/base/media_switches.h"

using base::win::ScopedCoMem;
using base::win::ScopedHandle;

namespace media {

typedef uint32 ChannelConfig;

// Converts Microsoft's channel configuration to ChannelLayout.
// This mapping is not perfect but the best we can do given the current
// ChannelLayout enumerator and the Windows-specific speaker configurations
// defined in ksmedia.h. Don't assume that the channel ordering in
// ChannelLayout is exactly the same as the Windows specific configuration.
// As an example: KSAUDIO_SPEAKER_7POINT1_SURROUND is mapped to
// CHANNEL_LAYOUT_7_1 but the positions of Back L, Back R and Side L, Side R
// speakers are different in these two definitions.
static ChannelLayout ChannelConfigToChannelLayout(ChannelConfig config) {
  switch (config) {
    case KSAUDIO_SPEAKER_DIRECTOUT:
      DVLOG(2) << "KSAUDIO_SPEAKER_DIRECTOUT=>CHANNEL_LAYOUT_NONE";
      return CHANNEL_LAYOUT_NONE;
    case KSAUDIO_SPEAKER_MONO:
      DVLOG(2) << "KSAUDIO_SPEAKER_MONO=>CHANNEL_LAYOUT_MONO";
      return CHANNEL_LAYOUT_MONO;
    case KSAUDIO_SPEAKER_STEREO:
      DVLOG(2) << "KSAUDIO_SPEAKER_STEREO=>CHANNEL_LAYOUT_STEREO";
      return CHANNEL_LAYOUT_STEREO;
    case KSAUDIO_SPEAKER_QUAD:
      DVLOG(2) << "KSAUDIO_SPEAKER_QUAD=>CHANNEL_LAYOUT_QUAD";
      return CHANNEL_LAYOUT_QUAD;
    case KSAUDIO_SPEAKER_SURROUND:
      DVLOG(2) << "KSAUDIO_SPEAKER_SURROUND=>CHANNEL_LAYOUT_4_0";
      return CHANNEL_LAYOUT_4_0;
    case KSAUDIO_SPEAKER_5POINT1:
      DVLOG(2) << "KSAUDIO_SPEAKER_5POINT1=>CHANNEL_LAYOUT_5_1_BACK";
      return CHANNEL_LAYOUT_5_1_BACK;
    case KSAUDIO_SPEAKER_5POINT1_SURROUND:
      DVLOG(2) << "KSAUDIO_SPEAKER_5POINT1_SURROUND=>CHANNEL_LAYOUT_5_1";
      return CHANNEL_LAYOUT_5_1;
    case KSAUDIO_SPEAKER_7POINT1:
      DVLOG(2) << "KSAUDIO_SPEAKER_7POINT1=>CHANNEL_LAYOUT_7_1_WIDE";
      return CHANNEL_LAYOUT_7_1_WIDE;
    case KSAUDIO_SPEAKER_7POINT1_SURROUND:
      DVLOG(2) << "KSAUDIO_SPEAKER_7POINT1_SURROUND=>CHANNEL_LAYOUT_7_1";
      return CHANNEL_LAYOUT_7_1;
    default:
      DVLOG(2) << "Unsupported channel layout: " << config;
      return CHANNEL_LAYOUT_UNSUPPORTED;
  }
}

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

base::TimeDelta CoreAudioUtil::RefererenceTimeToTimeDelta(REFERENCE_TIME time) {
  // Each unit of reference time is 100 nanoseconds <=> 0.1 microsecond.
  return base::TimeDelta::FromMicroseconds(0.1 * time + 0.5);
}

AUDCLNT_SHAREMODE CoreAudioUtil::GetShareMode() {
  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kEnableExclusiveAudio))
    return AUDCLNT_SHAREMODE_EXCLUSIVE;
  return AUDCLNT_SHAREMODE_SHARED;
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
  DVLOG(2) << ((data_flow == eCapture) ? "[in ] " : "[out] ")
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

  // Retrieve unique name of endpoint device.
  // Example: "{0.0.1.00000000}.{8db6020f-18e3-4f25-b6f5-7726c9122574}".
  AudioDeviceName device_name;
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
  DVLOG(2) << "friendly name: " << device_name.device_name;
  DVLOG(2) << "unique id    : " << device_name.unique_id;
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

  // Creates and activates an IAudioClient COM object given the selected
  // endpoint device.
  ScopedComPtr<IAudioClient> audio_client;
  HRESULT hr = audio_device->Activate(__uuidof(IAudioClient),
                                      CLSCTX_INPROC_SERVER,
                                      NULL,
                                      audio_client.ReceiveVoid());
  DVLOG_IF(1, FAILED(hr)) << "IMMDevice::Activate: " << std::hex << hr;
  return audio_client;
}

ScopedComPtr<IAudioClient> CoreAudioUtil::CreateDefaultClient(
    EDataFlow data_flow, ERole role) {
  DCHECK(CoreAudioUtil::IsSupported());
  ScopedComPtr<IMMDevice> default_device(CreateDefaultDevice(data_flow, role));
  return (default_device ? CreateClient(default_device) :
      ScopedComPtr<IAudioClient>());
}

HRESULT CoreAudioUtil::GetSharedModeMixFormat(
    IAudioClient* client, WAVEFORMATPCMEX* format) {
  DCHECK(CoreAudioUtil::IsSupported());
  ScopedCoMem<WAVEFORMATPCMEX> format_pcmex;
  HRESULT hr = client->GetMixFormat(
      reinterpret_cast<WAVEFORMATEX**>(&format_pcmex));
  if (FAILED(hr))
    return hr;

  size_t bytes = sizeof(WAVEFORMATEX) + format_pcmex->Format.cbSize;
  DCHECK_EQ(bytes, sizeof(WAVEFORMATPCMEX));

  memcpy(format, format_pcmex, bytes);

  DVLOG(2) << "wFormatTag: 0x" << std::hex << format->Format.wFormatTag
           << ", nChannels: " << std::dec << format->Format.nChannels
           << ", nSamplesPerSec: " << format->Format.nSamplesPerSec
           << ", nAvgBytesPerSec: " << format->Format.nAvgBytesPerSec
           << ", nBlockAlign: " << format->Format.nBlockAlign
           << ", wBitsPerSample: " << format->Format.wBitsPerSample
           << ", cbSize: " << format->Format.cbSize
           << ", wValidBitsPerSample: " << format->Samples.wValidBitsPerSample
           << ", dwChannelMask: 0x" << std::hex << format->dwChannelMask;

  return hr;
}

bool CoreAudioUtil::IsFormatSupported(IAudioClient* client,
                                      AUDCLNT_SHAREMODE share_mode,
                                      const WAVEFORMATPCMEX* format) {
  DCHECK(CoreAudioUtil::IsSupported());
  ScopedCoMem<WAVEFORMATEXTENSIBLE> closest_match;
  HRESULT hr = client->IsFormatSupported(
      share_mode, reinterpret_cast<const WAVEFORMATEX*>(format),
      reinterpret_cast<WAVEFORMATEX**>(&closest_match));

  // This log can only be triggered for shared mode.
  DLOG_IF(ERROR, hr == S_FALSE) << "Format is not supported "
                                << "but a closest match exists.";
  // This log can be triggered both for shared and exclusive modes.
  DLOG_IF(ERROR, hr == AUDCLNT_E_UNSUPPORTED_FORMAT) << "Unsupported format.";
  if (hr == S_FALSE) {
    DVLOG(2) << "wFormatTag: " << closest_match->Format.wFormatTag
             << ", nChannels: " << closest_match->Format.nChannels
             << ", nSamplesPerSec: " << closest_match->Format.nSamplesPerSec
             << ", wBitsPerSample: " << closest_match->Format.wBitsPerSample;
  }

  return (hr == S_OK);
}

HRESULT CoreAudioUtil::GetDevicePeriod(IAudioClient* client,
                                       AUDCLNT_SHAREMODE share_mode,
                                       REFERENCE_TIME* device_period) {
  DCHECK(CoreAudioUtil::IsSupported());

  // Get the period of the engine thread.
  REFERENCE_TIME default_period = 0;
  REFERENCE_TIME minimum_period = 0;
  HRESULT hr = client->GetDevicePeriod(&default_period, &minimum_period);
  if (FAILED(hr))
    return hr;

  *device_period = (share_mode == AUDCLNT_SHAREMODE_SHARED) ? default_period :
      minimum_period;
  DVLOG(2) << "device_period: "
           << RefererenceTimeToTimeDelta(*device_period).InMillisecondsF()
           << " [ms]";
  return hr;
}

HRESULT CoreAudioUtil::GetPreferredAudioParameters(
    IAudioClient* client, AudioParameters* params) {
  DCHECK(CoreAudioUtil::IsSupported());
  WAVEFORMATPCMEX format;
  HRESULT hr = GetSharedModeMixFormat(client, &format);
  if (FAILED(hr))
    return hr;

  REFERENCE_TIME default_period = 0;
  hr = GetDevicePeriod(client, AUDCLNT_SHAREMODE_SHARED, &default_period);
  if (FAILED(hr))
    return hr;

  // Get the integer mask which corresponds to the channel layout the
  // audio engine uses for its internal processing/mixing of shared-mode
  // streams. This mask indicates which channels are present in the multi-
  // channel stream. The least significant bit corresponds with the Front Left
  // speaker, the next least significant bit corresponds to the Front Right
  // speaker, and so on, continuing in the order defined in KsMedia.h.
  // See http://msdn.microsoft.com/en-us/library/windows/hardware/ff537083.aspx
  // for more details.
  ChannelConfig channel_config = format.dwChannelMask;

  // Convert Microsoft's channel configuration to genric ChannelLayout.
  ChannelLayout channel_layout = ChannelConfigToChannelLayout(channel_config);

  // Store preferred sample rate and buffer size.
  int sample_rate = format.Format.nSamplesPerSec;
  int frames_per_buffer = static_cast<int>(sample_rate *
      RefererenceTimeToTimeDelta(default_period).InSecondsF() + 0.5);

  // TODO(henrika): possibly use format.Format.wBitsPerSample here instead.
  // We use a hard-coded value of 16 bits per sample today even if most audio
  // engines does the actual mixing in 32 bits per sample.
  int bits_per_sample = 16;

  DVLOG(2) << "channel_layout   : " << channel_layout;
  DVLOG(2) << "sample_rate      : " << sample_rate;
  DVLOG(2) << "bits_per_sample  : " << bits_per_sample;
  DVLOG(2) << "frames_per_buffer: " << frames_per_buffer;

  AudioParameters audio_params(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                               channel_layout,
                               sample_rate,
                               bits_per_sample,
                               frames_per_buffer);

  *params = audio_params;
  return hr;
}

HRESULT CoreAudioUtil::GetPreferredAudioParameters(
    EDataFlow data_flow, ERole role, AudioParameters* params) {
  DCHECK(CoreAudioUtil::IsSupported());

  ScopedComPtr<IAudioClient> client = CreateDefaultClient(data_flow, role);
  if (!client) {
    // Map NULL-pointer to new error code which can be different from the
    // actual error code. The exact value is not important here.
    return AUDCLNT_E_ENDPOINT_CREATE_FAILED;
  }
  return GetPreferredAudioParameters(client, params);
}

HRESULT CoreAudioUtil::SharedModeInitialize(IAudioClient* client,
                                            const WAVEFORMATPCMEX* format,
                                            HANDLE event_handle,
                                            size_t* endpoint_buffer_size) {
  DCHECK(CoreAudioUtil::IsSupported());

  DWORD stream_flags = AUDCLNT_STREAMFLAGS_NOPERSIST;

  // Enable event-driven streaming if a valid event handle is provided.
  // After the stream starts, the audio engine will signal the event handle
  // to notify the client each time a buffer becomes ready to process.
  // Event-driven buffering is supported for both rendering and capturing.
  // Both shared-mode and exclusive-mode streams can use event-driven buffering.
  bool use_event = (event_handle != NULL &&
                    event_handle != INVALID_HANDLE_VALUE);
  if (use_event)
    stream_flags |= AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
  DVLOG(2) << "stream_flags: 0x" << std::hex << stream_flags;

  // Initialize the shared mode client for minimal delay.
  HRESULT hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                  stream_flags,
                                  0,
                                  0,
                                  reinterpret_cast<const WAVEFORMATEX*>(format),
                                  NULL);
  if (FAILED(hr)) {
    DVLOG(1) << "IAudioClient::Initialize: " << std::hex << hr;
    return hr;
  }

  if (use_event) {
    hr = client->SetEventHandle(event_handle);
    if (FAILED(hr)) {
      DVLOG(1) << "IAudioClient::SetEventHandle: " << std::hex << hr;
      return hr;
    }
  }

  UINT32 buffer_size_in_frames = 0;
  hr = client->GetBufferSize(&buffer_size_in_frames);
  if (FAILED(hr)) {
    DVLOG(1) << "IAudioClient::GetBufferSize: " << std::hex << hr;
    return hr;
  }

  *endpoint_buffer_size = static_cast<size_t>(buffer_size_in_frames);
  DVLOG(2) << "endpoint buffer size: " << buffer_size_in_frames;

  // TODO(henrika): utilize when delay measurements are added.
  REFERENCE_TIME  latency = 0;
  hr = client->GetStreamLatency(&latency);
  DVLOG(2) << "stream latency: "
           << RefererenceTimeToTimeDelta(latency).InMillisecondsF() << " [ms]";
  return hr;
}

ScopedComPtr<IAudioRenderClient> CoreAudioUtil::CreateRenderClient(
    IAudioClient* client) {
  DCHECK(CoreAudioUtil::IsSupported());

  // Get access to the IAudioRenderClient interface. This interface
  // enables us to write output data to a rendering endpoint buffer.
  ScopedComPtr<IAudioRenderClient> audio_render_client;
  HRESULT hr = client->GetService(__uuidof(IAudioRenderClient),
                                  audio_render_client.ReceiveVoid());
  if (FAILED(hr)) {
    DVLOG(1) << "IAudioClient::GetService: " << std::hex << hr;
    return ScopedComPtr<IAudioRenderClient>();
  }

  // TODO(henrika): verify that this scheme is the same for shared mode and
  // exclusive mode streams.

  // Avoid start-up glitches by filling up the endpoint buffer with "silence"
  // before starting the stream.
  UINT32 endpoint_buffer_size = 0;
  hr = client->GetBufferSize(&endpoint_buffer_size);
  DVLOG_IF(1, FAILED(hr)) << "IAudioClient::GetBufferSize: " << std::hex << hr;

  BYTE* data = NULL;
  hr = audio_render_client->GetBuffer(endpoint_buffer_size, &data);
  DVLOG_IF(1, FAILED(hr)) << "IAudioRenderClient::GetBuffer: "
                          << std::hex << hr;
  if (SUCCEEDED(hr)) {
    // Using the AUDCLNT_BUFFERFLAGS_SILENT flag eliminates the need to
    // explicitly write silence data to the rendering buffer.
    hr = audio_render_client->ReleaseBuffer(endpoint_buffer_size,
                                            AUDCLNT_BUFFERFLAGS_SILENT);
    DVLOG_IF(1, FAILED(hr)) << "IAudioRenderClient::ReleaseBuffer: "
                            << std::hex << hr;
  }

  // Sanity check: verify that the endpoint buffer is filled with silence.
  UINT32 num_queued_frames = 0;
  client->GetCurrentPadding(&num_queued_frames);
  DCHECK(num_queued_frames == endpoint_buffer_size);

  return audio_render_client;
}

ScopedComPtr<IAudioCaptureClient> CoreAudioUtil::CreateCaptureClient(
    IAudioClient* client) {
  DCHECK(CoreAudioUtil::IsSupported());

  // Get access to the IAudioCaptureClient interface. This interface
  // enables us to read input data from a capturing endpoint buffer.
  ScopedComPtr<IAudioCaptureClient> audio_capture_client;
  HRESULT hr = client->GetService(__uuidof(IAudioCaptureClient),
                                  audio_capture_client.ReceiveVoid());
  if (FAILED(hr)) {
    DVLOG(1) << "IAudioClient::GetService: " << std::hex << hr;
    return ScopedComPtr<IAudioCaptureClient>();
  }
  return audio_capture_client;
}

}  // namespace media
