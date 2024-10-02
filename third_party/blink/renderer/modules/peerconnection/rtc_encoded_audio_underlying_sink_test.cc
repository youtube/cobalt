// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_underlying_sink.h"

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_encoded_audio_frame.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_writer.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_frame.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_frame_delegate.h"
#include "third_party/blink/renderer/modules/peerconnection/testing/mock_transformable_audio_frame.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_encoded_audio_stream_transformer.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/webrtc/api/frame_transformer_interface.h"
#include "third_party/webrtc/api/scoped_refptr.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;

namespace blink {

namespace {

class MockWebRtcTransformedFrameCallback
    : public webrtc::TransformedFrameCallback {
 public:
  MOCK_METHOD1(OnTransformedFrame,
               void(std::unique_ptr<webrtc::TransformableFrameInterface>));
};

class FakeAudioFrame : public webrtc::TransformableFrameInterface {
 public:
  rtc::ArrayView<const uint8_t> GetData() const override {
    return rtc::ArrayView<const uint8_t>();
  }

  void SetData(rtc::ArrayView<const uint8_t> data) override {}
  uint32_t GetTimestamp() const override { return 0xDEADBEEF; }
  uint32_t GetSsrc() const override { return 0; }
  uint8_t GetPayloadType() const override { return 255; }
};

}  // namespace

class RTCEncodedAudioUnderlyingSinkTest : public testing::Test {
 public:
  RTCEncodedAudioUnderlyingSinkTest()
      : main_task_runner_(
            blink::scheduler::GetSingleThreadTaskRunnerForTesting()),
        webrtc_callback_(
            new rtc::RefCountedObject<MockWebRtcTransformedFrameCallback>()),
        transformer_(main_task_runner_) {}

  void SetUp() override {
    EXPECT_FALSE(transformer_.HasTransformedFrameCallback());
    transformer_.RegisterTransformedFrameCallback(webrtc_callback_);
    EXPECT_TRUE(transformer_.HasTransformedFrameCallback());
  }

  void TearDown() override {
    platform_->RunUntilIdle();
    transformer_.UnregisterTransformedFrameCallback();
    EXPECT_FALSE(transformer_.HasTransformedFrameCallback());
  }

  RTCEncodedAudioUnderlyingSink* CreateSink(
      ScriptState* script_state,
      webrtc::TransformableFrameInterface::Direction expected_direction =
          webrtc::TransformableFrameInterface::Direction::kSender) {
    return MakeGarbageCollected<RTCEncodedAudioUnderlyingSink>(
        script_state, transformer_.GetBroker(), expected_direction);
  }

  RTCEncodedAudioStreamTransformer* GetTransformer() { return &transformer_; }

  RTCEncodedAudioFrame* CreateEncodedAudioFrame(
      ScriptState* script_state,
      webrtc::TransformableFrameInterface::Direction direction =
          webrtc::TransformableFrameInterface::Direction::kSender) {
    auto mock_frame = std::make_unique<NiceMock<MockTransformableAudioFrame>>();
    ON_CALL(*mock_frame.get(), GetDirection).WillByDefault(Return(direction));
    webrtc::RTPHeader header;
    ON_CALL(*mock_frame.get(), GetHeader).WillByDefault(ReturnRef(header));
    std::unique_ptr<webrtc::TransformableAudioFrameInterface> audio_frame =
        base::WrapUnique(static_cast<webrtc::TransformableAudioFrameInterface*>(
            mock_frame.release()));
    return MakeGarbageCollected<RTCEncodedAudioFrame>(std::move(audio_frame));
  }

  ScriptValue CreateEncodedAudioFrameChunk(
      ScriptState* script_state,
      webrtc::TransformableFrameInterface::Direction direction =
          webrtc::TransformableFrameInterface::Direction::kSender) {
    return ScriptValue(
        script_state->GetIsolate(),
        ToV8Traits<RTCEncodedAudioFrame>::ToV8(
            script_state, CreateEncodedAudioFrame(script_state, direction))
            .ToLocalChecked());
  }

 protected:
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  rtc::scoped_refptr<MockWebRtcTransformedFrameCallback> webrtc_callback_;
  RTCEncodedAudioStreamTransformer transformer_;
};

TEST_F(RTCEncodedAudioUnderlyingSinkTest,
       WriteToStreamForwardsToWebRtcCallback) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* sink = CreateSink(script_state);
  auto* stream =
      WritableStream::CreateWithCountQueueingStrategy(script_state, sink, 1u);

