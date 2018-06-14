// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/audio/audio_buffer_source_node.h"
#include "cobalt/audio/audio_context.h"
#include "cobalt/audio/audio_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO: Consolidate ShellAudioBus creation code

namespace cobalt {
namespace audio {

#if defined(COBALT_MEDIA_SOURCE_2016)
typedef media::ShellAudioBus ShellAudioBus;
#else   // defined(COBALT_MEDIA_SOURCE_2016)
typedef ::media::ShellAudioBus ShellAudioBus;
#endif  // defined(COBALT_MEDIA_SOURCE_2016)

const int kRenderBufferSizeFrames = 32;

class AudioDestinationNodeMock : public AudioNode,
                                 public AudioDevice::RenderCallback {
#if defined(COBALT_MEDIA_SOURCE_2016)
  typedef media::ShellAudioBus ShellAudioBus;
#else   // defined(COBALT_MEDIA_SOURCE_2016)
  typedef ::media::ShellAudioBus ShellAudioBus;
#endif  // defined(COBALT_MEDIA_SOURCE_2016)

 public:
  explicit AudioDestinationNodeMock(AudioContext* context)
      : AudioNode(context) {
    AudioLock::AutoLock lock(audio_lock());

    AddInput(new AudioNodeInput(this));
  }

  // From AudioNode.
  scoped_ptr<ShellAudioBus> PassAudioBusFromSource(int32, /*number_of_frames*/
                                                   SampleType, bool*) override {
    NOTREACHED();
    return scoped_ptr<ShellAudioBus>();
  }

  // From AudioDevice::RenderCallback.
  void FillAudioBus(bool all_consumed, ShellAudioBus* audio_bus,
                    bool* silence) override {
    UNREFERENCED_PARAMETER(all_consumed);

    AudioLock::AutoLock lock(audio_lock());

    bool all_finished;
    // Destination node only has one input.
    Input(0)->FillAudioBus(audio_bus, silence, &all_finished);
  }
};

void FillAudioBusFromOneSource(
    scoped_ptr<ShellAudioBus> src_data,
    const AudioNodeChannelInterpretation& interpretation,
    ShellAudioBus* audio_bus, bool* silence) {
  scoped_refptr<AudioContext> audio_context(new AudioContext());
  scoped_refptr<AudioBufferSourceNode> source(
      audio_context->CreateBufferSource());
  scoped_refptr<AudioBuffer> buffer(new AudioBuffer(44100, src_data.Pass()));
  source->set_buffer(buffer);

  scoped_refptr<AudioDestinationNodeMock> destination(
      new AudioDestinationNodeMock(audio_context.get()));
  destination->set_channel_interpretation(interpretation);
  source->Connect(destination, 0, 0, NULL);
  source->Start(0, 0, NULL);

  destination->FillAudioBus(true, audio_bus, silence);
}

class AudioNodeInputOutputTest : public ::testing::Test {
 protected:
  MessageLoop message_loop_;
};

TEST_F(AudioNodeInputOutputTest, StereoToStereoSpeakersLayoutTest) {
  const size_t kNumOfSrcChannels = 2;
  const size_t kNumOfDestChannels = 2;
  const size_t kNumOfFrames = 25;
  const AudioNodeChannelInterpretation kInterpretation =
      kAudioNodeChannelInterpretationSpeakers;

  float src_data_in_float[50];
  for (size_t channel = 0; channel < kNumOfSrcChannels; ++channel) {
    for (size_t frame = 0; frame < kNumOfFrames; ++frame) {
      src_data_in_float[frame * kNumOfSrcChannels + channel] =
          20.0f * (channel + 1);
    }
  }

  scoped_ptr<ShellAudioBus> src_data(
      new ShellAudioBus(kNumOfSrcChannels, kNumOfFrames, src_data_in_float));
  scoped_ptr<ShellAudioBus> audio_bus(
      new ShellAudioBus(kNumOfDestChannels, kRenderBufferSizeFrames,
                        ShellAudioBus::kFloat32, ShellAudioBus::kInterleaved));
  audio_bus->ZeroAllFrames();
  bool silence = true;
  FillAudioBusFromOneSource(src_data.Pass(), kInterpretation, audio_bus.get(),
                            &silence);
  EXPECT_FALSE(silence);

  for (size_t c = 0; c < kNumOfDestChannels; ++c) {
    for (size_t i = 0; i < kRenderBufferSizeFrames; ++i) {
      if (i < kNumOfFrames) {
        if (c == 0) {
          EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 20);
        } else {
          EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 40);
        }
      } else {
        EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 0);
      }
    }
  }
}

