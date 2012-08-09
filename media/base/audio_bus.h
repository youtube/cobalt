// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_BUS_H_
#define MEDIA_BASE_AUDIO_BUS_H_

#include <vector>

#include "base/memory/aligned_memory.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/media_export.h"

namespace media {
class AudioParameters;

// Scoped container for "busing" audio channel data around.  Each channel is
// stored in planar format and guaranteed to have a 16-byte alignment.  AudioBus
// objects can be created normally or via wrapping.  Normally, AudioBus will
// dice up a contiguous memory block for channel data.  When wrapped, AudioBus
// instead routes requests for channel data to the wrapped object.
class MEDIA_EXPORT AudioBus {
 public:
  // Creates a new AudioBus and allocates |channels| of length |frames|.  Uses
  // channels() and frames_per_buffer() if given an AudioParameters object.
  static scoped_ptr<AudioBus> Create(int channels, int frames);
  static scoped_ptr<AudioBus> Create(const AudioParameters& params);

  // Creates a new AudioBus from an existing channel vector.  Does not transfer
  // ownership of |channel_data| to AudioBus; i.e., |channel_data| must outlive
  // the returned AudioBus.  Each channel pointer must be 16-byte aligned.
  static scoped_ptr<AudioBus> WrapVector(
      int frames, const std::vector<float*>& channel_data);

  // Returns a raw pointer to internal channel data.  Useful for copying state
  // between two AudioBus objects created with the same parameters.  data_size()
  // is in bytes.  Can not be used with an AudioBus constructed via wrapping.
  void* data();
  int data_size() const;

  // Returns a raw pointer to the requested channel.  Pointer is guaranteed to
  // have a 16-byte alignment.
  float* channel(int channel) { return channel_data_[channel]; }
  const float* channel(int channel) const { return channel_data_[channel]; }

  int channels() const { return channel_data_.size(); }
  int frames() const { return frames_; }

  // Helper method for zeroing out all channels of audio data.
  void Zero();
  void ZeroFrames(int frames);

 private:
  friend class scoped_ptr<AudioBus>;
  ~AudioBus();

  AudioBus(int channels, int frames);
  AudioBus(int frames, const std::vector<float*>& channel_data);

  // Contiguous block of channel memory.
  scoped_ptr_malloc<float, base::ScopedPtrAlignedFree> data_;
  int data_size_;

  std::vector<float*> channel_data_;
  int frames_;

  DISALLOW_COPY_AND_ASSIGN(AudioBus);
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_BUS_H_
