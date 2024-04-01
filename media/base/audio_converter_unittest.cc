// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_converter.h"

#include <stddef.h>

#include <memory>
#include <tuple>

#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/fake_audio_render_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// Parameters which control the many input case tests.
static const int kConvertInputs = 8;
static const int kConvertCycles = 3;

// Parameters used for testing.
static const ChannelLayout kChannelLayout = CHANNEL_LAYOUT_STEREO;
static const int kHighLatencyBufferSize = 2048;
static const int kLowLatencyBufferSize = 256;
static const int kSampleRate = 48000;

// Number of full sine wave cycles for each Render() call.
static const int kSineCycles = 4;

// Tuple of <input rate, output rate, output channel layout, epsilon>.
typedef std::tuple<int, int, ChannelLayout, double> AudioConverterTestData;
class AudioConverterTest
    : public testing::TestWithParam<AudioConverterTestData> {
 public:
  AudioConverterTest() : epsilon_(std::get<3>(GetParam())) {
    // Create input and output parameters based on test parameters.
    input_parameters_ =
        AudioParameters(AudioParameters::AUDIO_PCM_LINEAR, kChannelLayout,
                        std::get<0>(GetParam()), kHighLatencyBufferSize);
    output_parameters_ = AudioParameters(
        AudioParameters::AUDIO_PCM_LOW_LATENCY, std::get<2>(GetParam()),
        std::get<1>(GetParam()), kLowLatencyBufferSize);

    converter_ = std::make_unique<AudioConverter>(input_parameters_,
                                                  output_parameters_, false);

    audio_bus_ = AudioBus::Create(output_parameters_);
    expected_audio_bus_ = AudioBus::Create(output_parameters_);

    // Allocate one callback for generating expected results.
    double step = kSineCycles / static_cast<double>(
        output_parameters_.frames_per_buffer());
    expected_callback_ =
        std::make_unique<FakeAudioRenderCallback>(step, kSampleRate);
  }

  // Creates |count| input callbacks to be used for conversion testing.
  void InitializeInputs(int count) {
    // Setup FakeAudioRenderCallback step to compensate for resampling.
    double scale_factor = input_parameters_.sample_rate() /
        static_cast<double>(output_parameters_.sample_rate());
    double step = kSineCycles / (scale_factor *
        static_cast<double>(output_parameters_.frames_per_buffer()));

    for (int i = 0; i < count; ++i) {
      fake_callbacks_.push_back(
          std::make_unique<FakeAudioRenderCallback>(step, kSampleRate));
      converter_->AddInput(fake_callbacks_[i].get());
    }
  }

  // Resets all input callbacks to a pristine state.
  void Reset() {
    converter_->Reset();
    for (size_t i = 0; i < fake_callbacks_.size(); ++i)
      fake_callbacks_[i]->reset();
    expected_callback_->reset();
  }

  // Sets the volume on all input callbacks to |volume|.
  void SetVolume(float volume) {
    for (size_t i = 0; i < fake_callbacks_.size(); ++i)
      fake_callbacks_[i]->set_volume(volume);
  }

  // Validates audio data between |audio_bus_| and |expected_audio_bus_| from
  // |index|..|frames| after |scale| is applied to the expected audio data.
  bool ValidateAudioData(int index, int frames, float scale) {
    for (int i = 0; i < audio_bus_->channels(); ++i) {
      for (int j = index; j < frames; ++j) {
        double error = fabs(audio_bus_->channel(i)[j] -
            expected_audio_bus_->channel(i)[j] * scale);
        if (error > epsilon_) {
          EXPECT_NEAR(expected_audio_bus_->channel(i)[j] * scale,
                      audio_bus_->channel(i)[j], epsilon_)
              << " i=" << i << ", j=" << j;
          return false;
        }
      }
    }
    return true;
  }

  // Runs a single Convert() stage, fills |expected_audio_bus_| appropriately,
  // and validates equality with |audio_bus_| after |scale| is applied.
  bool RenderAndValidateAudioData(float scale) {
    // Render actual audio data.
    converter_->Convert(audio_bus_.get());

    // Render expected audio data.
    expected_callback_->Render(base::TimeDelta(), base::TimeTicks::Now(), 0,
                               expected_audio_bus_.get());

    // Zero out unused channels in the expected AudioBus just as AudioConverter
    // would during channel mixing.
    for (int i = input_parameters_.channels();
         i < output_parameters_.channels(); ++i) {
      memset(expected_audio_bus_->channel(i), 0,
             audio_bus_->frames() * sizeof(*audio_bus_->channel(i)));
    }

    return ValidateAudioData(0, audio_bus_->frames(), scale);
  }

  // Fills |audio_bus_| fully with |value|.
  void FillAudioData(float value) {
    for (int i = 0; i < audio_bus_->channels(); ++i) {
      std::fill(audio_bus_->channel(i),
                audio_bus_->channel(i) + audio_bus_->frames(), value);
    }
  }

  // Verifies converter output with a |inputs| number of transform inputs.
  void RunTest(int inputs) {
    InitializeInputs(inputs);

    SetVolume(0);
    for (int i = 0; i < kConvertCycles; ++i)
      ASSERT_TRUE(RenderAndValidateAudioData(0));

    Reset();

    // Set a different volume for each input and verify the results.
    float total_scale = 0;
    for (size_t i = 0; i < fake_callbacks_.size(); ++i) {
      float volume = static_cast<float>(i) / fake_callbacks_.size();
      total_scale += volume;
      fake_callbacks_[i]->set_volume(volume);
    }
    for (int i = 0; i < kConvertCycles; ++i)
      ASSERT_TRUE(RenderAndValidateAudioData(total_scale));

    Reset();

    // Remove every other input.
    for (size_t i = 1; i < fake_callbacks_.size(); i += 2)
      converter_->RemoveInput(fake_callbacks_[i].get());

    SetVolume(1);
    float scale = inputs > 1 ? inputs / 2.0f : inputs;
    for (int i = 0; i < kConvertCycles; ++i)
      ASSERT_TRUE(RenderAndValidateAudioData(scale));
  }

 protected:
  virtual ~AudioConverterTest() = default;

  // Converter under test.
  std::unique_ptr<AudioConverter> converter_;

  // Input and output parameters used for AudioConverter construction.
  AudioParameters input_parameters_;
  AudioParameters output_parameters_;

  // Destination AudioBus for AudioConverter output.
  std::unique_ptr<AudioBus> audio_bus_;

  // AudioBus containing expected results for comparison with |audio_bus_|.
  std::unique_ptr<AudioBus> expected_audio_bus_;

  // Vector of all input callbacks used to drive AudioConverter::Convert().
  std::vector<std::unique_ptr<FakeAudioRenderCallback>> fake_callbacks_;

  // Parallel input callback which generates the expected output.
  std::unique_ptr<FakeAudioRenderCallback> expected_callback_;

  // Epsilon value with which to perform comparisons between |audio_bus_| and
  // |expected_audio_bus_|.
  double epsilon_;

  DISALLOW_COPY_AND_ASSIGN(AudioConverterTest);
};