TEST_F(AudioNodeInputOutputTest, StereoToStereoDiscreteLayoutTest) {
  const size_t kNumOfSrcChannels = 2;
  const size_t kNumOfDestChannels = 2;
  const size_t kNumOfFrames = 25;
  const AudioNodeChannelInterpretation kInterpretation =
      kAudioNodeChannelInterpretationDiscrete;

  float src_data_in_float[50];
  for (size_t channel = 0; channel < kNumOfSrcChannels; ++channel) {
    for (size_t frame = 0; frame < kNumOfFrames; ++frame) {
      src_data_in_float[frame * kNumOfSrcChannels + channel] =
          20.0f * (channel + 1);
    }
  }

  scoped_ptr<ShellAudioBus> src_data(
      new ShellAudioBus(kNumOfSrcChannels, kNumOfFrames, src_data_in_float));

  scoped_ptr<ShellAudioBus> audio_bus(
      new ShellAudioBus(kNumOfDestChannels, kRenderBufferSizeFrames,
                        ShellAudioBus::kFloat32, ShellAudioBus::kInterleaved));
  audio_bus->ZeroAllFrames();
  bool silence = true;
  FillAudioBusFromOneSource(src_data.Pass(), kInterpretation, audio_bus.get(),
                            &silence);
  EXPECT_FALSE(silence);

  for (size_t c = 0; c < kNumOfDestChannels; ++c) {
    for (size_t i = 0; i < kRenderBufferSizeFrames; ++i) {
      if (i < kNumOfFrames) {
        if (c == 0) {
          EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 20);
        } else {
          EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 40);
        }
      } else {
        EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 0);
      }
    }
  }
}

TEST_F(AudioNodeInputOutputTest, MonoToStereoSpeakersLayoutTest) {
  const size_t kNumOfSrcChannels = 1;
  const size_t kNumOfDestChannels = 2;
  const size_t kNumOfFrames = 25;
  const AudioNodeChannelInterpretation kInterpretation =
      kAudioNodeChannelInterpretationSpeakers;

  float src_data_in_float[25];
  for (size_t i = 0; i < kNumOfFrames; ++i) {
    src_data_in_float[i] = 50.0f;
  }

  scoped_ptr<ShellAudioBus> src_data(
      new ShellAudioBus(kNumOfSrcChannels, kNumOfFrames, src_data_in_float));

  scoped_ptr<ShellAudioBus> audio_bus(
      new ShellAudioBus(kNumOfDestChannels, kRenderBufferSizeFrames,
                        ShellAudioBus::kFloat32, ShellAudioBus::kInterleaved));
  audio_bus->ZeroAllFrames();
  bool silence = true;
  FillAudioBusFromOneSource(src_data.Pass(), kInterpretation, audio_bus.get(),
                            &silence);
  EXPECT_FALSE(silence);

  for (size_t c = 0; c < kNumOfDestChannels; ++c) {
    for (size_t i = 0; i < kRenderBufferSizeFrames; ++i) {
      if (i < kNumOfFrames) {
        EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 50);
      } else {
        EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 0);
      }
    }
  }
}

