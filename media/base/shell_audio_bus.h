/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MEDIA_BASE_SHELL_AUDIO_BUS_H_
#define MEDIA_BASE_SHELL_AUDIO_BUS_H_

#include <vector>

#include "base/logging.h"
#include "base/memory/aligned_memory.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/media_export.h"

namespace media {

// This swiss army knife class encapsulates audio data in multiple channels, in
// different storage types and with different sample sizes.  It also provides
// operation to convert, mix between different types of audio data.  It should
// be used whenever such audio data is stored or passed around.
// In this class, "sample" is one audio wave form data at a certain time from a
// certain channel, while "frame" refers to all samples at the same time from
// all channels.  For example, for a 48000KHz stereo audio with samples in
// float, its sample size in bytes is 4 but its frame size in bytes is 8.  One
// second of such audio contains 48000 frames (96000 samples).
// Note: This class doesn't do endianness conversions.  It assumes that all data
// is in the correct endianness.
class MEDIA_EXPORT ShellAudioBus {
 public:
  // Guaranteed alignment of each channel's data; use 64-byte alignment so it
  // satisfies all our current platforms.  Note that this is only used for
  // buffers that are allocated and owned by the ShellAudioBus.  We don't
  // enforce alignment for the buffers passed in and extra caution should be
  // taken if they are used as hardware buffer.
  static const size_t kChannelAlignmentInBytes = 64;

  enum SampleType { kInt16, kFloat32 };

  enum StorageType { kInterleaved, kPlanar };

  ShellAudioBus(size_t channels, size_t frames, SampleType sample_type,
                StorageType storage_type);
  ShellAudioBus(size_t frames, const std::vector<float*>& samples);
  ShellAudioBus(size_t channels, size_t frames, float* samples);
  ShellAudioBus(size_t frames, const std::vector<int16*>& samples);
  ShellAudioBus(size_t channels, size_t frames, int16* samples);

  size_t channels() const { return channels_; }
  size_t frames() const { return frames_; }
  SampleType sample_type() const { return sample_type_; }
  StorageType storage_type() const { return storage_type_; }
  size_t GetSampleSizeInBytes() const;
  uint8* interleaved_data();
  const uint8* interleaved_data() const;
  const uint8* planar_data(size_t channel) const;

  int16 GetInt16Sample(size_t channel, size_t frame) const {
    DCHECK_EQ(sample_type_, kInt16);
    return *reinterpret_cast<const int16*>(GetSamplePtr(channel, frame));
  }
  float GetFloat32Sample(size_t channel, size_t frame) const {
    DCHECK_EQ(sample_type_, kFloat32);
    return *reinterpret_cast<const float*>(GetSamplePtr(channel, frame));
  }

  void ZeroFrames(size_t start_frame, size_t end_frame);
  void ZeroAllFrames() { ZeroFrames(0, frames()); }

  // Copy frames from |source| provided that it has the same number of channels
  // as the destination object (this).  This function does any necessary
  // conversion between different sample types and storage types.  When source
  // has less frames than the destination object, it will only copy these frames
  // and will not fill the rest frames in our buffer with 0.
  void Assign(const ShellAudioBus& source);

  // The same as the above function except that this function also does mixing.
  // |matrix| is a |dest.channels()| row * |source.channels()| column matrix in
  // row major.
  // dest.sample[dest_channel][frame] =
  //     source.sample[0][frame] * matrix[dest_channel * source.channels() + 0]
  //   + source.sample[1][frame] * matrix[dest_channel * source.channels() + 1]
  //     ...
  //   + source.sample[source.channels() - 1][frame] *
  //         matrix[channels() * source.channels() + source.channels() - 1];
  // Note: Both objects must have storage type of kFloat32.
  void Assign(const ShellAudioBus& source, const std::vector<float>& matrix);

  // The following functions are the same as the Assign() functions except that
  // they add the calculated samples to the target samples instead of replacing
  // the target samples with the calculated samples.
  // Note: Both objects must have storage type of kFloat32.
  void Mix(const ShellAudioBus& source);
  void Mix(const ShellAudioBus& source, const std::vector<float>& matrix);

 public:
  // The .*ForTypes? functions below are optimized versions that assume what
  // storage type the bus is using.  They are meant to be called after
  // checking what storage type the bus is once, and then performing a batch
  // of operations, where it is known that the type will not change.
  template <typename SampleTypeName, StorageType T>
  inline uint8* GetSamplePtrForType(size_t channel, size_t frame) const {
    DCHECK_LT(channel, channels_);
    DCHECK_LT(frame, frames_);

    if (T == kInterleaved) {
      return channel_data_[0] +
             sizeof(SampleTypeName) * (channels_ * frame + channel);
    } else if (T == kPlanar) {
      return channel_data_[channel] + sizeof(SampleTypeName) * frame;
    } else {
      NOTREACHED();
    }

    return NULL;
  }

  template <typename SampleTypeName, StorageType T>
  inline SampleTypeName GetSampleForType(size_t channel, size_t frame) const {
    return *reinterpret_cast<const SampleTypeName*>(
        GetSamplePtrForType<SampleTypeName, T>(channel, frame));
  }

  template <typename SourceSampleType,
            typename DestSampleType,
            StorageType SourceStorageType,
            StorageType DestStorageType>
  void MixForTypes(const ShellAudioBus& source);

 private:
  void SetFloat32Sample(size_t channel, size_t frame, float sample) {
    DCHECK_EQ(sample_type_, kFloat32);
    *reinterpret_cast<float*>(GetSamplePtr(channel, frame)) = sample;
  }
  uint8* GetSamplePtr(size_t channel, size_t frame);
  const uint8* GetSamplePtr(size_t channel, size_t frame) const;

  // Contiguous block of channel memory if the memory is owned by this object.
  scoped_ptr_malloc<uint8, base::ScopedPtrAlignedFree> data_;

  std::vector<uint8*> channel_data_;
  size_t channels_;
  size_t frames_;
  SampleType sample_type_;
  StorageType storage_type_;

  DISALLOW_COPY_AND_ASSIGN(ShellAudioBus);
};

}  // namespace media

#endif  // MEDIA_BASE_SHELL_AUDIO_BUS_H_