// Ensure the buffer delay provided by AudioConverter is accurate.
TEST(AudioConverterTest, AudioDelayAndDiscreteChannelCount) {
  // Choose input and output parameters such that the transform must make
  // multiple calls to fill the buffer.
  AudioParameters input_parameters(AudioParameters::AUDIO_PCM_LINEAR,
                                   CHANNEL_LAYOUT_DISCRETE, kSampleRate,
                                   kLowLatencyBufferSize);
  input_parameters.set_channels_for_discrete(10);
  AudioParameters output_parameters(AudioParameters::AUDIO_PCM_LINEAR,
                                    CHANNEL_LAYOUT_DISCRETE, kSampleRate * 2,
                                    kHighLatencyBufferSize);
  output_parameters.set_channels_for_discrete(5);

  AudioConverter converter(input_parameters, output_parameters, false);
  FakeAudioRenderCallback callback(0.2, kSampleRate);
  std::unique_ptr<AudioBus> audio_bus = AudioBus::Create(output_parameters);
  converter.AddInput(&callback);
  converter.Convert(audio_bus.get());

  // double input_sample_rate = input_parameters.sample_rate();
  // int fill_count =
  //     (output_parameters.frames_per_buffer() * input_sample_rate /
  //      output_parameters.sample_rate()) /
  //      input_parameters.frames_per_buffer();
  //
  // This magic number is the accumulated MultiChannelResampler delay after
  // |fill_count| (4) callbacks to provide input. The number of frames delayed
  // is an implementation detail of the SincResampler chunk size (480 for the
  // first two callbacks, 512 for the last two callbacks). See
  // SincResampler.ChunkSize().
  int kExpectedDelay = 992;
  auto expected_delay =
      AudioTimestampHelper::FramesToTime(kExpectedDelay, kSampleRate);
  EXPECT_EQ(expected_delay, callback.last_delay());
  EXPECT_EQ(input_parameters.channels(), callback.last_channel_count());
}

TEST_P(AudioConverterTest, ArbitraryOutputRequestSize) {
  // Resize output bus to be half of |output_parameters_|'s frames_per_buffer().
  audio_bus_ = AudioBus::Create(output_parameters_.channels(),
                                output_parameters_.frames_per_buffer() / 2);
  RunTest(1);
}

TEST_P(AudioConverterTest, NoInputs) {
  FillAudioData(1.0f);
  EXPECT_TRUE(RenderAndValidateAudioData(0.0f));
}

TEST_P(AudioConverterTest, OneInput) {
  RunTest(1);
}

TEST_P(AudioConverterTest, ManyInputs) {
  RunTest(kConvertInputs);
}

INSTANTIATE_TEST_SUITE_P(
    AudioConverterTest,
    AudioConverterTest,
    testing::Values(
        // No resampling. No channel mixing.
        std::make_tuple(44100, 44100, CHANNEL_LAYOUT_STEREO, 0.00000048),

        // Upsampling. Channel upmixing.
        std::make_tuple(44100, 48000, CHANNEL_LAYOUT_QUAD, 0.033),

        // Downsampling. Channel downmixing.
        std::make_tuple(48000, 41000, CHANNEL_LAYOUT_MONO, 0.042)));

}  // namespace media
