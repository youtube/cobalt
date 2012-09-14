// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_io.h"

#include <windows.h>
#include <objbase.h>  // This has to be before initguid.h
#include <initguid.h>
#include <mmsystem.h>
#include <setupapi.h>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "media/audio/audio_util.h"
#include "media/audio/win/audio_low_latency_input_win.h"
#include "media/audio/win/audio_low_latency_output_win.h"
#include "media/audio/win/audio_manager_win.h"
#include "media/audio/win/device_enumeration_win.h"
#include "media/audio/win/wavein_input_win.h"
#include "media/audio/win/waveout_output_win.h"
#include "media/base/limits.h"

// Libraries required for the SetupAPI and Wbem APIs used here.
#pragma comment(lib, "setupapi.lib")

// The following are defined in various DDK headers, and we (re)define them
// here to avoid adding the DDK as a chrome dependency.
#define DRV_QUERYDEVICEINTERFACE 0x80c
#define DRVM_MAPPER_PREFERRED_GET 0x2015
#define DRV_QUERYDEVICEINTERFACESIZE 0x80d
DEFINE_GUID(AM_KSCATEGORY_AUDIO, 0x6994ad04, 0x93ef, 0x11d0,
            0xa3, 0xcc, 0x00, 0xa0, 0xc9, 0x22, 0x31, 0x96);

