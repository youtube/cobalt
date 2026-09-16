/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/timing/simulator/decodability_tracker.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "api/sequence_checker.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/encoded_frame.h"
#include "rtc_base/checks.h"
#include "test/fake_encoded_frame.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "video/timing/simulator/assembler.h"
#include "video/timing/simulator/test_utils.h"

namespace webrtc::video_timing_simulator {
namespace {

using ::testing::Eq;
using ::testing::InSequence;
using ::testing::NiceMock;

using ::webrtc::test::FakeFrameBuilder;

constexpr int kFrameSizeBytes = 2000;
constexpr int kPayloadType = 96;

class MockDecodabilityTrackerEvents : public DecodabilityTrackerEvents {
 public:
  MOCK_METHOD(void,
              OnDecodableFrame,
              (const EncodedFrame& decodable_frame),
              (override));
};

class MockDecodedFrameIdCallback : public DecodedFrameIdCallback {
 public:
  MOCK_METHOD(void, OnDecodedFrameId, (int64_t frame_id), (override));
};

// Provides `FakeFrameBuilder`s without references set.
class EncodedFrameBuilderGenerator {
 public:
  struct BuilderWithFrameId {
    FakeFrameBuilder builder;
    int64_t frame_id;
  };

  BuilderWithFrameId NextEncodedFrameBuilder() {
    FakeFrameBuilder builder;
    builder = builder.Time(rtp_timestamp_)
                  .Id(frame_id_)
                  .AsLast()
                  .SpatialLayer(0)
                  .ReceivedTime(received_time_)
                  .Size(kFrameSizeBytes)
                  .PayloadType(kPayloadType);
    BuilderWithFrameId ret = {.builder = builder, .frame_id = frame_id_};

    // 30 fps.
    received_time_ += TimeDelta::Millis(33);
    rtp_timestamp_ += 3000;
    ++frame_id_;

    return ret;
  }

 private:
  Timestamp received_time_ = Timestamp::Seconds(10000);
  uint32_t rtp_timestamp_ = 0;
  int64_t frame_id_ = 0;
};

// Simulate https://www.w3.org/TR/webrtc-svc/#L1T1*.
//
//   TL0 |     [F0] ---> [F1] ---> [F2] ---> [F3] ---> [F4] ---> [F5]
//       +----------------------------------------------------------> Time
// Frame:      F0        F1        F2        F3        F4        F5
// Index:      0         1         2         3         4         5
class SingleLayerEncodedFrameGenerator {
 public:
  std::unique_ptr<EncodedFrame> NextEncodedFrame() {
    EncodedFrameBuilderGenerator::BuilderWithFrameId builder_and_frame_id =
        builder_generator_.NextEncodedFrameBuilder();
    if (builder_and_frame_id.frame_id == 0) {
      // Keyframe.
      return builder_and_frame_id.builder.Build();
    }
    return builder_and_frame_id.builder
        .Refs({builder_and_frame_id.frame_id - 1})
        .Build();
  }

 private:
  EncodedFrameBuilderGenerator builder_generator_;
};

TEST(SingleLayerEncodedFrameGeneratorTest, ProducesNonLayeredReferences) {
  SingleLayerEncodedFrameGenerator generator;
  auto keyframe = generator.NextEncodedFrame();
  EXPECT_TRUE(keyframe->is_keyframe());
  for (int i = 1; i < 8; ++i) {
    auto delta_frame = generator.NextEncodedFrame();
    EXPECT_FALSE(delta_frame->is_keyframe());
    EXPECT_THAT(delta_frame->num_references, Eq(1));
    EXPECT_THAT(delta_frame->references[0], Eq(i - 1));
  }
}

class DecodabilityTrackerTest : public SimulatedTimeTestFixture {
 protected:
  DecodabilityTrackerTest() {
    SendTask([this]() {
      RTC_DCHECK_RUN_ON(queue_ptr_);
      decodability_tracker_ = std::make_unique<DecodabilityTracker>(
          env_, &decodability_tracker_events_);
      decodability_tracker_->SetDecodedFrameIdCallback(&decoded_frame_id_cb_);
    });
  }
  ~DecodabilityTrackerTest() {
    SendTask([this]() {
      RTC_DCHECK_RUN_ON(queue_ptr_);
      decodability_tracker_.reset();
    });
  }