TEST_F(AudioNodeInputOutputTest, MonoToStereoDiscreteLayoutTest) {
  const size_t kNumOfSrcChannels = 1;
  const size_t kNumOfDestChannels = 2;
  const size_t kNumOfFrames = 25;
  const AudioNodeChannelInterpretation kInterpretation =
      kAudioNodeChannelInterpretationDiscrete;

  float src_data_in_float[25];
  for (size_t i = 0; i < kNumOfFrames; ++i) {
    src_data_in_float[i] = 50.0f;
  }

  scoped_ptr<ShellAudioBus> src_data(
      new ShellAudioBus(kNumOfSrcChannels, kNumOfFrames, src_data_in_float));

  scoped_ptr<ShellAudioBus> audio_bus(
      new ShellAudioBus(kNumOfDestChannels, kRenderBufferSizeFrames,
                        ShellAudioBus::kFloat32, ShellAudioBus::kInterleaved));
  audio_bus->ZeroAllFrames();
  bool silence = true;
  FillAudioBusFromOneSource(src_data.Pass(), kInterpretation, audio_bus.get(),
                            &silence);
  EXPECT_FALSE(silence);

  for (size_t c = 0; c < kNumOfDestChannels; ++c) {
    for (size_t i = 0; i < kRenderBufferSizeFrames; ++i) {
      if (i < kNumOfFrames && c == 0) {
        EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 50);
      } else {
        EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 0);
      }
    }
  }
}

TEST_F(AudioNodeInputOutputTest, QuadToStereoSpeakersLayoutTest) {
  const size_t kNumOfSrcChannels = 4;
  const size_t kNumOfDestChannels = 2;
  const size_t kNumOfFrames = 25;
  const AudioNodeChannelInterpretation kInterpretation =
      kAudioNodeChannelInterpretationSpeakers;

  float src_data_in_float[100];
  for (size_t channel = 0; channel < kNumOfSrcChannels; ++channel) {
    for (size_t frame = 0; frame < kNumOfFrames; ++frame) {
      src_data_in_float[frame * kNumOfSrcChannels + channel] =
          10.0f * (channel + 1);
    }
  }

  scoped_ptr<ShellAudioBus> src_data(
      new ShellAudioBus(kNumOfSrcChannels, kNumOfFrames, src_data_in_float));

  scoped_ptr<ShellAudioBus> audio_bus(
      new ShellAudioBus(kNumOfDestChannels, kRenderBufferSizeFrames,
                        ShellAudioBus::kFloat32, ShellAudioBus::kInterleaved));
  audio_bus->ZeroAllFrames();
  bool silence = true;
  FillAudioBusFromOneSource(src_data.Pass(), kInterpretation, audio_bus.get(),
                            &silence);
  EXPECT_FALSE(silence);

  for (size_t c = 0; c < kNumOfDestChannels; ++c) {
    for (size_t i = 0; i < kRenderBufferSizeFrames; ++i) {
      if (i < kNumOfFrames) {
        if (c == 0) {
          EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 20);
        } else {
          EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 30);
        }
      } else {
        EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 0);
      }
    }
  }
}

TEST_F(AudioNodeInputOutputTest, QuadToStereoDiscreteLayoutTest) {
  const size_t kNumOfSrcChannels = 4;
  const size_t kNumOfDestChannels = 2;
  const size_t kNumOfFrames = 25;
  const AudioNodeChannelInterpretation kInterpretation =
      kAudioNodeChannelInterpretationDiscrete;

  float src_data_in_float[100];
  for (size_t channel = 0; channel < kNumOfSrcChannels; ++channel) {
    for (size_t frame = 0; frame < kNumOfFrames; ++frame) {
      src_data_in_float[frame * kNumOfSrcChannels + channel] =
          10.0f * (channel + 1);
    }
  }

  scoped_ptr<ShellAudioBus> src_data(
      new ShellAudioBus(kNumOfSrcChannels, kNumOfFrames, src_data_in_float));

  scoped_ptr<ShellAudioBus> audio_bus(
      new ShellAudioBus(kNumOfDestChannels, kRenderBufferSizeFrames,
                        ShellAudioBus::kFloat32, ShellAudioBus::kInterleaved));
  audio_bus->ZeroAllFrames();
  bool silence = true;
  FillAudioBusFromOneSource(src_data.Pass(), kInterpretation, audio_bus.get(),
                            &silence);
  EXPECT_FALSE(silence);

  for (size_t c = 0; c < kNumOfDestChannels; ++c) {
    for (size_t i = 0; i < kRenderBufferSizeFrames; ++i) {
      if (i < kNumOfFrames) {
        if (c == 0) {
          EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 10);
        } else {
          EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 20);
        }
      } else {
        EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 0);
      }
    }
  }
}

