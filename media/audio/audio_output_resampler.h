// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_OUTPUT_RESAMPLER_H_
#define MEDIA_AUDIO_AUDIO_OUTPUT_RESAMPLER_H_

#include <map>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/time.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_output_dispatcher.h"
#include "media/audio/audio_parameters.h"

namespace media {

class OnMoreDataResampler;

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
    : public AudioOutputDispatcher {
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

 private:
  friend class base::RefCountedThreadSafe<AudioOutputResampler>;
  virtual ~AudioOutputResampler();

  // Used to initialize the FIFO and resamplers.
  void Initialize();

  // Used by StopStream()/CloseStream() to clear internal state.  When
  // |delete_callback| is set, the callbacks[stream_proxy] is destroyed.
  void Reset(AudioOutputProxy* stream_proxy, bool delete_callback);

  // Dispatcher to proxy all AudioOutputDispatcher calls too.
  scoped_refptr<AudioOutputDispatcher> dispatcher_;

  // Vector of each outstanding OnMoreDataResampler object.  A new instance is
  // created on every StartStream() call.  Access must be locked.
  typedef std::map<AudioOutputProxy*, OnMoreDataResampler*> CallbackMap;
  CallbackMap callbacks_;
  base::Lock callbacks_lock_;

  // Ratio of input bytes to output bytes used to correct playback delay with
  // regard to buffering and resampling.
  double io_ratio_;

  // Used by AudioOutputDispatcherImpl; kept so we can reinitialize on the fly.
  base::TimeDelta close_delay_;

  // AudioParameters used to setup the output stream.
  AudioParameters output_params_;

  DISALLOW_COPY_AND_ASSIGN(AudioOutputResampler);
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_OUTPUT_RESAMPLER_H_
