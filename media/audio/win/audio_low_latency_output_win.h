// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of AudioOutputStream for Windows using Windows Core Audio
// WASAPI for low latency rendering.
//
// Overview of operation and performance:
//
// - An object of WASAPIAudioOutputStream is created by the AudioManager
//   factory.
// - Next some thread will call Open(), at that point the underlying
//   Core Audio APIs are utilized to create two WASAPI interfaces called
//   IAudioClient and IAudioRenderClient.
// - Then some thread will call Start(source).
//   A thread called "wasapi_render_thread" is started and this thread listens
//   on an event signal which is set periodically by the audio engine to signal
//   render events. As a result, OnMoreData() will be called and the registered
//   client is then expected to provide data samples to be played out.
// - At some point, a thread will call Stop(), which stops and joins the
//   render thread and at the same time stops audio streaming.
// - The same thread that called stop will call Close() where we cleanup
//   and notify the audio manager, which likely will destroy this object.
// - Initial tests on Windows 7 shows that this implementation results in a
//   latency of approximately 35 ms if the selected packet size is less than
//   or equal to 20 ms. Using a packet size of 10 ms does not result in a
//   lower latency but only affects the size of the data buffer in each
//   OnMoreData() callback.
// - A total typical delay of 35 ms contains three parts:
//    o Audio endpoint device period (~10 ms).
//    o Stream latency between the buffer and endpoint device (~5 ms).
//    o Endpoint buffer (~20 ms to ensure glitch-free rendering).
// - Note that, if the user selects a packet size of e.g. 100 ms, the total
//   delay will be approximately 115 ms (10 + 5 + 100).
//
// Implementation notes:
//
// - The minimum supported client is Windows Vista.
// - This implementation is single-threaded, hence:
//    o Construction and destruction must take place from the same thread.
//    o All APIs must be called from the creating thread as well.
// - It is recommended to first acquire the native sample rate of the default
//   input device and then use the same rate when creating this object. Use
//   WASAPIAudioOutputStream::HardwareSampleRate() to retrieve the sample rate.
// - Calling Close() also leads to self destruction.
// - Stream switching is not supported if the user shifts the audio device
//   after Open() is called but before Start() has been called.
// - Stream switching can fail if streaming starts on one device with a
//   supported format (X) and the new default device - to which we would like
//   to switch - uses another format (Y), which is not supported given the
//   configured audio parameters.
// - The audio device must be opened with the same number of channels as it
//   supports natively (see HardwareChannelCount()) otherwise Open() will fail.
// - Support for 8-bit audio has not yet been verified and tested.
//
// Core Audio API details:
//
// - The public API methods (Open(), Start(), Stop() and Close()) must be
//   called on constructing thread. The reason is that we want to ensure that
//   the COM environment is the same for all API implementations.
// - Utilized MMDevice interfaces:
//     o IMMDeviceEnumerator
//     o IMMDevice
// - Utilized WASAPI interfaces:
//     o IAudioClient
//     o IAudioRenderClient
// - The stream is initialized in shared mode and the processing of the
//   audio buffer is event driven.
// - The Multimedia Class Scheduler service (MMCSS) is utilized to boost
//   the priority of the render thread.
// - Audio-rendering endpoint devices can have three roles:
//   Console (eConsole), Communications (eCommunications), and Multimedia
//   (eMultimedia). Search for "Device Roles" on MSDN for more details.
//
// Threading details:
//
// - It is assumed that this class is created on the audio thread owned
//   by the AudioManager.
// - It is a requirement to call the following methods on the same audio
//   thread: Open(), Start(), Stop(), and Close().
// - Audio rendering is performed on the audio render thread, owned by this
//   class, and the AudioSourceCallback::OnMoreData() method will be called
//   from this thread. Stream switching also takes place on the audio-render
//   thread.
//
// Experimental exclusive mode:
//
// - It is possible to open up a stream in exclusive mode by using the
//   --enable-exclusive-audio command line flag.
// - The internal buffering scheme is less flexible for exclusive streams.
//   Hence, some manual tuning will be required before deciding what frame
//   size to use. See the WinAudioOutputTest unit test for more details.
// - If an application opens a stream in exclusive mode, the application has
//   exclusive use of the audio endpoint device that plays the stream.
// - Exclusive-mode should only be utilized when the lowest possible latency
//   is important.
// - In exclusive mode, the client can choose to open the stream in any audio
//   format that the endpoint device supports, i.e. not limited to the device's
//   current (default) configuration.
// - Initial measurements on Windows 7 (HP Z600 workstation) have shown that
//   the lowest possible latencies we can achieve on this machine are:
//     o ~3.3333ms @ 48kHz <=> 160 audio frames per buffer.
//     o ~3.6281ms @ 44.1kHz <=> 160 audio frames per buffer.
// - See http://msdn.microsoft.com/en-us/library/windows/desktop/dd370844(v=vs.85).aspx
//   for more details.