TEST_F(AudioNodeInputOutputTest, FivePointOneToStereoSpeakersLayoutTest) {
  const size_t kNumOfSrcChannels = 6;
  const size_t kNumOfDestChannels = 2;
  const size_t kNumOfFrames = 10;
  const AudioNodeChannelInterpretation kInterpretation =
      kAudioNodeChannelInterpretationSpeakers;

  float src_data_in_float[60];
  for (size_t channel = 0; channel < kNumOfSrcChannels; ++channel) {
    for (size_t frame = 0; frame < kNumOfFrames; ++frame) {
      src_data_in_float[frame * kNumOfSrcChannels + channel] =
          10.0f * (channel + 1);
    }
  }

  scoped_ptr<ShellAudioBus> src_data(
      new ShellAudioBus(kNumOfSrcChannels, kNumOfFrames, src_data_in_float));

  scoped_ptr<ShellAudioBus> audio_bus(
      new ShellAudioBus(kNumOfDestChannels, kRenderBufferSizeFrames,
                        ShellAudioBus::kFloat32, ShellAudioBus::kInterleaved));
  audio_bus->ZeroAllFrames();
  bool silence = true;
  FillAudioBusFromOneSource(src_data.Pass(), kInterpretation, audio_bus.get(),
                            &silence);
  EXPECT_FALSE(silence);

  for (size_t c = 0; c < kNumOfDestChannels; ++c) {
    for (size_t i = 0; i < kRenderBufferSizeFrames; ++i) {
      if (i < kNumOfFrames) {
        if (c == 0) {
          EXPECT_FLOAT_EQ(audio_bus->GetFloat32Sample(c, i), 66.568f);
        } else {
          EXPECT_FLOAT_EQ(audio_bus->GetFloat32Sample(c, i), 83.639f);
        }
      } else {
        EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 0);
      }
    }
  }
}

TEST_F(AudioNodeInputOutputTest, FivePointOneToStereoDiscreteLayoutTest) {
  const size_t kNumOfSrcChannels = 6;
  const size_t kNumOfDestChannels = 2;
  const size_t kNumOfFrames = 10;
  const AudioNodeChannelInterpretation kInterpretation =
      kAudioNodeChannelInterpretationDiscrete;

  float src_data_in_float[60];
  for (size_t channel = 0; channel < kNumOfSrcChannels; ++channel) {
    for (size_t frame = 0; frame < kNumOfFrames; ++frame) {
      src_data_in_float[frame * kNumOfSrcChannels + channel] =
          10.0f * (channel + 1);
    }
  }

  scoped_ptr<ShellAudioBus> src_data(
      new ShellAudioBus(kNumOfSrcChannels, kNumOfFrames, src_data_in_float));

  scoped_ptr<ShellAudioBus> audio_bus(
      new ShellAudioBus(kNumOfDestChannels, kRenderBufferSizeFrames,
                        ShellAudioBus::kFloat32, ShellAudioBus::kInterleaved));
  audio_bus->ZeroAllFrames();
  bool silence = true;
  FillAudioBusFromOneSource(src_data.Pass(), kInterpretation, audio_bus.get(),
                            &silence);
  EXPECT_FALSE(silence);

  for (size_t c = 0; c < kNumOfDestChannels; ++c) {
    for (size_t i = 0; i < kRenderBufferSizeFrames; ++i) {
      if (i < kNumOfFrames) {
        if (c == 0) {
          EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 10);
        } else {
          EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 20);
        }
      } else {
        EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 0);
      }
    }
  }
}

