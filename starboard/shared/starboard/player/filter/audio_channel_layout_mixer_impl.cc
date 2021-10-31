// Copyright 2019 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "starboard/shared/starboard/player/filter/audio_channel_layout_mixer.h"

#include <vector>

#include "starboard/common/log.h"
#include "starboard/shared/starboard/media/media_util.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

namespace {

using media::GetBytesPerSample;

// 1 -> 2
const float kMonoToStereoMatrix[] = {
    1.0f,  // output.L = input
    1.0f,  // output.R = input
};

// 1 -> 4
const float kMonoToQuadMatrix[] = {
    1.0f,  // output.L = input
    1.0f,  // output.R = input
    0.0f,  // output.SL = 0
    0.0f,  // output.SR = 0
};

// 1 -> 5.1
const float kMonoToFivePointOneMatrix[] = {
    0.0f,  // output.L = 0
    0.0f,  // output.R = 0
    1.0f,  // output.C = input
    0.0f,  // output.LFE = 0
    0.0f,  // output.SL = 0
    0.0f,  // output.SR = 0
};

// 2 -> 4
const float kStereoToQuadMatrix[] = {
    1.0f, 0.0f,  // output.L = input.L
    0.0f, 1.0f,  // output.R = input.R
    0.0f, 0.0f,  // output.SL = 0
    0.0f, 0.0f,  // output.SR = 0
};

// 2 -> 5.1
const float kStereoToFivePointOneMatrix[] = {
    1.0f, 0.0f,  // output.L = input.L
    0.0f, 1.0f,  // output.R = input.R
    0.0f, 0.0f,  // output.C = 0
    0.0f, 0.0f,  // output.LFE = 0
    0.0f, 0.0f,  // output.SL = 0
    0.0f, 0.0f,  // output.SR = 0
};

// 4 -> 5.1
const float kQuadToFivePointOneMatrix[] = {
    1.0f, 0.0f, 0.0f, 0.0f,  // output.L = input.L
    0.0f, 1.0f, 0.0f, 0.0f,  // output.R = input.R
    0.0f, 0.0f, 0.0f, 0.0f,  // output.C = 0
    0.0f, 0.0f, 0.0f, 0.0f,  // output.LFE = 0
    0.0f, 0.0f, 1.0f, 0.0f,  // output.SL = input.SL
    0.0f, 0.0f, 0.0f, 1.0f,  // output.SR = input.SR
};

// 2 -> 1
const float kStereoToMonoMatrix[] = {
    0.5f, 0.5f,  // output = 0.5 * (input.L + input.R)
};

// 4 -> 1
const float kQuadToMonoMatrix[] = {
    // output = 0.25 * (input.L + input.R + input.SL + input.SR)
    0.25f, 0.25f, 0.25f, 0.25f,
};

// 5.1 -> 1
const float kFivePointOneToMonoMatrix[] = {
    // output = sqrt(0.5) * (input.L + input.R) + input.C + 0.5 * (input.SL +
    //          input.SR)
    0.7071f, 0.7071f, 1.0f, 0.0f, 0.5f, 0.5f,
};

// 4 -> 2
const float kQuadToStereoMatrix[] = {
    0.5f, 0.0f, 0.5f, 0.0f,  // output.L = 0.5 * (input.L + input.SL)
    0.0f, 0.5f, 0.0f, 0.5f,  // output.R = 0.5 * (input.R + input.SR)
};

// 5.1 -> 2
const float kFivePointOneToStereoMatrix[] = {
    // output.L = L + sqrt(0.5) * (input.C + input.SL)
    1.0f, 0.0f, 0.7071f, 0.0f, 0.7071f, 0.0f,
    // output.R = R + sqrt(0.5) * (input.C + input.SR)
    0.0f, 1.0f, 0.7071f, 0.0f, 0.0f, 0.7071f,
};

// 5.1 -> 4
const float kFivePointOneToQuadMatrix[] = {
    // output.L = L + sqrt(0.5) * input.C
    1.0f, 0.0f, 0.7071f, 0.0f, 0.0f, 0.0f,
    // output.R = R + sqrt(0.5) * input.C
    0.0f, 1.0f, 0.7071f, 0.0f, 0.0f, 0.0f,
    // output.SL = input.SL
    0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
    // output.SR = input.SR
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
};

// Get the samples of frames at |frame_index|.  If |input| is already
// interleaved, the return pointer points to the buffer contained inside
// |input|. If |input| is planar, it will copy all samples into |aux_buffer| and
// return |aux_buffer| instead.  Note that |aux_buffer| should be large enough
// to hold samples from all channels of the frame.
template <typename SampleType>
const SampleType* GetInterleavedSamplesOfFrame(
    const scoped_refptr<DecodedAudio>& input,
    int frame_index,
    SampleType* aux_buffer) {
  const SampleType* input_buffer =
      reinterpret_cast<const SampleType*>(input->buffer());
  if (input->storage_type() == kSbMediaAudioFrameStorageTypeInterleaved) {
    return input_buffer + frame_index * input->channels();
  }
  SB_DCHECK(input->storage_type() == kSbMediaAudioFrameStorageTypePlanar);
  for (size_t channel_index = 0; channel_index < input->channels();
       channel_index++) {
    aux_buffer[channel_index] =
        input_buffer[channel_index * input->frames() + frame_index];
  }
  return aux_buffer;
}

template <typename SampleType>
void StoreInterleavedSamplesOfFrame(const SampleType* samples,
                                    scoped_refptr<DecodedAudio>* destination,
                                    int frame_index) {
  SampleType* dest_buffer =
      reinterpret_cast<SampleType*>((*destination)->buffer());
  for (size_t channel_index = 0; channel_index < (*destination)->channels();
       channel_index++) {
    if ((*destination)->storage_type() ==
        kSbMediaAudioFrameStorageTypeInterleaved) {
      dest_buffer[frame_index * (*destination)->channels() + channel_index] =
          samples[channel_index];
    } else {
      SB_DCHECK((*destination)->storage_type() ==
                kSbMediaAudioFrameStorageTypePlanar);
      dest_buffer[channel_index * (*destination)->frames() + frame_index] =
          samples[channel_index];
    }
  }
}

template <typename SampleType>
SampleType ClipSample(float sample);

template <>
float ClipSample<float>(float sample) {
  return sample;
}

template <>
int16_t ClipSample<int16_t>(float sample) {
  if (sample > kSbInt16Max) {
    return kSbInt16Max;
  }
  if (sample < kSbInt16Min) {
    return kSbInt16Min;
  }
  return sample;
}

// Apply a matrix of [number_of_output_samples, number_of_input_samples] to
// |input_samples| and store the result in |output_samples|.
template <typename SampleType>
void MixFrameWithMatrix(const SampleType* input_frame,
                        int number_of_input_channels,
                        const float* matrix,
                        SampleType* output_frame,
                        int number_of_output_channels) {
  for (size_t output_index = 0; output_index < number_of_output_channels;
       output_index++) {
    float output_sample = 0;
    for (size_t input_index = 0; input_index < number_of_input_channels;
         input_index++) {
      output_sample +=
          input_frame[input_index] *
          matrix[output_index * number_of_input_channels + input_index];
    }
    output_frame[output_index] = ClipSample<SampleType>(output_sample);
  }
}

class AudioChannelLayoutMixerImpl : public AudioChannelLayoutMixer {
 public:
  AudioChannelLayoutMixerImpl(SbMediaAudioSampleType sample_type,
                              SbMediaAudioFrameStorageType storage_type,
                              int output_channels);