  NonThrowableExceptionState exception_state;
  auto* writer = stream->getWriter(script_state, exception_state);

  EXPECT_CALL(*webrtc_callback_, OnTransformedFrame(_));
  ScriptPromiseTester write_tester(
      script_state,
      writer->write(script_state, CreateEncodedAudioFrameChunk(script_state),
                    exception_state));
  EXPECT_FALSE(write_tester.IsFulfilled());

  writer->releaseLock(script_state);
  ScriptPromiseTester close_tester(
      script_state, stream->close(script_state, exception_state));
  close_tester.WaitUntilSettled();

  // Writing to the sink after the stream closes should fail.
  DummyExceptionStateForTesting dummy_exception_state;
  sink->write(script_state, CreateEncodedAudioFrameChunk(script_state),
              /*controller=*/nullptr, dummy_exception_state);
  EXPECT_TRUE(dummy_exception_state.HadException());
  EXPECT_EQ(dummy_exception_state.Code(),
            static_cast<ExceptionCode>(DOMExceptionCode::kInvalidStateError));
}

TEST_F(RTCEncodedAudioUnderlyingSinkTest, WriteInvalidDataFails) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* sink = CreateSink(script_state);
  ScriptValue v8_integer = ScriptValue::From(script_state, 0);

  // Writing something that is not an RTCEncodedAudioFrame integer to the sink
  // should fail.
  DummyExceptionStateForTesting dummy_exception_state;
  sink->write(script_state, v8_integer, /*controller=*/nullptr,
              dummy_exception_state);
  EXPECT_TRUE(dummy_exception_state.HadException());
}

TEST_F(RTCEncodedAudioUnderlyingSinkTest, WriteInvalidDirectionFails) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* sink = CreateSink(
      script_state, webrtc::TransformableFrameInterface::Direction::kSender);

  // Write an encoded chunk with direction set to Receiver should fail as it
  // doesn't match the expected direction of our sink.
  DummyExceptionStateForTesting dummy_exception_state;
  sink->write(script_state,
              CreateEncodedAudioFrameChunk(
                  script_state,
                  webrtc::TransformableFrameInterface::Direction::kReceiver),
              /*controller=*/nullptr, dummy_exception_state);
  EXPECT_TRUE(dummy_exception_state.HadException());
}

TEST_F(RTCEncodedAudioUnderlyingSinkTest,
       WriteLargeButNotTooLargeFrameSucceeds) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* sink = CreateSink(
      script_state, webrtc::TransformableFrameInterface::Direction::kSender);
  RTCEncodedAudioFrame* frame = CreateEncodedAudioFrame(script_state);
  frame->setData(
      DOMArrayBuffer::Create(/*num_elements=*/1000, /*element_byte_size=*/1));

  DummyExceptionStateForTesting dummy_exception_state;
  EXPECT_CALL(*webrtc_callback_, OnTransformedFrame(_));
  sink->write(
      script_state,
      ScriptValue(script_state->GetIsolate(),
                  ToV8Traits<RTCEncodedAudioFrame>::ToV8(script_state, frame)
                      .ToLocalChecked()),
      /*controller=*/nullptr, dummy_exception_state);
  EXPECT_FALSE(dummy_exception_state.HadException());
}

TEST_F(RTCEncodedAudioUnderlyingSinkTest, WriteTooLargeFrameFails) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* sink = CreateSink(
      script_state, webrtc::TransformableFrameInterface::Direction::kSender);
  RTCEncodedAudioFrame* frame = CreateEncodedAudioFrame(script_state);
  // Set too much data on the frame.
  frame->setData(
      DOMArrayBuffer::Create(/*num_elements=*/1001, /*element_byte_size=*/1));

  DummyExceptionStateForTesting dummy_exception_state;
  sink->write(
      script_state,
      ScriptValue(script_state->GetIsolate(),
                  ToV8Traits<RTCEncodedAudioFrame>::ToV8(script_state, frame)
                      .ToLocalChecked()),
      /*controller=*/nullptr, dummy_exception_state);
  EXPECT_TRUE(dummy_exception_state.HadException());
}

}  // namespace blink