TEST_F(AudioNodeInputOutputTest, StereoToMonoSpeakersLayoutTest) {
  const size_t kNumOfSrcChannels = 2;
  const size_t kNumOfDestChannels = 1;
  const size_t kNumOfFrames = 25;
  const AudioNodeChannelInterpretation kInterpretation =
      kAudioNodeChannelInterpretationSpeakers;

  float src_data_in_float[50];
  for (size_t channel = 0; channel < kNumOfSrcChannels; ++channel) {
    for (size_t frame = 0; frame < kNumOfFrames; ++frame) {
      src_data_in_float[frame * kNumOfSrcChannels + channel] =
          20.0f * (channel + 1);
    }
  }

  scoped_ptr<ShellAudioBus> src_data(
      new ShellAudioBus(kNumOfSrcChannels, kNumOfFrames, src_data_in_float));

  scoped_ptr<ShellAudioBus> audio_bus(
      new ShellAudioBus(kNumOfDestChannels, kRenderBufferSizeFrames,
                        ShellAudioBus::kFloat32, ShellAudioBus::kInterleaved));
  audio_bus->ZeroAllFrames();
  bool silence = true;
  FillAudioBusFromOneSource(src_data.Pass(), kInterpretation, audio_bus.get(),
                            &silence);
  EXPECT_FALSE(silence);

  for (size_t c = 0; c < kNumOfDestChannels; ++c) {
    for (size_t i = 0; i < kRenderBufferSizeFrames; ++i) {
      if (i < kNumOfFrames) {
        EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 30);
      } else {
        EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 0);
      }
    }
  }
}

TEST_F(AudioNodeInputOutputTest, StereoToMonoDiscreteLayoutTest) {
  const size_t kNumOfSrcChannels = 2;
  const size_t kNumOfDestChannels = 1;
  const size_t kNumOfFrames = 25;
  const AudioNodeChannelInterpretation kInterpretation =
      kAudioNodeChannelInterpretationDiscrete;

  float src_data_in_float[50];
  for (size_t channel = 0; channel < kNumOfSrcChannels; ++channel) {
    for (size_t frame = 0; frame < kNumOfFrames; ++frame) {
      src_data_in_float[frame * kNumOfSrcChannels + channel] =
          20.0f * (channel + 1);
    }
  }

  scoped_ptr<ShellAudioBus> src_data(
      new ShellAudioBus(kNumOfSrcChannels, kNumOfFrames, src_data_in_float));

  scoped_ptr<ShellAudioBus> audio_bus(
      new ShellAudioBus(kNumOfDestChannels, kRenderBufferSizeFrames,
                        ShellAudioBus::kFloat32, ShellAudioBus::kInterleaved));
  audio_bus->ZeroAllFrames();
  bool silence = true;
  FillAudioBusFromOneSource(src_data.Pass(), kInterpretation, audio_bus.get(),
                            &silence);
  EXPECT_FALSE(silence);

  for (size_t c = 0; c < kNumOfDestChannels; ++c) {
    for (size_t i = 0; i < kRenderBufferSizeFrames; ++i) {
      if (i < kNumOfFrames) {
        EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 20);
      } else {
        EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 0);
      }
    }
  }
}

TEST_F(AudioNodeInputOutputTest, QuadToMonoSpeakersLayoutTest) {
  const size_t kNumOfSrcChannels = 4;
  const size_t kNumOfDestChannels = 1;
  const size_t kNumOfFrames = 25;
  const AudioNodeChannelInterpretation kInterpretation =
      kAudioNodeChannelInterpretationSpeakers;

  float src_data_in_float[100];
  for (size_t channel = 0; channel < kNumOfSrcChannels; ++channel) {
    for (size_t frame = 0; frame < kNumOfFrames; ++frame) {
      src_data_in_float[frame * kNumOfSrcChannels + channel] =
          10.0f * (channel + 1);
    }
  }

  scoped_ptr<ShellAudioBus> src_data(
      new ShellAudioBus(kNumOfSrcChannels, kNumOfFrames, src_data_in_float));

  scoped_ptr<ShellAudioBus> audio_bus(
      new ShellAudioBus(kNumOfDestChannels, kRenderBufferSizeFrames,
                        ShellAudioBus::kFloat32, ShellAudioBus::kInterleaved));
  audio_bus->ZeroAllFrames();
  bool silence = true;
  FillAudioBusFromOneSource(src_data.Pass(), kInterpretation, audio_bus.get(),
                            &silence);
  EXPECT_FALSE(silence);

  for (size_t c = 0; c < kNumOfDestChannels; ++c) {
    for (size_t i = 0; i < kRenderBufferSizeFrames; ++i) {
      if (i < kNumOfFrames) {
        EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 25);
      } else {
        EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 0);
      }
    }
  }
}