namespace media {

// Maximum number of output streams that can be open simultaneously.
static const int kMaxOutputStreams = 50;

// Up to 8 channels can be passed to the driver.
// This should work, given the right drivers, but graceful error handling is
// needed.
static const int kWinMaxChannels = 8;

// We use 3 buffers for recording audio so that if a recording callback takes
// some time to return we won't lose audio. More buffers while recording are
// ok because they don't introduce any delay in recording, unlike in playback
// where you first need to fill in that number of buffers before starting to
// play.
static const int kNumInputBuffers = 3;

static int GetVersionPartAsInt(DWORDLONG num) {
  return static_cast<int>(num & 0xffff);
}

// Returns a string containing the given device's description and installed
// driver version.
static string16 GetDeviceAndDriverInfo(HDEVINFO device_info,
                                       SP_DEVINFO_DATA* device_data) {
  // Save the old install params setting and set a flag for the
  // SetupDiBuildDriverInfoList below to return only the installed drivers.
  SP_DEVINSTALL_PARAMS old_device_install_params;
  old_device_install_params.cbSize = sizeof(old_device_install_params);
  SetupDiGetDeviceInstallParams(device_info, device_data,
                                &old_device_install_params);
  SP_DEVINSTALL_PARAMS device_install_params = old_device_install_params;
  device_install_params.FlagsEx |= DI_FLAGSEX_INSTALLEDDRIVER;
  SetupDiSetDeviceInstallParams(device_info, device_data,
                                &device_install_params);

  SP_DRVINFO_DATA driver_data;
  driver_data.cbSize = sizeof(driver_data);
  string16 device_and_driver_info;
  if (SetupDiBuildDriverInfoList(device_info, device_data,
                                 SPDIT_COMPATDRIVER)) {
    if (SetupDiEnumDriverInfo(device_info, device_data, SPDIT_COMPATDRIVER, 0,
                              &driver_data)) {
      DWORDLONG version = driver_data.DriverVersion;
      device_and_driver_info = string16(driver_data.Description) + L" v" +
          base::IntToString16(GetVersionPartAsInt((version >> 48))) + L"." +
          base::IntToString16(GetVersionPartAsInt((version >> 32))) + L"." +
          base::IntToString16(GetVersionPartAsInt((version >> 16))) + L"." +
          base::IntToString16(GetVersionPartAsInt(version));
    }
    SetupDiDestroyDriverInfoList(device_info, device_data, SPDIT_COMPATDRIVER);
  }

  SetupDiSetDeviceInstallParams(device_info, device_data,
                                &old_device_install_params);

  return device_and_driver_info;
}

AudioManagerWin::AudioManagerWin() {
  if (!media::IsWASAPISupported()) {
    // Use the Wave API for device enumeration if XP or lower.
    enumeration_type_ = kWaveEnumeration;
  } else {
    // Use the MMDevice API for device enumeration if Vista or higher.
    enumeration_type_ = kMMDeviceEnumeration;
  }

  SetMaxOutputStreamsAllowed(kMaxOutputStreams);
}

AudioManagerWin::~AudioManagerWin() {
  Shutdown();
}

bool AudioManagerWin::HasAudioOutputDevices() {
  return (::waveOutGetNumDevs() != 0);
}

bool AudioManagerWin::HasAudioInputDevices() {
  return (::waveInGetNumDevs() != 0);
}

string16 AudioManagerWin::GetAudioInputDeviceModel() {
  // Get the default audio capture device and its device interface name.
  DWORD device_id = 0;
  waveInMessage(reinterpret_cast<HWAVEIN>(WAVE_MAPPER),
                DRVM_MAPPER_PREFERRED_GET,
                reinterpret_cast<DWORD_PTR>(&device_id), NULL);
  ULONG device_interface_name_size = 0;
  waveInMessage(reinterpret_cast<HWAVEIN>(device_id),
                DRV_QUERYDEVICEINTERFACESIZE,
                reinterpret_cast<DWORD_PTR>(&device_interface_name_size), 0);
  size_t bytes_in_char16 = sizeof(string16::value_type);
  DCHECK_EQ(0u, device_interface_name_size % bytes_in_char16);
  if (device_interface_name_size <= bytes_in_char16)
    return string16();  // No audio capture device.

  string16 device_interface_name;
  string16::value_type* name_ptr = WriteInto(&device_interface_name,
      device_interface_name_size / bytes_in_char16);
  waveInMessage(reinterpret_cast<HWAVEIN>(device_id),
                DRV_QUERYDEVICEINTERFACE,
                reinterpret_cast<DWORD_PTR>(name_ptr),
                static_cast<DWORD_PTR>(device_interface_name_size));

  // Enumerate all audio devices and find the one matching the above device
  // interface name.
  HDEVINFO device_info = SetupDiGetClassDevs(
      &AM_KSCATEGORY_AUDIO, 0, 0, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
  if (device_info == INVALID_HANDLE_VALUE)
    return string16();

  DWORD interface_index = 0;
  SP_DEVICE_INTERFACE_DATA interface_data;
  interface_data.cbSize = sizeof(interface_data);
  while (SetupDiEnumDeviceInterfaces(device_info, 0, &AM_KSCATEGORY_AUDIO,
                                     interface_index++, &interface_data)) {
    // Query the size of the struct, allocate it and then query the data.
    SP_DEVINFO_DATA device_data;
    device_data.cbSize = sizeof(device_data);
    DWORD interface_detail_size = 0;
    SetupDiGetDeviceInterfaceDetail(device_info, &interface_data, 0, 0,
                                    &interface_detail_size, &device_data);
    if (!interface_detail_size)
      continue;

    scoped_array<char> interface_detail_buffer(new char[interface_detail_size]);
    SP_DEVICE_INTERFACE_DETAIL_DATA* interface_detail =
        reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA*>(
            interface_detail_buffer.get());
    interface_detail->cbSize = interface_detail_size;
    if (!SetupDiGetDeviceInterfaceDetail(device_info, &interface_data,
                                         interface_detail,
                                         interface_detail_size, NULL,
                                         &device_data))
      return string16();

    bool device_found = (device_interface_name == interface_detail->DevicePath);

    if (device_found)
      return GetDeviceAndDriverInfo(device_info, &device_data);
  }

  return string16();
}

bool AudioManagerWin::CanShowAudioInputSettings() {
  return true;
}

void AudioManagerWin::ShowAudioInputSettings() {
  std::wstring program;
  std::string argument;
  if (!media::IsWASAPISupported()) {
    program = L"sndvol32.exe";
    argument = "-R";
  } else {
    program = L"control.exe";
    argument = "mmsys.cpl,,1";
  }

  FilePath path;
  PathService::Get(base::DIR_SYSTEM, &path);
  path = path.Append(program);
  CommandLine command_line(path);
  command_line.AppendArg(argument);
  base::LaunchProcess(command_line, base::LaunchOptions(), NULL);
}

void AudioManagerWin::GetAudioInputDeviceNames(
    media::AudioDeviceNames* device_names) {
  DCHECK(enumeration_type() !=  kUninitializedEnumeration);
  // Enumerate all active audio-endpoint capture devices.
  if (enumeration_type() == kWaveEnumeration) {
    // Utilize the Wave API for Windows XP.
    media::GetInputDeviceNamesWinXP(device_names);
  } else {
    // Utilize the MMDevice API (part of Core Audio) for Vista and higher.
    media::GetInputDeviceNamesWin(device_names);
  }

  // Always add default device parameters as first element.
  if (!device_names->empty()) {
    media::AudioDeviceName name;
    name.device_name = AudioManagerBase::kDefaultDeviceName;
    name.unique_id = AudioManagerBase::kDefaultDeviceId;
    device_names->push_front(name);
  }
}

// Factory for the implementations of AudioOutputStream for AUDIO_PCM_LINEAR
// mode.
// - PCMWaveOutAudioOutputStream: Based on the waveOut API.
AudioOutputStream* AudioManagerWin::MakeLinearOutputStream(
      const AudioParameters& params) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  if (params.channels() > kWinMaxChannels)
    return NULL;

  return new PCMWaveOutAudioOutputStream(this,
                                         params,
                                         media::NumberOfWaveOutBuffers(),
                                         WAVE_MAPPER);
}

// Factory for the implementations of AudioOutputStream for
// AUDIO_PCM_LOW_LATENCY mode. Two implementations should suffice most
// windows user's needs.
// - PCMWaveOutAudioOutputStream: Based on the waveOut API.
// - WASAPIAudioOutputStream: Based on Core Audio (WASAPI) API.
AudioOutputStream* AudioManagerWin::MakeLowLatencyOutputStream(
      const AudioParameters& params) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  if (params.channels() > kWinMaxChannels)
    return NULL;

