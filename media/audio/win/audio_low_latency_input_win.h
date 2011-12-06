// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of AudioInputStream for Windows using Windows Core Audio
// WASAPI for low latency capturing.
//
// Overview of operation:
//
// - An object of WASAPIAudioInputStream is created by the AudioManager
//   factory.
// - Next some thread will call Open(), at that point the underlying
//   Core Audio APIs are utilized to create two WASAPI interfaces called
//   IAudioClient and IAudioCaptureClient.
// - Then some thread will call Start(sink).
//   A thread called "wasapi_capture_thread" is started and this thread listens
//   on an event signal which is set periodically by the audio engine for
//   each recorded data packet. As a result, data samples will be provided
//   to the registered sink.
// - At some point, a thread will call Stop(), which stops and joins the
//   capture thread and at the same time stops audio streaming.
// - The same thread that called stop will call Close() where we cleanup
//   and notify the audio manager, which likely will destroy this object.
//
// Implementation notes:
//
// - The minimum supported client is Windows Vista.
// - This implementation is single-threaded, hence:
//    o Construction and destruction must take place from the same thread.
//    o It is recommended to call all APIs from the same thread as well.
// - It is recommended to first acquire the native sample rate of the default
//   input device and then use the same rate when creating this object. Use
//   WASAPIAudioInputStream::HardwareSampleRate() to retrieve the sample rate.
// - Calling Close() also leads to self destruction.
//
// Core Audio API details:
//
// - CoInitializeEx() is called on the creating thread and on the internal
//   capture thread. Each thread's concurrency model and apartment is set
//   to multi-threaded (MTA). CHECK() is called to ensure that we crash if
//   CoInitializeEx(MTA) fails.
// - Utilized MMDevice interfaces:
//     o IMMDeviceEnumerator
//     o IMMDevice
// - Utilized WASAPI interfaces:
//     o IAudioClient
//     o IAudioCaptureClient
// - The stream is initialized in shared mode and the processing of the
//   audio buffer is event driven.
// - The Multimedia Class Scheduler service (MMCSS) is utilized to boost
//   the priority of the capture thread.
//
#ifndef MEDIA_AUDIO_WIN_AUDIO_LOW_LATENCY_INPUT_WIN_H_
#define MEDIA_AUDIO_WIN_AUDIO_LOW_LATENCY_INPUT_WIN_H_

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

// AudioInputStream implementation using Windows Core Audio APIs.
class MEDIA_EXPORT WASAPIAudioInputStream
    : public AudioInputStream,
      public base::DelegateSimpleThread::Delegate {
 public:
  // The ctor takes all the usual parameters, plus |manager| which is the
  // the audio manager who is creating this object.
  WASAPIAudioInputStream(AudioManagerWin* manager,
                         const AudioParameters& params,
                         const std::string& device_id);
  // The dtor is typically called by the AudioManager only and it is usually
  // triggered by calling AudioInputStream::Close().
  virtual ~WASAPIAudioInputStream();

  // Implementation of AudioInputStream.
  virtual bool Open() OVERRIDE;
  virtual void Start(AudioInputCallback* callback) OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual void Close() OVERRIDE;

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
  HRESULT SetCaptureDevice();
  HRESULT ActivateCaptureDevice();
  HRESULT GetAudioEngineStreamFormat();
  bool DesiredFormatIsSupported();
  HRESULT InitializeAudioEngine();

  // Initializes the COM library for use by the calling thread and set the
  // thread's concurrency model to multi-threaded.
  base::win::ScopedCOMInitializer com_init_;

  // Our creator, the audio manager needs to be notified when we close.
  AudioManagerWin* manager_;

  // Capturing is driven by this thread (which has no message loop).
  // All OnData() callbacks will be called from this thread.
  base::DelegateSimpleThread* capture_thread_;

  // Contains the desired audio format which is set up at construction.
  WAVEFORMATEX format_;

  // Copy of the audio format which we know the audio engine supports.
  // It is recommended to ensure that the sample rate in |format_| is identical
  // to the sample rate in |audio_engine_mix_format_|.
  base::win::ScopedCoMem<WAVEFORMATEX> audio_engine_mix_format_;

  bool opened_;
  bool started_;

  // Size in bytes of each audio frame (4 bytes for 16-bit stereo PCM)
  size_t frame_size_;

  // Size in audio frames of each audio packet where an audio packet
  // is defined as the block of data which the user received in each
  // OnData() callback.
  size_t packet_size_frames_;

  // Size in bytes of each audio packet.
  size_t packet_size_bytes_;

  // Length of the audio endpoint buffer.
  size_t endpoint_buffer_size_frames_;

  // Contains the unique name of the selected endpoint device.
  // Note that AudioManagerBase::kDefaultDeviceId represents the default
  // device role and is not a valid ID as such.
  std::string device_id_;

  // Conversion factor used in delay-estimation calculations.
  // Converts a raw performance counter value to 100-nanosecond unit.
  double perf_count_to_100ns_units_;

  // Conversion factor used in delay-estimation calculations.
  // Converts from milliseconds to audio frames.
  double ms_to_frame_count_;

  // Pointer to the object that will receive the recorded audio samples.
  AudioInputCallback* sink_;

  // An IMMDevice interface which represents an audio endpoint device.
  base::win::ScopedComPtr<IMMDevice> endpoint_device_;

  // An IAudioClient interface which enables a client to create and initialize
  // an audio stream between an audio application and the audio engine.
  base::win::ScopedComPtr<IAudioClient> audio_client_;

  // The IAudioCaptureClient interface enables a client to read input data
  // from a capture endpoint buffer.
  base::win::ScopedComPtr<IAudioCaptureClient> audio_capture_client_;

  // The audio engine will signal this event each time a buffer has been
  // recorded.
  base::win::ScopedHandle audio_samples_ready_event_;

  // This event will be signaled when capturing shall stop.
  base::win::ScopedHandle stop_capture_event_;

  DISALLOW_COPY_AND_ASSIGN(WASAPIAudioInputStream);
};

#endif  // MEDIA_AUDIO_WIN_AUDIO_LOW_LATENCY_INPUT_WIN_H_
