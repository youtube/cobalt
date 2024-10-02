// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_underlying_source.h"
#include <memory>

#include "base/test/mock_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_encoded_audio_frame.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_frame.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/webrtc/api/frame_transformer_interface.h"

namespace blink {

namespace {
class FakeTransformableFrame : public webrtc::TransformableFrameInterface {
 public:
  FakeTransformableFrame() = default;
  ~FakeTransformableFrame() override = default;

  rtc::ArrayView<const uint8_t> GetData() const override { return nullptr; }
  void SetData(rtc::ArrayView<const uint8_t> data) override {}
  uint32_t GetTimestamp() const override { return 0; }
  uint32_t GetSsrc() const override { return 0; }
  // 255 is not a valid payload type (which can be in the range [0..127]).
  uint8_t GetPayloadType() const override { return 255; }
};
}  // namespace

class RTCEncodedAudioUnderlyingSourceTest : public testing::Test {
 public:
  RTCEncodedAudioUnderlyingSource* CreateSource(ScriptState* script_state,
                                                bool is_receiver = false) {
    return MakeGarbageCollected<RTCEncodedAudioUnderlyingSource>(
        script_state, WTF::CrossThreadBindOnce(disconnect_callback_.Get()),
        is_receiver);
  }

 protected:
  base::MockOnceClosure disconnect_callback_;
};

TEST_F(RTCEncodedAudioUnderlyingSourceTest,
       SourceDataFlowsThroughStreamAndCloses) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* source = CreateSource(script_state);
  auto* stream =
      ReadableStream::CreateWithCountQueueingStrategy(script_state, source, 0);

  NonThrowableExceptionState exception_state;
  auto* reader =
      stream->GetDefaultReaderForTesting(script_state, exception_state);

  ScriptPromiseTester read_tester(script_state,
                                  reader->read(script_state, exception_state));
  EXPECT_FALSE(read_tester.IsFulfilled());
  source->OnFrameFromSource(std::make_unique<FakeTransformableFrame>());
  read_tester.WaitUntilSettled();
  EXPECT_TRUE(read_tester.IsFulfilled());

  EXPECT_CALL(disconnect_callback_, Run());
  source->Close();
}

TEST_F(RTCEncodedAudioUnderlyingSourceTest, CancelStream) {
  V8TestingScope v8_scope;
  auto* source = CreateSource(v8_scope.GetScriptState());
  auto* stream = ReadableStream::CreateWithCountQueueingStrategy(
      v8_scope.GetScriptState(), source, 0);

  EXPECT_CALL(disconnect_callback_, Run());
  NonThrowableExceptionState exception_state;
  stream->cancel(v8_scope.GetScriptState(), exception_state);
}

TEST_F(RTCEncodedAudioUnderlyingSourceTest,
       QueuedFramesAreDroppedWhenOverflow) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* source = CreateSource(script_state);
  // Create a stream, to ensure there is a controller associated to the source.
  ReadableStream::CreateWithCountQueueingStrategy(v8_scope.GetScriptState(),
                                                  source, 0);
  for (int i = 0; i > RTCEncodedAudioUnderlyingSource::kMinQueueDesiredSize;
       --i) {
    EXPECT_EQ(source->Controller()->DesiredSize(), i);
    source->OnFrameFromSource(std::make_unique<FakeTransformableFrame>());
  }
  EXPECT_EQ(source->Controller()->DesiredSize(),
            RTCEncodedAudioUnderlyingSource::kMinQueueDesiredSize);

  source->OnFrameFromSource(std::make_unique<FakeTransformableFrame>());
  EXPECT_EQ(source->Controller()->DesiredSize(),
            RTCEncodedAudioUnderlyingSource::kMinQueueDesiredSize);

  EXPECT_CALL(disconnect_callback_, Run());
  source->Close();
}

}  // namespace blink