  void OnAssembledFrame(std::unique_ptr<EncodedFrame> assembled_frame) {
    SendTask([this, assembled_frame = std::move(assembled_frame)]() mutable {
      RTC_DCHECK_RUN_ON(queue_ptr_);
      decodability_tracker_->OnAssembledFrame(std::move(assembled_frame));
    });
  }

  // Expectations.
  MockDecodabilityTrackerEvents decodability_tracker_events_;
  NiceMock<MockDecodedFrameIdCallback> decoded_frame_id_cb_;

  // Object under test.
  std::unique_ptr<DecodabilityTracker> decodability_tracker_;
};

TEST_F(DecodabilityTrackerTest, KeyframeIsDecodable) {
  SingleLayerEncodedFrameGenerator generator;
  auto keyframe = generator.NextEncodedFrame();

  EXPECT_THAT(keyframe->num_references, Eq(0));
  EXPECT_CALL(decodability_tracker_events_,
              OnDecodableFrame(EncodedFrameWithId(0)));
  EXPECT_CALL(decoded_frame_id_cb_, OnDecodedFrameId(0));
  OnAssembledFrame(std::move(keyframe));
}

TEST_F(DecodabilityTrackerTest, DeltaFrameIsNotDecodable) {
  SingleLayerEncodedFrameGenerator generator;
  auto keyframe = generator.NextEncodedFrame();
  auto delta_frame = generator.NextEncodedFrame();

  EXPECT_THAT(delta_frame->num_references, Eq(1));
  EXPECT_CALL(decodability_tracker_events_,
              OnDecodableFrame(EncodedFrameWithId(0)))
      .Times(0);
  EXPECT_CALL(decoded_frame_id_cb_, OnDecodedFrameId(0)).Times(0);
  OnAssembledFrame(std::move(delta_frame));
}

TEST_F(DecodabilityTrackerTest, KeyframeAndDeltaFrameAreDecodable) {
  SingleLayerEncodedFrameGenerator generator;
  auto keyframe = generator.NextEncodedFrame();
  auto delta_frame = generator.NextEncodedFrame();

  {
    InSequence seq;
    EXPECT_CALL(decodability_tracker_events_,
                OnDecodableFrame(EncodedFrameWithId(0)));
    EXPECT_CALL(decoded_frame_id_cb_, OnDecodedFrameId(0));
    OnAssembledFrame(std::move(keyframe));

    EXPECT_CALL(decodability_tracker_events_,
                OnDecodableFrame(EncodedFrameWithId(1)));
    EXPECT_CALL(decoded_frame_id_cb_, OnDecodedFrameId(1));
    OnAssembledFrame(std::move(delta_frame));
  }
}

TEST_F(DecodabilityTrackerTest, ReorderedKeyframeAndDeltaFrameAreDecodable) {
  SingleLayerEncodedFrameGenerator generator;
  auto keyframe = generator.NextEncodedFrame();
  auto delta_frame = generator.NextEncodedFrame();

  {
    InSequence seq;
    OnAssembledFrame(std::move(delta_frame));
    EXPECT_CALL(decodability_tracker_events_,
                OnDecodableFrame(EncodedFrameWithId(0)));
    EXPECT_CALL(decoded_frame_id_cb_, OnDecodedFrameId(0));
    EXPECT_CALL(decodability_tracker_events_,
                OnDecodableFrame(EncodedFrameWithId(1)));
    EXPECT_CALL(decoded_frame_id_cb_, OnDecodedFrameId(1));
    OnAssembledFrame(std::move(keyframe));
  }
}

// Simulate https://www.w3.org/TR/webrtc-svc/#L1T3*.
//
//   TL2 |         [TL2a]     [TL2b]         [TL2a]
//       |          /          /             /
//       |         /          /             /
//   TL1 |        /       [TL1]            /
//       |       /          /             /
//       |      /          /             /
//   TL0 |     [K]----------------------[TL0]
//       +-------------------------------------------> Time
// Frame:      K   TL2a    TL1 TL2b      TL0 TL2a
// Index:      0   1       2   3         4   5
class TemporalLayersEncodedFrameGenerator {
 public:
  static constexpr int kNumTemporalLayers = 4;

