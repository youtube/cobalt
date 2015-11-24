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

#include "cobalt/audio/audio_context.h"

#include "cobalt/audio/audio_buffer_source_node.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace audio {

typedef ::media::ShellAudioBus ShellAudioBus;

const int kRenderBufferSizeFrames = 32;

class AudioDestinationNodeMock : public AudioNode,
                                 public AudioDevice::RenderCallback {
  typedef ::media::ShellAudioBus ShellAudioBus;

 public:
  explicit AudioDestinationNodeMock(AudioContext* context)
      : AudioNode(context) {
    AddInput(new AudioNodeInput(this));
  }

  // From AudioNode.
  scoped_ptr<ShellAudioBus> PassAudioBusFromSource(
      int32 /*number_of_frames*/) OVERRIDE {
    NOTREACHED();
    return scoped_ptr<ShellAudioBus>();
  }

  // From AudioDevice::RenderCallback.
  void FillAudioBus(ShellAudioBus* audio_bus, bool* silence) OVERRIDE {
    // Destination node only has one input.
    Input(0)->FillAudioBus(audio_bus, silence);
  }
};

void FillAudioBusFromOneSource(
    size_t num_of_src_channel, size_t num_of_frames,
    scoped_array<uint8> src_data,
    const AudioNode::ChannelInterpretation& interpretation,
    ShellAudioBus* audio_bus, bool* silence) {
  scoped_refptr<AudioBufferSourceNode> source(new AudioBufferSourceNode(NULL));
  scoped_refptr<AudioBuffer> buffer(
      new AudioBuffer(NULL, 44100, static_cast<int32>(num_of_frames),
                      static_cast<int32>(num_of_src_channel), src_data.Pass()));
  source->set_buffer(buffer);

  scoped_refptr<AudioDestinationNodeMock> destination(
      new AudioDestinationNodeMock(NULL));
  destination->set_channel_interpretation(interpretation);
  source->Connect(destination, 0, 0, NULL);
  source->Start(0, 0, NULL);

  destination->FillAudioBus(audio_bus, silence);
}