TEST_F(AudioNodeInputOutputTest, QuadToMonoDiscreteLayoutTest) {
  const size_t kNumOfSrcChannels = 4;
  const size_t kNumOfDestChannels = 1;
  const size_t kNumOfFrames = 25;
  const AudioNodeChannelInterpretation kInterpretation =
      kAudioNodeChannelInterpretationDiscrete;

  float src_data_in_float[100];
  for (size_t channel = 0; channel < kNumOfSrcChannels; ++channel) {
    for (size_t frame = 0; frame < kNumOfFrames; ++frame) {
      src_data_in_float[frame * kNumOfSrcChannels + channel] =
          10.0f * (channel + 1);
    }
  }

  scoped_ptr<ShellAudioBus> src_data(
      new ShellAudioBus(kNumOfSrcChannels, kNumOfFrames, src_data_in_float));

  scoped_ptr<ShellAudioBus> audio_bus(
      new ShellAudioBus(kNumOfDestChannels, kRenderBufferSizeFrames,
                        ShellAudioBus::kFloat32, ShellAudioBus::kInterleaved));
  audio_bus->ZeroAllFrames();
  bool silence = true;
  FillAudioBusFromOneSource(src_data.Pass(), kInterpretation, audio_bus.get(),
                            &silence);
  EXPECT_FALSE(silence);

  for (size_t c = 0; c < kNumOfDestChannels; ++c) {
    for (size_t i = 0; i < kRenderBufferSizeFrames; ++i) {
      if (i < kNumOfFrames) {
        EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 10);
      } else {
        EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 0);
      }
    }
  }
}

TEST_F(AudioNodeInputOutputTest, FivePointOneToMonoSpeakersLayoutTest) {
  const size_t kNumOfSrcChannels = 6;
  const size_t kNumOfDestChannels = 1;
  const size_t kNumOfFrames = 10;
  const AudioNodeChannelInterpretation kInterpretation =
      kAudioNodeChannelInterpretationSpeakers;

  float src_data_in_float[60];
  for (size_t channel = 0; channel < kNumOfSrcChannels; ++channel) {
    for (size_t frame = 0; frame < kNumOfFrames; ++frame) {
      src_data_in_float[frame * kNumOfSrcChannels + channel] =
          10.0f * (channel + 1);
    }
  }

  scoped_ptr<ShellAudioBus> src_data(
      new ShellAudioBus(kNumOfSrcChannels, kNumOfFrames, src_data_in_float));

  scoped_ptr<ShellAudioBus> audio_bus(
      new ShellAudioBus(kNumOfDestChannels, kRenderBufferSizeFrames,
                        ShellAudioBus::kFloat32, ShellAudioBus::kInterleaved));
  audio_bus->ZeroAllFrames();
  bool silence = true;
  FillAudioBusFromOneSource(src_data.Pass(), kInterpretation, audio_bus.get(),
                            &silence);
  EXPECT_FALSE(silence);

  for (size_t c = 0; c < kNumOfDestChannels; ++c) {
    for (size_t i = 0; i < kRenderBufferSizeFrames; ++i) {
      if (i < kNumOfFrames) {
        EXPECT_FLOAT_EQ(audio_bus->GetFloat32Sample(c, i), 106.213f);
      } else {
        EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 0);
      }
    }
  }
}

TEST_F(AudioNodeInputOutputTest, FivePointOneToMonoDiscreteLayoutTest) {
  const size_t kNumOfSrcChannels = 6;
  const size_t kNumOfDestChannels = 1;
  const size_t kNumOfFrames = 10;
  const AudioNodeChannelInterpretation kInterpretation =
      kAudioNodeChannelInterpretationDiscrete;

  float src_data_in_float[60];
  for (size_t channel = 0; channel < kNumOfSrcChannels; ++channel) {
    for (size_t frame = 0; frame < kNumOfFrames; ++frame) {
      src_data_in_float[frame * kNumOfSrcChannels + channel] =
          10.0f * (channel + 1);
    }
  }

  scoped_ptr<ShellAudioBus> src_data(
      new ShellAudioBus(kNumOfSrcChannels, kNumOfFrames, src_data_in_float));

  scoped_ptr<ShellAudioBus> audio_bus(
      new ShellAudioBus(kNumOfDestChannels, kRenderBufferSizeFrames,
                        ShellAudioBus::kFloat32, ShellAudioBus::kInterleaved));
  audio_bus->ZeroAllFrames();
  bool silence = true;
  FillAudioBusFromOneSource(src_data.Pass(), kInterpretation, audio_bus.get(),
                            &silence);
  EXPECT_FALSE(silence);

  for (size_t c = 0; c < kNumOfDestChannels; ++c) {
    for (size_t i = 0; i < kRenderBufferSizeFrames; ++i) {
      if (i < kNumOfFrames) {
        EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 10);
      } else {
        EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 0);
      }
    }
  }
}