  std::unique_ptr<EncodedFrame> NextEncodedFrame() {
    EncodedFrameBuilderGenerator::BuilderWithFrameId builder_and_frame_id =
        builder_generator_.NextEncodedFrameBuilder();
    int64_t frame_id = builder_and_frame_id.frame_id;
    if (frame_id == 0) {
      // Keyframe.
      return builder_and_frame_id.builder.Build();
    }
    FakeFrameBuilder builder = builder_and_frame_id.builder;
    int64_t mod = frame_id % kNumTemporalLayers;
    if (mod == 0) {
      // The keyframe is handled above.
      RTC_CHECK_GE(frame_id, 4);
      return builder.Refs({frame_id - 4}).Build();
    } else if (mod == 1) {
      RTC_CHECK_GE(frame_id, 1);
      return builder.Refs({frame_id - 1}).Build();
    } else if (mod == 2) {
      RTC_CHECK_GE(frame_id, 2);
      return builder.Refs({frame_id - 2}).Build();
    } else if (mod == 3) {
      RTC_CHECK_GE(frame_id, 3);
      return builder.Refs({frame_id - 1}).Build();
    }
    RTC_CHECK_NOTREACHED();
    return FakeFrameBuilder().Build();
  }

 private:
  EncodedFrameBuilderGenerator builder_generator_;
};

TEST(TemporalLayersEncodedFrameGenerator, ProducesLayeredReferences) {
  TemporalLayersEncodedFrameGenerator generator;

  auto keyframe = generator.NextEncodedFrame();
  EXPECT_TRUE(keyframe->is_keyframe());

  auto tl2a = generator.NextEncodedFrame();
  EXPECT_FALSE(tl2a->is_keyframe());
  EXPECT_THAT(tl2a->references[0], Eq(0));

  auto tl1 = generator.NextEncodedFrame();
  EXPECT_FALSE(tl1->is_keyframe());
  EXPECT_THAT(tl1->references[0], Eq(0));

  auto tl2b = generator.NextEncodedFrame();
  EXPECT_FALSE(tl2b->is_keyframe());
  EXPECT_THAT(tl2b->references[0], Eq(2));

  auto tl0_next_gop = generator.NextEncodedFrame();
  EXPECT_FALSE(tl0_next_gop->is_keyframe());
  EXPECT_THAT(tl0_next_gop->references[0], Eq(0));
}

TEST_F(DecodabilityTrackerTest, OneTemporalLayerGoPIsDecodable) {
  TemporalLayersEncodedFrameGenerator generator;
  auto keyframe = generator.NextEncodedFrame();
  auto tl2a = generator.NextEncodedFrame();
  auto tl1 = generator.NextEncodedFrame();
  auto tl2b = generator.NextEncodedFrame();

  {
    InSequence seq;
    EXPECT_CALL(decodability_tracker_events_,
                OnDecodableFrame(EncodedFrameWithId(0)));
    OnAssembledFrame(std::move(keyframe));

    EXPECT_CALL(decodability_tracker_events_,
                OnDecodableFrame(EncodedFrameWithId(1)));
    OnAssembledFrame(std::move(tl2a));

    EXPECT_CALL(decodability_tracker_events_,
                OnDecodableFrame(EncodedFrameWithId(2)));
    OnAssembledFrame(std::move(tl1));

    EXPECT_CALL(decodability_tracker_events_,
                OnDecodableFrame(EncodedFrameWithId(3)));
    OnAssembledFrame(std::move(tl2b));
  }
}

TEST_F(DecodabilityTrackerTest, TwoTemporalLayerGoPsAreDecodable) {
  TemporalLayersEncodedFrameGenerator generator;
  {
    InSequence seq;
    for (int i = 0; i < 8; ++i) {
      EXPECT_CALL(decodability_tracker_events_,
                  OnDecodableFrame(EncodedFrameWithId(i)));
      OnAssembledFrame(generator.NextEncodedFrame());
    }
  }
}

// TODO: b/423646186 - Update this test when we handle reordered frames better.
TEST_F(DecodabilityTrackerTest, SkipsOverReorderedTl2A) {
  TemporalLayersEncodedFrameGenerator generator;
  auto keyframe = generator.NextEncodedFrame();
  auto tl2a = generator.NextEncodedFrame();
  auto tl1 = generator.NextEncodedFrame();
  auto tl2b = generator.NextEncodedFrame();

  {
    InSequence seq;
    EXPECT_CALL(decodability_tracker_events_,
                OnDecodableFrame(EncodedFrameWithId(0)));
    OnAssembledFrame(std::move(keyframe));

    EXPECT_CALL(decodability_tracker_events_,
                OnDecodableFrame(EncodedFrameWithId(2)));
    OnAssembledFrame(std::move(tl1));

    EXPECT_CALL(decodability_tracker_events_,
                OnDecodableFrame(EncodedFrameWithId(1)))
        .Times(0);
    OnAssembledFrame(std::move(tl2a));

    EXPECT_CALL(decodability_tracker_events_,
                OnDecodableFrame(EncodedFrameWithId(3)));
    OnAssembledFrame(std::move(tl2b));
  }
}

TEST_F(DecodabilityTrackerTest, DoesNotSkipOverReorderedTl1) {
  TemporalLayersEncodedFrameGenerator generator;
  auto keyframe = generator.NextEncodedFrame();
  auto tl2a = generator.NextEncodedFrame();
  auto tl1 = generator.NextEncodedFrame();
  auto tl2b = generator.NextEncodedFrame();

  {
    InSequence seq;
    EXPECT_CALL(decodability_tracker_events_,
                OnDecodableFrame(EncodedFrameWithId(0)));
    OnAssembledFrame(std::move(keyframe));

    EXPECT_CALL(decodability_tracker_events_,
                OnDecodableFrame(EncodedFrameWithId(1)));
    OnAssembledFrame(std::move(tl2a));

    OnAssembledFrame(std::move(tl2b));

    EXPECT_CALL(decodability_tracker_events_,
                OnDecodableFrame(EncodedFrameWithId(2)));
    EXPECT_CALL(decodability_tracker_events_,
                OnDecodableFrame(EncodedFrameWithId(3)));
    OnAssembledFrame(std::move(tl1));
  }
}

// TODO: b/423646186 - Update this test when we handle reordered frames better.
TEST_F(DecodabilityTrackerTest, SkipsOverReorderedTl2B) {
  TemporalLayersEncodedFrameGenerator generator;
  auto keyframe = generator.NextEncodedFrame();
  auto tl2a = generator.NextEncodedFrame();
  auto tl1 = generator.NextEncodedFrame();
  auto tl2b = generator.NextEncodedFrame();
  auto tl0_next_gop = generator.NextEncodedFrame();

  {
    InSequence seq;
    EXPECT_CALL(decodability_tracker_events_,
                OnDecodableFrame(EncodedFrameWithId(0)));
    OnAssembledFrame(std::move(keyframe));

    EXPECT_CALL(decodability_tracker_events_,
                OnDecodableFrame(EncodedFrameWithId(1)));
    OnAssembledFrame(std::move(tl2a));

    EXPECT_CALL(decodability_tracker_events_,
                OnDecodableFrame(EncodedFrameWithId(2)));
    OnAssembledFrame(std::move(tl1));

    EXPECT_CALL(decodability_tracker_events_,
                OnDecodableFrame(EncodedFrameWithId(4)));
    OnAssembledFrame(std::move(tl0_next_gop));

    EXPECT_CALL(decodability_tracker_events_,
                OnDecodableFrame(EncodedFrameWithId(3)))
        .Times(0);
    OnAssembledFrame(std::move(tl2b));
  }
}

}  // namespace
}  // namespace webrtc::video_timing_simulator