TEST(AudioNodeInputOutputTest, StereoToStereoSpeakersLayoutTest) {
  size_t num_of_src_channel = 2;
  size_t num_of_dest_channel = 2;
  size_t num_of_frames = 25;
  AudioNode::ChannelInterpretation interpretation = AudioNode::kSpeakers;

  float src_data_in_float[50];
  for (size_t c = 0; c < num_of_src_channel; ++c) {
    for (size_t i = 0; i < num_of_frames; ++i) {
      src_data_in_float[c * num_of_frames + i] = 20.0f * (c + 1);
    }
  }

  scoped_array<uint8> src_data(new uint8[200]);
  uint8* src_buffer = src_data.get();
  memcpy(src_buffer, src_data_in_float, 200 * sizeof(uint8));

  scoped_ptr<ShellAudioBus> audio_bus(
      new ShellAudioBus(num_of_dest_channel, kRenderBufferSizeFrames,
                        ShellAudioBus::kFloat32, ShellAudioBus::kPlanar));
  audio_bus->ZeroAllFrames();
  bool silence = true;
  FillAudioBusFromOneSource(num_of_src_channel, num_of_frames, src_data.Pass(),
                            interpretation, audio_bus.get(), &silence);
  EXPECT_FALSE(silence);

  for (size_t c = 0; c < num_of_dest_channel; ++c) {
    for (size_t i = 0; i < kRenderBufferSizeFrames; ++i) {
      if (i < num_of_frames) {
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

TEST(AudioNodeInputOutputTest, StereoToStereoDiscreteLayoutTest) {
  size_t num_of_src_channel = 2;
  size_t num_of_dest_channel = 2;
  size_t num_of_frames = 25;
  AudioNode::ChannelInterpretation interpretation = AudioNode::kDiscrete;

  float src_data_in_float[50];
  for (size_t c = 0; c < num_of_src_channel; ++c) {
    for (size_t i = 0; i < num_of_frames; ++i) {
      src_data_in_float[c * num_of_frames + i] = 20.0f * (c + 1);
    }
  }

  scoped_array<uint8> src_data(new uint8[200]);
  uint8* src_buffer = src_data.get();
  memcpy(src_buffer, src_data_in_float, 200 * sizeof(uint8));

  scoped_ptr<ShellAudioBus> audio_bus(
      new ShellAudioBus(num_of_dest_channel, kRenderBufferSizeFrames,
                        ShellAudioBus::kFloat32, ShellAudioBus::kPlanar));
  audio_bus->ZeroAllFrames();
  bool silence = true;
  FillAudioBusFromOneSource(num_of_src_channel, num_of_frames, src_data.Pass(),
                            interpretation, audio_bus.get(), &silence);
  EXPECT_FALSE(silence);

  for (size_t c = 0; c < num_of_dest_channel; ++c) {
    for (size_t i = 0; i < kRenderBufferSizeFrames; ++i) {
      if (i < num_of_frames) {
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

TEST(AudioNodeInputOutputTest, MonoToStereoSpeakersLayoutTest) {
  size_t num_of_src_channel = 1;
  size_t num_of_dest_channel = 2;
  size_t num_of_frames = 25;
  AudioNode::ChannelInterpretation interpretation = AudioNode::kSpeakers;

  float src_data_in_float[25];
  for (size_t i = 0; i < num_of_frames; ++i) {
    src_data_in_float[i] = 50.0f;
  }

  scoped_array<uint8> src_data(new uint8[100]);
  uint8* src_buffer = src_data.get();
  memcpy(src_buffer, src_data_in_float, 100 * sizeof(uint8));

  scoped_ptr<ShellAudioBus> audio_bus(
      new ShellAudioBus(num_of_dest_channel, kRenderBufferSizeFrames,
                        ShellAudioBus::kFloat32, ShellAudioBus::kPlanar));
  audio_bus->ZeroAllFrames();
  bool silence = true;
  FillAudioBusFromOneSource(num_of_src_channel, num_of_frames, src_data.Pass(),
                            interpretation, audio_bus.get(), &silence);
  EXPECT_FALSE(silence);

  for (size_t c = 0; c < num_of_dest_channel; ++c) {
    for (size_t i = 0; i < kRenderBufferSizeFrames; ++i) {
      if (i < num_of_frames) {
        EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 50);
      } else {
        EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 0);
      }
    }
  }
}

TEST(AudioNodeInputOutputTest, MonoToStereoDiscreteLayoutTest) {
  size_t num_of_src_channel = 1;
  size_t num_of_dest_channel = 2;
  size_t num_of_frames = 25;
  AudioNode::ChannelInterpretation interpretation = AudioNode::kDiscrete;

  float src_data_in_float[25];
  for (size_t i = 0; i < num_of_frames; ++i) {
    src_data_in_float[i] = 50.0f;
  }

  scoped_array<uint8> src_data(new uint8[100]);
  uint8* src_buffer = src_data.get();
  memcpy(src_buffer, src_data_in_float, 100 * sizeof(uint8));

  scoped_ptr<ShellAudioBus> audio_bus(
      new ShellAudioBus(num_of_dest_channel, kRenderBufferSizeFrames,
                        ShellAudioBus::kFloat32, ShellAudioBus::kPlanar));
  audio_bus->ZeroAllFrames();
  bool silence = true;
  FillAudioBusFromOneSource(num_of_src_channel, num_of_frames, src_data.Pass(),
                            interpretation, audio_bus.get(), &silence);
  EXPECT_FALSE(silence);

  for (size_t c = 0; c < num_of_dest_channel; ++c) {
    for (size_t i = 0; i < kRenderBufferSizeFrames; ++i) {
      if (i < num_of_frames && c == 0) {
        EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 50);
      } else {
        EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 0);
      }
    }
  }
}

TEST(AudioNodeInputOutputTest, QuadToStereoSpeakersLayoutTest) {
  size_t num_of_src_channel = 4;
  size_t num_of_dest_channel = 2;
  size_t num_of_frames = 25;
  AudioNode::ChannelInterpretation interpretation = AudioNode::kSpeakers;

  float src_data_in_float[100];
  for (size_t c = 0; c < num_of_src_channel; ++c) {
    for (size_t i = 0; i < num_of_frames; ++i) {
      src_data_in_float[c * num_of_frames + i] = 10.0f * (c + 1);
    }
  }

  scoped_array<uint8> src_data(new uint8[400]);
  uint8* src_buffer = src_data.get();
  memcpy(src_buffer, src_data_in_float, 400 * sizeof(uint8));

  scoped_ptr<ShellAudioBus> audio_bus(
      new ShellAudioBus(num_of_dest_channel, kRenderBufferSizeFrames,
                        ShellAudioBus::kFloat32, ShellAudioBus::kPlanar));
  audio_bus->ZeroAllFrames();
  bool silence = true;
  FillAudioBusFromOneSource(num_of_src_channel, num_of_frames, src_data.Pass(),
                            interpretation, audio_bus.get(), &silence);
  EXPECT_FALSE(silence);

  for (size_t c = 0; c < num_of_dest_channel; ++c) {
    for (size_t i = 0; i < kRenderBufferSizeFrames; ++i) {
      if (i < num_of_frames) {
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

TEST(AudioNodeInputOutputTest, QuadToStereoDiscreteLayoutTest) {
  size_t num_of_src_channel = 4;
  size_t num_of_dest_channel = 2;
  size_t num_of_frames = 25;
  AudioNode::ChannelInterpretation interpretation = AudioNode::kDiscrete;

  float src_data_in_float[100];
  for (size_t c = 0; c < num_of_src_channel; ++c) {
    for (size_t i = 0; i < num_of_frames; ++i) {
      src_data_in_float[c * num_of_frames + i] = 10.0f * (c + 1);
    }
  }

  scoped_array<uint8> src_data(new uint8[400]);
  uint8* src_buffer = src_data.get();
  memcpy(src_buffer, src_data_in_float, 400 * sizeof(uint8));

  scoped_ptr<ShellAudioBus> audio_bus(
      new ShellAudioBus(num_of_dest_channel, kRenderBufferSizeFrames,
                        ShellAudioBus::kFloat32, ShellAudioBus::kPlanar));
  audio_bus->ZeroAllFrames();
  bool silence = true;
  FillAudioBusFromOneSource(num_of_src_channel, num_of_frames, src_data.Pass(),
                            interpretation, audio_bus.get(), &silence);
  EXPECT_FALSE(silence);

  for (size_t c = 0; c < num_of_dest_channel; ++c) {
    for (size_t i = 0; i < kRenderBufferSizeFrames; ++i) {
      if (i < num_of_frames) {
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

TEST(AudioNodeInputOutputTest, FivePointOneToStereoSpeakersLayoutTest) {
  size_t num_of_src_channel = 6;
  size_t num_of_dest_channel = 2;
  size_t num_of_frames = 10;
  AudioNode::ChannelInterpretation interpretation = AudioNode::kSpeakers;

  float src_data_in_float[60];
  for (size_t c = 0; c < num_of_src_channel; ++c) {
    for (size_t i = 0; i < num_of_frames; ++i) {
      src_data_in_float[c * num_of_frames + i] = 10.0f * (c + 1);
    }
  }

  scoped_array<uint8> src_data(new uint8[240]);
  uint8* src_buffer = src_data.get();
  memcpy(src_buffer, src_data_in_float, 240 * sizeof(uint8));

  scoped_ptr<ShellAudioBus> audio_bus(
      new ShellAudioBus(num_of_dest_channel, kRenderBufferSizeFrames,
                        ShellAudioBus::kFloat32, ShellAudioBus::kPlanar));
  audio_bus->ZeroAllFrames();
  bool silence = true;
  FillAudioBusFromOneSource(num_of_src_channel, num_of_frames, src_data.Pass(),
                            interpretation, audio_bus.get(), &silence);
  EXPECT_FALSE(silence);

  for (size_t c = 0; c < num_of_dest_channel; ++c) {
    for (size_t i = 0; i < kRenderBufferSizeFrames; ++i) {
      if (i < num_of_frames) {
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

TEST(AudioNodeInputOutputTest, FivePointOneToStereoDiscreteLayoutTest) {
  size_t num_of_src_channel = 6;
  size_t num_of_dest_channel = 2;
  size_t num_of_frames = 10;
  AudioNode::ChannelInterpretation interpretation = AudioNode::kDiscrete;

  float src_data_in_float[60];
  for (size_t c = 0; c < num_of_src_channel; ++c) {
    for (size_t i = 0; i < num_of_frames; ++i) {
      src_data_in_float[c * num_of_frames + i] = 10.0f * (c + 1);
    }
  }

  scoped_array<uint8> src_data(new uint8[240]);
  uint8* src_buffer = src_data.get();
  memcpy(src_buffer, src_data_in_float, 240 * sizeof(uint8));

  scoped_ptr<ShellAudioBus> audio_bus(
      new ShellAudioBus(num_of_dest_channel, kRenderBufferSizeFrames,
                        ShellAudioBus::kFloat32, ShellAudioBus::kPlanar));
  audio_bus->ZeroAllFrames();
  bool silence = true;
  FillAudioBusFromOneSource(num_of_src_channel, num_of_frames, src_data.Pass(),
                            interpretation, audio_bus.get(), &silence);
  EXPECT_FALSE(silence);

  for (size_t c = 0; c < num_of_dest_channel; ++c) {
    for (size_t i = 0; i < kRenderBufferSizeFrames; ++i) {
      if (i < num_of_frames) {
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

TEST(AudioNodeInputOutputTest, StereoToMonoSpeakersLayoutTest) {
  size_t num_of_src_channel = 2;
  size_t num_of_dest_channel = 1;
  size_t num_of_frames = 25;
  AudioNode::ChannelInterpretation interpretation = AudioNode::kSpeakers;

  float src_data_in_float[50];
  for (size_t c = 0; c < num_of_src_channel; ++c) {
    for (size_t i = 0; i < num_of_frames; ++i) {
      src_data_in_float[c * num_of_frames + i] = 20.0f * (c + 1);
    }
  }

  scoped_array<uint8> src_data(new uint8[200]);
  uint8* src_buffer = src_data.get();
  memcpy(src_buffer, src_data_in_float, 200 * sizeof(uint8));

  scoped_ptr<ShellAudioBus> audio_bus(
      new ShellAudioBus(num_of_dest_channel, kRenderBufferSizeFrames,
                        ShellAudioBus::kFloat32, ShellAudioBus::kPlanar));
  audio_bus->ZeroAllFrames();
  bool silence = true;
  FillAudioBusFromOneSource(num_of_src_channel, num_of_frames, src_data.Pass(),
                            interpretation, audio_bus.get(), &silence);
  EXPECT_FALSE(silence);

  for (size_t c = 0; c < num_of_dest_channel; ++c) {
    for (size_t i = 0; i < kRenderBufferSizeFrames; ++i) {
      if (i < num_of_frames) {
        EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 30);
      } else {
        EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 0);
      }
    }
  }
}

TEST(AudioNodeInputOutputTest, StereoToMonoDiscreteLayoutTest) {
  size_t num_of_src_channel = 2;
  size_t num_of_dest_channel = 1;
  size_t num_of_frames = 25;
  AudioNode::ChannelInterpretation interpretation = AudioNode::kDiscrete;

  float src_data_in_float[50];
  for (size_t c = 0; c < num_of_src_channel; ++c) {
    for (size_t i = 0; i < num_of_frames; ++i) {
      src_data_in_float[c * num_of_frames + i] = 20.0f * (c + 1);
    }
  }

  scoped_array<uint8> src_data(new uint8[200]);
  uint8* src_buffer = src_data.get();
  memcpy(src_buffer, src_data_in_float, 200 * sizeof(uint8));

  scoped_ptr<ShellAudioBus> audio_bus(
      new ShellAudioBus(num_of_dest_channel, kRenderBufferSizeFrames,
                        ShellAudioBus::kFloat32, ShellAudioBus::kPlanar));
  audio_bus->ZeroAllFrames();
  bool silence = true;
  FillAudioBusFromOneSource(num_of_src_channel, num_of_frames, src_data.Pass(),
                            interpretation, audio_bus.get(), &silence);
  EXPECT_FALSE(silence);

  for (size_t c = 0; c < num_of_dest_channel; ++c) {
    for (size_t i = 0; i < kRenderBufferSizeFrames; ++i) {
      if (i < num_of_frames) {
        EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 20);
      } else {
        EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 0);
      }
    }
  }
}

TEST(AudioNodeInputOutputTest, QuadToMonoSpeakersLayoutTest) {
  size_t num_of_src_channel = 4;
  size_t num_of_dest_channel = 1;
  size_t num_of_frames = 25;
  AudioNode::ChannelInterpretation interpretation = AudioNode::kSpeakers;

  float src_data_in_float[100];
  for (size_t c = 0; c < num_of_src_channel; ++c) {
    for (size_t i = 0; i < num_of_frames; ++i) {
      src_data_in_float[c * num_of_frames + i] = 10.0f * (c + 1);
    }
  }

  scoped_array<uint8> src_data(new uint8[400]);
  uint8* src_buffer = src_data.get();
  memcpy(src_buffer, src_data_in_float, 400 * sizeof(uint8));

  scoped_ptr<ShellAudioBus> audio_bus(
      new ShellAudioBus(num_of_dest_channel, kRenderBufferSizeFrames,
                        ShellAudioBus::kFloat32, ShellAudioBus::kPlanar));
  audio_bus->ZeroAllFrames();
  bool silence = true;
  FillAudioBusFromOneSource(num_of_src_channel, num_of_frames, src_data.Pass(),
                            interpretation, audio_bus.get(), &silence);
  EXPECT_FALSE(silence);

  for (size_t c = 0; c < num_of_dest_channel; ++c) {
    for (size_t i = 0; i < kRenderBufferSizeFrames; ++i) {
      if (i < num_of_frames) {
        EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 25);
      } else {
        EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 0);
      }
    }
  }
}

TEST(AudioNodeInputOutputTest, QuadToMonoDiscreteLayoutTest) {
  size_t num_of_src_channel = 4;
  size_t num_of_dest_channel = 1;
  size_t num_of_frames = 25;
  AudioNode::ChannelInterpretation interpretation = AudioNode::kDiscrete;

  float src_data_in_float[100];
  for (size_t c = 0; c < num_of_src_channel; ++c) {
    for (size_t i = 0; i < num_of_frames; ++i) {
      src_data_in_float[c * num_of_frames + i] = 10.0f * (c + 1);
    }
  }

  scoped_array<uint8> src_data(new uint8[400]);
  uint8* src_buffer = src_data.get();
  memcpy(src_buffer, src_data_in_float, 400 * sizeof(uint8));

  scoped_ptr<ShellAudioBus> audio_bus(
      new ShellAudioBus(num_of_dest_channel, kRenderBufferSizeFrames,
                        ShellAudioBus::kFloat32, ShellAudioBus::kPlanar));
  audio_bus->ZeroAllFrames();
  bool silence = true;
  FillAudioBusFromOneSource(num_of_src_channel, num_of_frames, src_data.Pass(),
                            interpretation, audio_bus.get(), &silence);
  EXPECT_FALSE(silence);

  for (size_t c = 0; c < num_of_dest_channel; ++c) {
    for (size_t i = 0; i < kRenderBufferSizeFrames; ++i) {
      if (i < num_of_frames) {
        EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 10);
      } else {
        EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 0);
      }
    }
  }
}

TEST(AudioNodeInputOutputTest, FivePointOneToMonoSpeakersLayoutTest) {
  size_t num_of_src_channel = 6;
  size_t num_of_dest_channel = 1;
  size_t num_of_frames = 10;
  AudioNode::ChannelInterpretation interpretation = AudioNode::kSpeakers;

  float src_data_in_float[60];
  for (size_t c = 0; c < num_of_src_channel; ++c) {
    for (size_t i = 0; i < num_of_frames; ++i) {
      src_data_in_float[c * num_of_frames + i] = 10.0f * (c + 1);
    }
  }

  scoped_array<uint8> src_data(new uint8[240]);
  uint8* src_buffer = src_data.get();
  memcpy(src_buffer, src_data_in_float, 240 * sizeof(uint8));

  scoped_ptr<ShellAudioBus> audio_bus(
      new ShellAudioBus(num_of_dest_channel, kRenderBufferSizeFrames,
                        ShellAudioBus::kFloat32, ShellAudioBus::kPlanar));
  audio_bus->ZeroAllFrames();
  bool silence = true;
  FillAudioBusFromOneSource(num_of_src_channel, num_of_frames, src_data.Pass(),
                            interpretation, audio_bus.get(), &silence);
  EXPECT_FALSE(silence);

  for (size_t c = 0; c < num_of_dest_channel; ++c) {
    for (size_t i = 0; i < kRenderBufferSizeFrames; ++i) {
      if (i < num_of_frames) {
        EXPECT_FLOAT_EQ(audio_bus->GetFloat32Sample(c, i), 106.213f);
      } else {
        EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 0);
      }
    }
  }
}

TEST(AudioNodeInputOutputTest, FivePointOneToMonoDiscreteLayoutTest) {
  size_t num_of_src_channel = 6;
  size_t num_of_dest_channel = 1;
  size_t num_of_frames = 10;
  AudioNode::ChannelInterpretation interpretation = AudioNode::kDiscrete;

  float src_data_in_float[60];
  for (size_t c = 0; c < num_of_src_channel; ++c) {
    for (size_t i = 0; i < num_of_frames; ++i) {
      src_data_in_float[c * num_of_frames + i] = 10.0f * (c + 1);
    }
  }

  scoped_array<uint8> src_data(new uint8[240]);
  uint8* src_buffer = src_data.get();
  memcpy(src_buffer, src_data_in_float, 240 * sizeof(uint8));

  scoped_ptr<ShellAudioBus> audio_bus(
      new ShellAudioBus(num_of_dest_channel, kRenderBufferSizeFrames,
                        ShellAudioBus::kFloat32, ShellAudioBus::kPlanar));
  audio_bus->ZeroAllFrames();
  bool silence = true;
  FillAudioBusFromOneSource(num_of_src_channel, num_of_frames, src_data.Pass(),
                            interpretation, audio_bus.get(), &silence);
  EXPECT_FALSE(silence);

  for (size_t c = 0; c < num_of_dest_channel; ++c) {
    for (size_t i = 0; i < kRenderBufferSizeFrames; ++i) {
      if (i < num_of_frames) {
        EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 10);
      } else {
        EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 0);
      }
    }
  }
}

TEST(AudioNodeInputOutputTest, MultipleInputNodesLayoutTest) {
  size_t num_of_src_channel = 2;
  size_t num_of_dest_channel = 2;
  AudioNode::ChannelInterpretation interpretation = AudioNode::kSpeakers;

  size_t num_of_frames_1 = 25;
  float src_data_in_float_1[50];
  for (size_t c = 0; c < num_of_src_channel; ++c) {
    for (size_t i = 0; i < num_of_frames_1; ++i) {
      src_data_in_float_1[c * num_of_frames_1 + i] = 20.0f * (c + 1);
    }
  }
  scoped_array<uint8> src_data_1(new uint8[200]);
  uint8* src_buffer_1 = src_data_1.get();
  memcpy(src_buffer_1, src_data_in_float_1, 200 * sizeof(uint8));
  scoped_refptr<AudioBufferSourceNode> source_1(
      new AudioBufferSourceNode(NULL));
  scoped_refptr<AudioBuffer> buffer_1(new AudioBuffer(
      NULL, 44100, static_cast<int32>(num_of_frames_1),
      static_cast<int32>(num_of_src_channel), src_data_1.Pass()));
  source_1->set_buffer(buffer_1);

  size_t num_of_frames_2 = 50;
  float src_data_in_float_2[100];
  for (size_t c = 0; c < num_of_src_channel; ++c) {
    for (size_t i = 0; i < num_of_frames_2; ++i) {
      src_data_in_float_2[c * num_of_frames_2 + i] = 40.0f * (c + 1);
    }
  }
  scoped_array<uint8> src_data_2(new uint8[400]);
  uint8* src_buffer_2 = src_data_2.get();
  memcpy(src_buffer_2, src_data_in_float_2, 400 * sizeof(uint8));
  scoped_refptr<AudioBufferSourceNode> source_2(
      new AudioBufferSourceNode(NULL));
  scoped_refptr<AudioBuffer> buffer_2(new AudioBuffer(
      NULL, 44100, static_cast<int32>(num_of_frames_2),
      static_cast<int32>(num_of_src_channel), src_data_2.Pass()));
  source_2->set_buffer(buffer_2);

  scoped_refptr<AudioDestinationNodeMock> destination(
      new AudioDestinationNodeMock(NULL));
  destination->set_channel_interpretation(interpretation);
  source_1->Connect(destination, 0, 0, NULL);
  source_2->Connect(destination, 0, 0, NULL);
  source_1->Start(0, 0, NULL);
  source_2->Start(0, 0, NULL);

  scoped_ptr<ShellAudioBus> audio_bus(
      new ShellAudioBus(num_of_dest_channel, kRenderBufferSizeFrames,
                        ShellAudioBus::kFloat32, ShellAudioBus::kPlanar));
  audio_bus->ZeroAllFrames();
  bool silence = true;
  destination->FillAudioBus(audio_bus.get(), &silence);
  EXPECT_FALSE(silence);

  for (size_t c = 0; c < num_of_dest_channel; ++c) {
    for (size_t i = 0; i < kRenderBufferSizeFrames; ++i) {
      if (i < num_of_frames_1) {
        if (c == 0) {
          EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 60);
        } else {
          EXPECT_EQ(audio_bus->GetFloat32Sample(c, i), 120);
        }
      } else if (i < num_of_frames_2) {
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