#ifndef MEDIA_AUDIO_WIN_AUDIO_LOW_LATENCY_OUTPUT_WIN_H_
#define MEDIA_AUDIO_WIN_AUDIO_LOW_LATENCY_OUTPUT_WIN_H_

#include <Audioclient.h>
#include <MMDeviceAPI.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_handle.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_parameters.h"
#include "media/base/media_export.h"

namespace media {

class AudioManagerWin;

// AudioOutputStream implementation using Windows Core Audio APIs.
class MEDIA_EXPORT WASAPIAudioOutputStream :
      public AudioOutputStream,
      public base::DelegateSimpleThread::Delegate {
 public:
  // The ctor takes all the usual parameters, plus |manager| which is the
  // the audio manager who is creating this object.
  WASAPIAudioOutputStream(AudioManagerWin* manager,
                          const AudioParameters& params,
                          ERole device_role);

  // The dtor is typically called by the AudioManager only and it is usually
  // triggered by calling AudioOutputStream::Close().
  virtual ~WASAPIAudioOutputStream();

  // Implementation of AudioOutputStream.
  virtual bool Open() override;
  virtual void Start(AudioSourceCallback* callback) override;
  virtual void Stop() override;
  virtual void Close() override;
  virtual void SetVolume(double volume) override;
  virtual void GetVolume(double* volume) override;

  // Retrieves the number of channels the audio engine uses for its internal
  // processing/mixing of shared-mode streams for the default endpoint device.
  static int HardwareChannelCount();

  // Retrieves the channel layout the audio engine uses for its internal
  // processing/mixing of shared-mode streams for the default endpoint device.
  // Note that we convert an internal channel layout mask (see ChannelMask())
  // into a Chrome-specific channel layout enumerator in this method, hence
  // the match might not be perfect.
  static ChannelLayout HardwareChannelLayout();

  // Retrieves the sample rate the audio engine uses for its internal
  // processing/mixing of shared-mode streams for the default endpoint device.
  static int HardwareSampleRate(ERole device_role);

  // Returns AUDCLNT_SHAREMODE_EXCLUSIVE if --enable-exclusive-mode is used
  // as command-line flag and AUDCLNT_SHAREMODE_SHARED otherwise (default).
  static AUDCLNT_SHAREMODE GetShareMode();

  bool started() const { return render_thread_.get() != NULL; }

  // Returns the number of channels the audio engine uses for its internal
  // processing/mixing of shared-mode streams for the default endpoint device.
  int GetEndpointChannelCountForTesting() { return format_.Format.nChannels; }

 private:
  // DelegateSimpleThread::Delegate implementation.
  virtual void Run() override;

  // Issues the OnError() callback to the |sink_|.
  void HandleError(HRESULT err);

  // The Open() method is divided into these sub methods.
  HRESULT SetRenderDevice();
  HRESULT ActivateRenderDevice();
  bool DesiredFormatIsSupported();
  HRESULT InitializeAudioEngine();

  // Called when the device will be opened in shared mode and use the
  // internal audio engine's mix format.
  HRESULT SharedModeInitialization();

  // Called when the device will be opened in exclusive mode and use the
  // application specified format.
  HRESULT ExclusiveModeInitialization();

  // Converts unique endpoint ID to user-friendly device name.
  std::string GetDeviceName(LPCWSTR device_id) const;

  // Contains the thread ID of the creating thread.
  base::PlatformThreadId creating_thread_id_;

  // Our creator, the audio manager needs to be notified when we close.
  AudioManagerWin* manager_;

  // Rendering is driven by this thread (which has no message loop).
  // All OnMoreData() callbacks will be called from this thread.
  scoped_ptr<base::DelegateSimpleThread> render_thread_;

  // Contains the desired audio format which is set up at construction.
  // Extended PCM waveform format structure based on WAVEFORMATEXTENSIBLE.
  // Use this for multiple channel and hi-resolution PCM data.
  WAVEFORMATPCMEX format_;

  // Copy of the audio format which we know the audio engine supports.
  // It is recommended to ensure that the sample rate in |format_| is identical
  // to the sample rate in |audio_engine_mix_format_|.
  base::win::ScopedCoMem<WAVEFORMATPCMEX> audio_engine_mix_format_;

  bool opened_;

  // Set to true as soon as a new default device is detected, and cleared when
  // the streaming has switched from using the old device to the new device.
  // All additional device detections during an active state are ignored to
  // ensure that the ongoing switch can finalize without disruptions.
  bool restart_rendering_mode_;

  // Volume level from 0 to 1.
  float volume_;

  // Size in bytes of each audio frame (4 bytes for 16-bit stereo PCM).
  size_t frame_size_;

  // Size in audio frames of each audio packet where an audio packet
  // is defined as the block of data which the source is expected to deliver
  // in each OnMoreData() callback.
  size_t packet_size_frames_;

  // Size in bytes of each audio packet.
  size_t packet_size_bytes_;

  // Size in milliseconds of each audio packet.
  float packet_size_ms_;

  // Length of the audio endpoint buffer.
  size_t endpoint_buffer_size_frames_;

  // Defines the role that the system has assigned to an audio endpoint device.
  ERole device_role_;

  // The sharing mode for the connection.
  // Valid values are AUDCLNT_SHAREMODE_SHARED and AUDCLNT_SHAREMODE_EXCLUSIVE
  // where AUDCLNT_SHAREMODE_SHARED is the default.
  AUDCLNT_SHAREMODE share_mode_;

  // The channel count set by the client in |params| which is provided to the
  // constructor. The client must feed the AudioSourceCallback::OnMoreData()
  // callback with PCM-data that contains this number of channels.
  int client_channel_count_;

  // Counts the number of audio frames written to the endpoint buffer.
  UINT64 num_written_frames_;

  // Pointer to the client that will deliver audio samples to be played out.
  AudioSourceCallback* source_;

  // An IMMDeviceEnumerator interface which represents a device enumerator.
  base::win::ScopedComPtr<IMMDeviceEnumerator> device_enumerator_;

  // An IMMDevice interface which represents an audio endpoint device.
  base::win::ScopedComPtr<IMMDevice> endpoint_device_;

  // An IAudioClient interface which enables a client to create and initialize
  // an audio stream between an audio application and the audio engine.
  base::win::ScopedComPtr<IAudioClient> audio_client_;

  // The IAudioRenderClient interface enables a client to write output
  // data to a rendering endpoint buffer.
  base::win::ScopedComPtr<IAudioRenderClient> audio_render_client_;

  // The audio engine will signal this event each time a buffer becomes
  // ready to be filled by the client.
  base::win::ScopedHandle audio_samples_render_event_;

  // This event will be signaled when rendering shall stop.
  base::win::ScopedHandle stop_render_event_;

  // Container for retrieving data from AudioSourceCallback::OnMoreData().
  scoped_ptr<AudioBus> audio_bus_;

  DISALLOW_COPY_AND_ASSIGN(WASAPIAudioOutputStream);
};

}  // namespace media

#endif  // MEDIA_AUDIO_WIN_AUDIO_LOW_LATENCY_OUTPUT_WIN_H_
