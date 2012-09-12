// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_OUTPUT_RESAMPLER_H_
#define MEDIA_AUDIO_AUDIO_OUTPUT_RESAMPLER_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/time.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_output_dispatcher.h"
#include "media/audio/audio_parameters.h"

namespace media {

class AudioPullFifo;
class MultiChannelResampler;

// AudioOutputResampler is a browser-side resampling and rebuffering solution
// which ensures audio data is always output at given parameters.  The rough
// flow is: Client -> [FIFO] -> [Resampler] -> Output Device.
//
// The FIFO and resampler are only used when necessary.  To be clear:
//   - The resampler is only used if the input and output sample rates differ.
//   - The FIFO is only used if the input and output frame sizes differ or if
//     the resampler is used.
//
// AOR works by intercepting the AudioSourceCallback provided to StartStream()
// and redirecting to the appropriate resampling or FIFO callback which passes
// through to the original callback only when necessary.
//
// AOR will automatically fall back from AUDIO_PCM_LOW_LATENCY to
// AUDIO_PCM_LINEAR if the output device fails to open at the requested output
// parameters.
// TODO(dalecurtis): Ideally the low latency path will be as reliable as the
// high latency path once we have channel mixing and support querying for the
// hardware's configured bit depth.  Monitor the UMA stats for fallback and
// remove fallback support once it's stable.  http://crbug.com/148418
//
// Currently channel downmixing and upmixing is not supported.
// TODO(dalecurtis): Add channel remixing.  http://crbug.com/138762
class MEDIA_EXPORT AudioOutputResampler
    : public AudioOutputDispatcher,
      public AudioOutputStream::AudioSourceCallback {
 public:
  AudioOutputResampler(AudioManager* audio_manager,
                       const AudioParameters& input_params,
                       const AudioParameters& output_params,
                       const base::TimeDelta& close_delay);

  // AudioOutputDispatcher interface.
  virtual bool OpenStream() OVERRIDE;
  virtual bool StartStream(AudioOutputStream::AudioSourceCallback* callback,
                           AudioOutputProxy* stream_proxy) OVERRIDE;
  virtual void StopStream(AudioOutputProxy* stream_proxy) OVERRIDE;
  virtual void StreamVolumeSet(AudioOutputProxy* stream_proxy,
                               double volume) OVERRIDE;
  virtual void CloseStream(AudioOutputProxy* stream_proxy) OVERRIDE;
  virtual void Shutdown() OVERRIDE;

  // AudioSourceCallback interface.
  virtual int OnMoreData(AudioBus* dest,
                         AudioBuffersState buffers_state) OVERRIDE;
  virtual int OnMoreIOData(AudioBus* source,
                           AudioBus* dest,
                           AudioBuffersState buffers_state) OVERRIDE;
  virtual void OnError(AudioOutputStream* stream, int code) OVERRIDE;
  virtual void WaitTillDataReady() OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<AudioOutputResampler>;
  virtual ~AudioOutputResampler();

  // Used to initialize the FIFO and resamplers.
  void Initialize();

  // Called by MultiChannelResampler when more data is necessary.
  void ProvideInput(AudioBus* audio_bus);

  // Called by AudioPullFifo when more data is necessary.  Requires
  // |source_lock_| to have been acquired.
  void SourceCallback_Locked(AudioBus* audio_bus);

  // Used by StopStream()/CloseStream()/Shutdown() to clear internal state.
  // TODO(dalecurtis): Probably only one of these methods needs to call this,
  // the rest should DCHECK()/CHECK() that the values were reset.
  void Reset();

  // Handles resampling.
  scoped_ptr<MultiChannelResampler> resampler_;

  // Dispatcher to proxy all AudioOutputDispatcher calls too.
  scoped_refptr<AudioOutputDispatcher> dispatcher_;

  // Source callback and associated lock.
  base::Lock source_lock_;
  AudioOutputStream::AudioSourceCallback* source_callback_;

  // Used to buffer data between the client and the output device in cases where
  // the client buffer size is not the same as the output device buffer size.
  // Bound to SourceCallback_Locked() so must only be used when |source_lock_|
  // has already been acquired.
  scoped_ptr<AudioPullFifo> audio_fifo_;

  // Ratio of input bytes to output bytes used to correct playback delay with
  // regard to buffering and resampling.
  double io_ratio_;

  // Used by AudioOutputDispatcherImpl; kept so we can reinitialize on the fly.
  base::TimeDelta close_delay_;

  // Last AudioBuffersState object received via OnMoreData(), used to correct
  // playback delay by ProvideInput() and passed on to |source_callback_|.
  AudioBuffersState current_buffers_state_;

  // Total number of bytes (in terms of output parameters) stored in resampler
  // or FIFO buffers which have not been sent to the audio device.
  int outstanding_audio_bytes_;

  // AudioParameters used to setup the output stream.
  AudioParameters output_params_;

  DISALLOW_COPY_AND_ASSIGN(AudioOutputResampler);
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_OUTPUT_RESAMPLER_H_