  AudioOutputStream* stream = NULL;
  if (!media::IsWASAPISupported()) {
    // Fall back to Windows Wave implementation on Windows XP or lower.
    DVLOG(1) << "Using WaveOut since WASAPI requires at least Vista.";
    stream = new PCMWaveOutAudioOutputStream(this, params, 2, WAVE_MAPPER);
  } else {
    // TODO(henrika): improve possibility to specify audio endpoint.
    // Use the default device (same as for Wave) for now to be compatible.
    stream = new WASAPIAudioOutputStream(this, params, eConsole);
  }

  return stream;
}

// Factory for the implementations of AudioInputStream for AUDIO_PCM_LINEAR
// mode.
AudioInputStream* AudioManagerWin::MakeLinearInputStream(
    const AudioParameters& params, const std::string& device_id) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LINEAR, params.format());
  return CreatePCMWaveInAudioInputStream(params, device_id);
}

// Factory for the implementations of AudioInputStream for
// AUDIO_PCM_LOW_LATENCY mode.
AudioInputStream* AudioManagerWin::MakeLowLatencyInputStream(
    const AudioParameters& params, const std::string& device_id) {
  DCHECK_EQ(AudioParameters::AUDIO_PCM_LOW_LATENCY, params.format());
  AudioInputStream* stream = NULL;
  if (!media::IsWASAPISupported()) {
    // Fall back to Windows Wave implementation on Windows XP or lower.
    DVLOG(1) << "Using WaveIn since WASAPI requires at least Vista.";
    stream = CreatePCMWaveInAudioInputStream(params, device_id);
  } else {
    stream = new WASAPIAudioInputStream(this, params, device_id);
  }

  return stream;
}

AudioInputStream* AudioManagerWin::CreatePCMWaveInAudioInputStream(
    const AudioParameters& params,
    const std::string& device_id) {
  std::string xp_device_id = device_id;
  if (device_id != AudioManagerBase::kDefaultDeviceId &&
      enumeration_type_ == kMMDeviceEnumeration) {
    xp_device_id = media::ConvertToWinXPDeviceId(device_id);
    if (xp_device_id.empty()) {
      DLOG(ERROR) << "Cannot find a waveIn device which matches the device ID "
                  << device_id;
      return NULL;
    }
  }

  return new PCMWaveInAudioInputStream(this, params, kNumInputBuffers,
                                       xp_device_id);
}

/// static
AudioManager* CreateAudioManager() {
  return new AudioManagerWin();
}

AudioParameters AudioManagerWin::GetPreferredLowLatencyOutputStreamParameters(
    const AudioParameters& input_params) {
  // If WASAPI isn't supported we'll fallback to WaveOut, which will take care
  // of resampling and bits per sample changes.  By setting these equal to the
  // input values, AudioOutputResampler will skip resampling and bit per sample
  // differences (since the input parameters will match the output parameters).
  int sample_rate = input_params.sample_rate();
  int bits_per_sample = input_params.bits_per_sample();
  if (IsWASAPISupported()) {
    sample_rate = GetAudioHardwareSampleRate();
    bits_per_sample = 16;
  }

  // TODO(dalecurtis): This should include bits per channel and channel layout
  // eventually.
  return AudioParameters(
      AudioParameters::AUDIO_PCM_LOW_LATENCY, input_params.channel_layout(),
      sample_rate, bits_per_sample, GetAudioHardwareBufferSize());
}

}  // namespace media
