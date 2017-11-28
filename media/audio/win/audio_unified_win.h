// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_WIN_AUDIO_UNIFIED_WIN_H_
#define MEDIA_AUDIO_WIN_AUDIO_UNIFIED_WIN_H_

#include <Audioclient.h>
#include <MMDeviceAPI.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_handle.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_parameters.h"
#include "media/base/media_export.h"

namespace media {

class AudioManagerWin;

// Implementation of AudioOutputStream for Windows using the Core Audio API
// where both capturing and rendering takes place on the same thread to enable
// audio I/O.
//
// The user should also ensure that audio I/O is supported by calling
// HasUnifiedDefaultIO().
//
// Implementation notes:
//
// - Certain conditions must be fulfilled to support audio I/O:
//    o Both capture and render side must use the same sample rate.
//    o Both capture and render side must use the same channel count.
//    o Both capture and render side must use the same channel configuration.
//    o See HasUnifiedDefaultIO() for more details.
//
// TODO(henrika):
//
//  - Add support for exclusive mode.
//  - Add multi-channel support.
//  - Add support for non-matching sample rates.
//
class MEDIA_EXPORT WASAPIUnifiedStream
    : public AudioOutputStream,
      public base::DelegateSimpleThread::Delegate {
 public:
  // The ctor takes all the usual parameters, plus |manager| which is the
  // the audio manager who is creating this object.
  WASAPIUnifiedStream(AudioManagerWin* manager,
                      const AudioParameters& params);

  // The dtor is typically called by the AudioManager only and it is usually
  // triggered by calling AudioOutputStream::Close().
  virtual ~WASAPIUnifiedStream();

  // Implementation of AudioOutputStream.
  virtual bool Open() override;
  virtual void Start(AudioSourceCallback* callback) override;
  virtual void Stop() override;
  virtual void Close() override;
  virtual void SetVolume(double volume) override;
  virtual void GetVolume(double* volume) override;

  // Returns true if all conditions to support audio IO are fulfilled.
  // Input and output sides of the Audio Engine must use the same native
  // device period (requires e.g. identical sample rates) and have the same
  // channel count.
  static bool HasUnifiedDefaultIO();

  bool started() const {
    return audio_io_thread_.get() != NULL;
  }

 private:
  // DelegateSimpleThread::Delegate implementation.
  virtual void Run() override;

  // Issues the OnError() callback to the |source_|.
  void HandleError(HRESULT err);

  // Stops and joins the audio thread in case of an error.
  void StopAndJoinThread(HRESULT err);

  // Converts unique endpoint ID to user-friendly device name.
  std::string GetDeviceName(LPCWSTR device_id) const;

  // Returns the number of channels the audio engine uses for its internal
  // processing/mixing of shared-mode streams for the default endpoint device.
  int endpoint_channel_count() { return format_.Format.nChannels; }

  // Contains the thread ID of the creating thread.
  base::PlatformThreadId creating_thread_id_;

  // Our creator, the audio manager needs to be notified when we close.
  AudioManagerWin* manager_;

  // The sharing mode for the streams.
  // Valid values are AUDCLNT_SHAREMODE_SHARED and AUDCLNT_SHAREMODE_EXCLUSIVE
  // where AUDCLNT_SHAREMODE_SHARED is the default.
  AUDCLNT_SHAREMODE share_mode_;

  // Rendering and capturing is driven by this thread (no message loop).
  // All OnMoreIOData() callbacks will be called from this thread.
  scoped_ptr<base::DelegateSimpleThread> audio_io_thread_;

  // Contains the desired audio format which is set up at construction.
  // Extended PCM waveform format structure based on WAVEFORMATEXTENSIBLE.
  // Use this for multiple channel and hi-resolution PCM data.
  WAVEFORMATPCMEX format_;

  // True when successfully opened.
  bool opened_;

  // Size in bytes of each audio frame (4 bytes for 16-bit stereo PCM).
  size_t frame_size_;

  // Size in audio frames of each audio packet where an audio packet
  // is defined as the block of data which the source is expected to deliver
  // in each OnMoreIOData() callback.
  size_t packet_size_frames_;

  // Length of the audio endpoint buffer.
  size_t endpoint_render_buffer_size_frames_;
  size_t endpoint_capture_buffer_size_frames_;

  // Counts the number of audio frames written to the endpoint buffer.
  uint64 num_written_frames_;

  // Time stamp for last delay measurement.
  base::TimeTicks last_delay_sample_time_;

  // Contains the total (sum of render and capture) delay in milliseconds.
  double total_delay_ms_;

  // Pointer to the client that will deliver audio samples to be played out.
  AudioSourceCallback* source_;

  // IMMDevice interfaces which represents audio endpoint devices.
  base::win::ScopedComPtr<IMMDevice> endpoint_render_device_;
  base::win::ScopedComPtr<IMMDevice> endpoint_capture_device_;

  // IAudioClient interfaces which enables a client to create and initialize
  // an audio stream between an audio application and the audio engine.
  base::win::ScopedComPtr<IAudioClient> audio_output_client_;
  base::win::ScopedComPtr<IAudioClient> audio_input_client_;

  // IAudioRenderClient interfaces enables a client to write output
  // data to a rendering endpoint buffer.
  base::win::ScopedComPtr<IAudioRenderClient> audio_render_client_;

  // IAudioCaptureClient interfaces enables a client to read input
  // data from a capturing endpoint buffer.
  base::win::ScopedComPtr<IAudioCaptureClient> audio_capture_client_;

  // The audio engine will signal this event each time a buffer has been
  // recorded.
  base::win::ScopedHandle capture_event_;

  // This event will be signaled when streaming shall stop.
  base::win::ScopedHandle stop_streaming_event_;

  // Container for retrieving data from AudioSourceCallback::OnMoreIOData().
  scoped_ptr<AudioBus> render_bus_;

  // Container for sending data to AudioSourceCallback::OnMoreIOData().
  scoped_ptr<AudioBus> capture_bus_;

  DISALLOW_COPY_AND_ASSIGN(WASAPIUnifiedStream);
};

}  // namespace media

#endif  // MEDIA_AUDIO_WIN_AUDIO_UNIFIED_WIN_H_