TEST_F(AudioNodeInputOutputTest, MultipleInputNodesLayoutTest) {
  scoped_refptr<AudioContext> audio_context(new AudioContext());

  const size_t kNumOfSrcChannels = 2;
  const size_t kNumOfDestChannels = 2;
  const AudioNodeChannelInterpretation kInterpretation =
      kAudioNodeChannelInterpretationSpeakers;

  const size_t kNumOfFrames_1 = 25;
  float src_data_in_float_1[50];
  for (size_t channel = 0; channel < kNumOfSrcChannels; ++channel) {
    for (size_t frame = 0; frame < kNumOfFrames_1; ++frame) {
      src_data_in_float_1[frame * kNumOfSrcChannels + channel] =
          20.0f * (channel + 1);
    }
  }

  scoped_ptr<ShellAudioBus> src_data_1(new ShellAudioBus(
      kNumOfSrcChannels, kNumOfFrames_1, src_data_in_float_1));
  scoped_refptr<AudioBufferSourceNode> source_1(
      audio_context->CreateBufferSource());
  scoped_refptr<AudioBuffer> buffer_1(
      new AudioBuffer(44100, src_data_1.Pass()));
  source_1->set_buffer(buffer_1);

  const size_t kNumOfFrames_2 = 50;
  float src_data_in_float_2[100];
  for (size_t channel = 0; channel < kNumOfSrcChannels; ++channel) {
    for (size_t frame = 0; frame < kNumOfFrames_2; ++frame) {
      src_data_in_float_2[frame * kNumOfSrcChannels + channel] =
          40.0f * (channel + 1);
    }
  }

  scoped_ptr<ShellAudioBus> src_data_2(new ShellAudioBus(
      kNumOfSrcChannels, kNumOfFrames_2, src_data_in_float_2));
  scoped_refptr<AudioBufferSourceNode> source_2(
      audio_context->CreateBufferSource());
  scoped_refptr<AudioBuffer> buffer_2(
      new AudioBuffer(44100, src_data_2.Pass()));
  source_2->set_buffer(buffer_2);

  scoped_refptr<AudioDestinationNodeMock> destination(
      new AudioDestinationNodeMock(audio_context.get()));
  destination->set_channel_interpretation(kInterpretation);
  source_1->Connect(destination, 0, 0, NULL);
  source_2->Connect(destination, 0, 0, NULL);
  source_1->Start(0, 0, NULL);
  source_2->Start(0, 0, NULL);

  scoped_ptr<ShellAudioBus> audio_bus(
      new ShellAudioBus(kNumOfDestChannels, kRenderBufferSizeFrames,
                        ShellAudioBus::kFloat32, ShellAudioBus::kPlanar));
  audio_bus->ZeroAllFrames();
  bool silence = true;
  destination->FillAudioBus(true, audio_bus.get(), &silence);
  EXPECT_FALSE(silence);

  for (size_t c = 0; c < kNumOfDestChannels; ++c) {
    for (size_t i = 0; i < kRenderBufferSizeFrames; ++i) {
      if (i < kNumOfFrames_1) {
        if (c == 0) {
          EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 60);
        } else {
          EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 120);
        }
      } else if (i < kNumOfFrames_2) {
        if (c == 0) {
          EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 40);
        } else {
          EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 80);
        }
      } else {
        EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 0);
      }
    }
  }
}

}  // namespace audio
}  // namespace cobalt
