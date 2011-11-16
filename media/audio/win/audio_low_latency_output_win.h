// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
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
//
// Core Audio API details:
//
// - CoInitializeEx() is called on the creating thread and on the internal
//   capture thread. Each thread's concurrency model and apartment is set
//   to multi-threaded (MTA). CHECK() is called to ensure that we crash if
//   CoInitializeEx(MTA) fails.
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
#ifndef MEDIA_AUDIO_WIN_AUDIO_LOW_LATENCY_OUTPUT_WIN_H_
#define MEDIA_AUDIO_WIN_AUDIO_LOW_LATENCY_OUTPUT_WIN_H_

#include <Audioclient.h>
#include <MMDeviceAPI.h>

#include "base/compiler_specific.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_handle.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_parameters.h"
#include "media/base/media_export.h"

class AudioManagerWin;

// AudioOutputStream implementation using Windows Core Audio APIs.
class MEDIA_EXPORT WASAPIAudioOutputStream
    : public AudioOutputStream,
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
  virtual bool Open() OVERRIDE;
  virtual void Start(AudioSourceCallback* callback) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void SetVolume(double volume) OVERRIDE;
  virtual void GetVolume(double* volume) OVERRIDE;

  // Retrieves the stream format that the audio engine uses for its internal
  // processing/mixing of shared-mode streams.
  static double HardwareSampleRate(ERole device_role);

  bool started() const { return started_; }

 private:
  // DelegateSimpleThread::Delegate implementation.
  virtual void Run() OVERRIDE;

  // Issues the OnError() callback to the |sink_|.
  void HandleError(HRESULT err);

  // The Open() method is divided into these sub methods.
  HRESULT SetRenderDevice(ERole device_role);
  HRESULT ActivateRenderDevice();
  HRESULT GetAudioEngineStreamFormat();
  bool DesiredFormatIsSupported();
  HRESULT InitializeAudioEngine();

  // Initializes the COM library for use by the calling thread and sets the
  // thread's concurrency model to multi-threaded.
  base::win::ScopedCOMInitializer com_init_;

  // Contains the thread ID of the creating thread.
  base::PlatformThreadId creating_thread_id_;

  // Our creator, the audio manager needs to be notified when we close.
  AudioManagerWin* manager_;

  // Rendering is driven by this thread (which has no message loop).
  // All OnMoreData() callbacks will be called from this thread.
  base::DelegateSimpleThread* render_thread_;

  // Contains the desired audio format which is set up at construction.
  WAVEFORMATEX format_;

  // Copy of the audio format which we know the audio engine supports.
  // It is recommended to ensure that the sample rate in |format_| is identical
  // to the sample rate in |audio_engine_mix_format_|.
  base::win::ScopedCoMem<WAVEFORMATEX> audio_engine_mix_format_;

  bool opened_;
  bool started_;

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

  // Counts the number of audio frames written to the endpoint buffer.
  UINT64 num_written_frames_;

  // Pointer to the client that will deliver audio samples to be played out.
  AudioSourceCallback* source_;

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

  DISALLOW_COPY_AND_ASSIGN(WASAPIAudioOutputStream);
};

#endif  // MEDIA_AUDIO_WIN_AUDIO_LOW_LATENCY_OUTPUT_WIN_H_