  scoped_refptr<DecodedAudio> Mix(
      const scoped_refptr<DecodedAudio>& input) override;

 private:
  template <typename SampleType>
  scoped_refptr<DecodedAudio> Mix(const scoped_refptr<DecodedAudio>& input,
                                  const float* matrix);

  scoped_refptr<DecodedAudio> MixMonoToStereoOptimized(
      const scoped_refptr<DecodedAudio>& input);

  SbMediaAudioSampleType sample_type_;
  SbMediaAudioFrameStorageType storage_type_;
  int output_channels_;
};

AudioChannelLayoutMixerImpl::AudioChannelLayoutMixerImpl(
    SbMediaAudioSampleType sample_type,
    SbMediaAudioFrameStorageType storage_type,
    int output_channels)
    : sample_type_(sample_type),
      storage_type_(storage_type),
      output_channels_(output_channels) {}

scoped_refptr<DecodedAudio> AudioChannelLayoutMixerImpl::Mix(
    const scoped_refptr<DecodedAudio>& input) {
  SB_DCHECK(input->sample_type() == sample_type_);
  SB_DCHECK(input->storage_type() == storage_type_);

  if (input->channels() == output_channels_) {
    return input;
  }

  if (input->channels() == 1 && output_channels_ == 2) {
    return MixMonoToStereoOptimized(input);
  }

  const float* matrix = nullptr;
  if (input->channels() == 1) {
    if (output_channels_ == 2) {
      matrix = kMonoToStereoMatrix;
    } else if (output_channels_ == 4) {
      matrix = kMonoToQuadMatrix;
    } else if (output_channels_ == 6) {
      matrix = kMonoToFivePointOneMatrix;
    }
  } else if (input->channels() == 2) {
    if (output_channels_ == 1) {
      matrix = kStereoToMonoMatrix;
    } else if (output_channels_ == 4) {
      matrix = kStereoToQuadMatrix;
    } else if (output_channels_ == 6) {
      matrix = kStereoToFivePointOneMatrix;
    }
  } else if (input->channels() == 4) {
    if (output_channels_ == 1) {
      matrix = kQuadToMonoMatrix;
    } else if (output_channels_ == 2) {
      matrix = kQuadToStereoMatrix;
    } else if (output_channels_ == 6) {
      matrix = kQuadToFivePointOneMatrix;
    }
  } else if (input->channels() == 6) {
    if (output_channels_ == 1) {
      matrix = kFivePointOneToMonoMatrix;
    } else if (output_channels_ == 2) {
      matrix = kFivePointOneToStereoMatrix;
    } else if (output_channels_ == 4) {
      matrix = kFivePointOneToQuadMatrix;
    }
  }

  if (!matrix) {
    SB_NOTREACHED() << "Mixing " << input->channels() << " channels to "
                    << output_channels_ << " channels is not supported.";
    return scoped_refptr<DecodedAudio>();
  }

  if (sample_type_ == kSbMediaAudioSampleTypeInt16Deprecated) {
    return Mix<int16_t>(input, matrix);
  }
  SB_DCHECK(sample_type_ == kSbMediaAudioSampleTypeFloat32);
  return Mix<float>(input, matrix);
}

template <typename SampleType>
scoped_refptr<DecodedAudio> AudioChannelLayoutMixerImpl::Mix(
    const scoped_refptr<DecodedAudio>& input,
    const float* matrix) {
  size_t frames = input->frames();
  scoped_refptr<DecodedAudio> output(new DecodedAudio(
      output_channels_, sample_type_, storage_type_, input->timestamp(),
      frames * output_channels_ * GetBytesPerSample(sample_type_)));
  SampleType aux_buffer[8];
  SampleType output_buffer[8];
  for (int frame_index = 0; frame_index < frames; frame_index++) {
    const SampleType* interleavedSamplesOfFrame =
        GetInterleavedSamplesOfFrame(input, frame_index, aux_buffer);
    MixFrameWithMatrix(interleavedSamplesOfFrame, input->channels(), matrix,
                       output_buffer, output_channels_);
    StoreInterleavedSamplesOfFrame(output_buffer, &output, frame_index);
  }
  return output;
}

scoped_refptr<DecodedAudio>
AudioChannelLayoutMixerImpl::MixMonoToStereoOptimized(
    const scoped_refptr<DecodedAudio>& input) {
  SB_DCHECK(output_channels_ == 2);
  SB_DCHECK(input->channels() == 1);

  scoped_refptr<DecodedAudio> output(
      new DecodedAudio(output_channels_, sample_type_, storage_type_,
                       input->timestamp(), input->size() * 2));
  if (storage_type_ == kSbMediaAudioFrameStorageTypeInterleaved) {
    size_t frames_left = input->frames();
    size_t bytes_per_sample = GetBytesPerSample(sample_type_);
    const uint8_t* src_buffer_ptr = input->buffer();
    uint8_t* dest_buffer_ptr = output->buffer();
    while (frames_left > 0) {
      memcpy(dest_buffer_ptr, src_buffer_ptr, bytes_per_sample);
      dest_buffer_ptr += bytes_per_sample;
      memcpy(dest_buffer_ptr, src_buffer_ptr, bytes_per_sample);
      dest_buffer_ptr += bytes_per_sample;
      src_buffer_ptr += bytes_per_sample;
      frames_left--;
    }
  } else {
    SB_DCHECK(storage_type_ == kSbMediaAudioFrameStorageTypePlanar);
    memcpy(output->buffer(), input->buffer(), input->size());
    memcpy(output->buffer() + input->size(), input->buffer(),
                 input->size());
  }
  return output;
}

}  // namespace

// static
scoped_ptr<AudioChannelLayoutMixer> AudioChannelLayoutMixer::Create(
    SbMediaAudioSampleType sample_type,
    SbMediaAudioFrameStorageType storage_type,
    int output_channels) {
  return scoped_ptr<AudioChannelLayoutMixer>(new AudioChannelLayoutMixerImpl(
      sample_type, storage_type, output_channels));
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
