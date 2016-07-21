// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_BUS_H_
#define MEDIA_BASE_AUDIO_BUS_H_

#include <limits>
#include <vector>

#include "base/logging.h"
#include "base/memory/aligned_memory.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/media_export.h"

namespace media {
class AudioParameters;

// Scoped container for "busing" audio channel data around.  Each channel is
// stored in planar format and guaranteed to be aligned by kChannelAlignment.
// AudioBus objects can be created normally or via wrapping.  Normally, AudioBus
// will dice up a contiguous memory block for channel data.  When wrapped,
// AudioBus instead routes requests for channel data to the wrapped object.
class MEDIA_EXPORT AudioBus {
 public:
#if defined(__LB_PS3__)
  // We interleave our data already, but all data is floats, so we require that
  // no frame be smaller than 4 bytes.
  enum { kChannelAlignment = 4 };
#else
  // Guaranteed alignment of each channel's data; use 16-byte alignment for easy
  // SSE optimizations.
  enum { kChannelAlignment = 16 };
#endif

  // Creates a new AudioBus and allocates |channels| of length |frames|.  Uses
  // channels() and frames_per_buffer() from AudioParameters if given.
  static scoped_ptr<AudioBus> Create(int channels, int frames);
  static scoped_ptr<AudioBus> Create(const AudioParameters& params);

#if defined(__LB_SHELL__) || defined(COBALT)
  static scoped_ptr<AudioBus> Create(int channels, int frames_per_channel,
                                     int bytes_per_frame, bool interleaved);
#endif  // defined(__LB_SHELL__) || defined(COBALT)

  // Creates a new AudioBus with the given number of channels, but zero length.
  // It's expected to be used with SetChannelData() and set_frames() to
  // wrap externally allocated memory.
  static scoped_ptr<AudioBus> CreateWrapper(int channels);

  // Creates a new AudioBus from an existing channel vector.  Does not transfer
  // ownership of |channel_data| to AudioBus; i.e., |channel_data| must outlive
  // the returned AudioBus.  Each channel must be aligned by kChannelAlignment.
  static scoped_ptr<AudioBus> WrapVector(
      int frames, const std::vector<float*>& channel_data);

  // Creates a new AudioBus by wrapping an existing block of memory.  Block must
  // be at least CalculateMemorySize() bytes in size.  |data| must outlive the
  // returned AudioBus.  |data| must be aligned by kChannelAlignment.
  static scoped_ptr<AudioBus> WrapMemory(int channels, int frames, void* data);
  static scoped_ptr<AudioBus> WrapMemory(const AudioParameters& params,
                                         void* data);
  // Returns the required memory size to use the WrapMemory() method.
  static int CalculateMemorySize(const AudioParameters& params);

  // Calculates the required size for an AudioBus given the number of channels
  // and frames.
  static int CalculateMemorySize(int channels, int frames);

  // Helper methods for converting an AudioBus from and to interleaved integer
  // data.  Expects interleaving to be [ch0, ch1, ..., chN, ch0, ch1, ...] with
  // |bytes_per_sample| per value.  Values are scaled and bias corrected during
  // conversion.  ToInterleaved() will also clip values to format range.
  // Handles uint8, int16, and int32 currently.  FromInterleaved() will zero out
  // any unfilled frames when |frames| is less than frames().
  void FromInterleaved(const void* source, int frames, int bytes_per_sample);
  void ToInterleaved(int frames, int bytes_per_sample, void* dest) const;
  void ToInterleavedPartial(int start_frame, int frames, int bytes_per_sample,
                            void* dest) const;

#if defined(__LB_SHELL__) || defined(COBALT)
  // The following two functions work on float samples instead of integer
  // samples.
  // FromInterleavedFloat fills the audio bus with interleaved samples. It is
  // possible to fill frames in the middle of the audio bus by using a non-zero
  // "audio_bus_offset". Note that it will not fill the rest samples with 0.
  // "frames" indicates frame count per channel instead of the combined frames.
  void FromInterleavedFloat(const float* source, int frames,
                            int audio_bus_offset);
  // ToInterleavedFloat will interleave data from the audio bus and store them
  // into dest.
  // "frames" indicates frame count per channel instead of the combined frames.
  // It is an error if the requested frame is larger than what the audio bus
  // can offer.
  // "extra_channels" has to be greater than or equal to 0. A non-zero value
  // indicates that there are more channels in the "dest" than in this audio bus
  // and they will be filled with 0.
  void ToInterleavedFloat(int frames, int audio_bus_offset, int extra_channels,
                          float* dest) const;
#endif  // defined(__LB_SHELL__) || defined(COBALT)

  // Similar to FromInterleaved() above, but meant for streaming sources.  Does
  // not zero out remaining frames, the caller is responsible for doing so using
  // ZeroFramesPartial().  Frames are deinterleaved from the start of |source|
  // to channel(x)[start_frame].
  void FromInterleavedPartial(const void* source, int start_frame, int frames,
                              int bytes_per_sample);

  // Helper method for copying channel data from one AudioBus to another.  Both
  // AudioBus object must have the same frames() and channels().
  void CopyTo(AudioBus* dest) const;

  // Returns a raw pointer to the requested channel.  Pointer is guaranteed to
  // have a 16-byte alignment.  Warning: Do not rely on having sane (i.e. not
  // inf, nan, or between [-1.0, 1.0]) values in the channel data.
  float* channel(int channel) {
    return channel_data_[static_cast<size_t>(channel)];
  }
  const float* channel(int channel) const {
    return channel_data_[static_cast<size_t>(channel)];
  }
  void SetChannelData(int channel, float* data);

  int channels() const {
    DCHECK_LE(channel_data_.size(),
              static_cast<size_t>(std::numeric_limits<int>::max()));
    return static_cast<int>(channel_data_.size());
  }

  int frames() const { return frames_; }
  void set_frames(int frames);

  // Helper method for zeroing out all channels of audio data.
  void Zero();
  void ZeroFrames(int frames);
  void ZeroFramesPartial(int start_frame, int frames);

 private:
  friend class scoped_ptr<AudioBus>;
  ~AudioBus();

  AudioBus(int channels, int frames);
  AudioBus(int channels, int frames, float* data);
  AudioBus(int frames, const std::vector<float*>& channel_data);
  explicit AudioBus(int channels);

  // Helper method for building |channel_data_| from a block of memory.  |data|
  // must be at least BlockSize() bytes in size.
  void BuildChannelData(int channels, int aligned_frame, float* data);

  // Contiguous block of channel memory.
  scoped_ptr_malloc<float, base::ScopedPtrAlignedFree> data_;

  std::vector<float*> channel_data_;
  int frames_;

  // Protect SetChannelData() and set_frames() for use by CreateWrapper().
  bool can_set_channel_data_;

  DISALLOW_COPY_AND_ASSIGN(AudioBus);
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_BUS_H_
