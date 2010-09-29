// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_AUDIO_RENDERER_IMPL_H_
#define MEDIA_FILTERS_AUDIO_RENDERER_IMPL_H_

// This is the default implementation of AudioRenderer, which uses the audio
// interfaces to open an audio device.  Although it cannot be used in the
// sandbox, it serves as a reference implementation and can be used in other
// applications such as the test player.
//
// Note: THIS IS NOT THE AUDIO RENDERER USED IN CHROME.
//
// See src/chrome/renderer/media/audio_renderer_impl.h for chrome's
// implementation.

#include <deque>

#include "base/lock.h"
#include "media/audio/audio_io.h"
#include "media/base/buffers.h"
#include "media/base/factory.h"
#include "media/base/filters.h"
#include "media/filters/audio_renderer_base.h"

namespace media {

class AudioRendererImpl : public AudioRendererBase,
                          public AudioOutputStream::AudioSourceCallback {
 public:
  // FilterFactory provider.
  static FilterFactory* CreateFilterFactory() {
    return new FilterFactoryImpl0<AudioRendererImpl>();
  }

  static bool IsMediaFormatSupported(const MediaFormat& media_format);

  // MediaFilter implementation.
  virtual void SetPlaybackRate(float playback_rate);

  // AudioRenderer implementation.
  virtual void SetVolume(float volume);

  // AudioSourceCallback implementation.
  virtual uint32 OnMoreData(AudioOutputStream* stream, uint8* dest,
                            uint32 len, AudioBuffersState buffers_state);
  virtual void OnClose(AudioOutputStream* stream);
  virtual void OnError(AudioOutputStream* stream, int code);

 protected:
  // Only allow a factory to create this class.
  friend class FilterFactoryImpl0<AudioRendererImpl>;
  AudioRendererImpl();
  virtual ~AudioRendererImpl();

  // AudioRendererBase implementation.
  virtual bool OnInitialize(const MediaFormat& media_format);
  virtual void OnStop();

 private:
  // Audio output stream device.
  AudioOutputStream* stream_;
  int bytes_per_second_;

  DISALLOW_COPY_AND_ASSIGN(AudioRendererImpl);
};

}  // namespace media

#endif  // MEDIA_FILTERS_AUDIO_RENDERER_IMPL_H_
