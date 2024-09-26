// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/breakout_box/media_stream_audio_track_underlying_sink.h"

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_audio_sink.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_data.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_writer.h"
#include "third_party/blink/renderer/modules/breakout_box/pushable_media_stream_audio_source.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_audio_sink.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/to_v8.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component_impl.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"

using testing::_;
using testing::StrictMock;

namespace blink {

class MediaStreamAudioTrackUnderlyingSinkTest : public testing::Test {
 public:
  MediaStreamAudioTrackUnderlyingSinkTest() {
    // Use the IO thread for testing purposes, instead of an audio task runner.
    auto pushable_audio_source =
        std::make_unique<PushableMediaStreamAudioSource>(
            scheduler::GetSingleThreadTaskRunnerForTesting(),
            platform_->GetIOTaskRunner());
    pushable_audio_source_ = pushable_audio_source.get();
    media_stream_source_ = MakeGarbageCollected<MediaStreamSource>(
        "dummy_source_id", MediaStreamSource::kTypeAudio, "dummy_source_name",
        /*remote=*/false, std::move(pushable_audio_source));
  }

  ~MediaStreamAudioTrackUnderlyingSinkTest() override {
    platform_->RunUntilIdle();
    WebHeap::CollectAllGarbageForTesting();
  }

  MediaStreamAudioTrackUnderlyingSink* CreateUnderlyingSink(
      ScriptState* script_state) {
    return MakeGarbageCollected<MediaStreamAudioTrackUnderlyingSink>(
        pushable_audio_source_->GetBroker());
  }

  void CreateTrackAndConnectToSource() {
    media_stream_component_ = MakeGarbageCollected<MediaStreamComponentImpl>(
        media_stream_source_->Id(), media_stream_source_,
        std::make_unique<MediaStreamAudioTrack>(true /* is_local_track */));
    pushable_audio_source_->ConnectToInitializedTrack(media_stream_component_);
  }

  ScriptValue CreateAudioData(ScriptState* script_state,
                              AudioData** audio_data_out = nullptr) {
    const scoped_refptr<media::AudioBuffer> media_buffer =
        media::AudioBuffer::CreateEmptyBuffer(
            media::ChannelLayout::CHANNEL_LAYOUT_STEREO,
            /*channel_count=*/2,
            /*sample_rate=*/44100,
            /*frame_count=*/500, base::TimeDelta());
    AudioData* audio_data =
        MakeGarbageCollected<AudioData>(std::move(media_buffer));
    if (audio_data_out)
      *audio_data_out = audio_data;
    return ScriptValue(
        script_state->GetIsolate(),
        ToV8Traits<AudioData>::ToV8(script_state, audio_data).ToLocalChecked());
  }

 protected:
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;
  Persistent<MediaStreamSource> media_stream_source_;
  Persistent<MediaStreamComponent> media_stream_component_;

  PushableMediaStreamAudioSource* pushable_audio_source_;
};

TEST_F(MediaStreamAudioTrackUnderlyingSinkTest,
       WriteToStreamForwardsToMediaStreamSink) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* underlying_sink = CreateUnderlyingSink(script_state);
  auto* writable_stream = WritableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_sink, 1u);

  CreateTrackAndConnectToSource();

  base::RunLoop write_loop;
  StrictMock<MockMediaStreamAudioSink> mock_sink;
  EXPECT_CALL(mock_sink, OnSetFormat(_)).Times(::testing::AnyNumber());
  EXPECT_CALL(mock_sink, OnData(_, _))
      .WillOnce(base::test::RunOnceClosure(write_loop.QuitClosure()));

  WebMediaStreamAudioSink::AddToAudioTrack(
      &mock_sink, WebMediaStreamTrack(media_stream_component_.Get()));

  NonThrowableExceptionState exception_state;
  auto* writer = writable_stream->getWriter(script_state, exception_state);

  AudioData* audio_data = nullptr;
  auto audio_data_chunk = CreateAudioData(script_state, &audio_data);
  EXPECT_NE(audio_data, nullptr);
  EXPECT_NE(audio_data->data(), nullptr);
  ScriptPromiseTester write_tester(
      script_state,
      writer->write(script_state, audio_data_chunk, exception_state));
  // |audio_data| should be invalidated after sending it to the sink.
  write_tester.WaitUntilSettled();
  EXPECT_EQ(audio_data->data(), nullptr);
  write_loop.Run();

  writer->releaseLock(script_state);
  ScriptPromiseTester close_tester(
      script_state, writable_stream->close(script_state, exception_state));
  close_tester.WaitUntilSettled();

  // Writing to the sink after the stream closes should fail.
  DummyExceptionStateForTesting dummy_exception_state;
  underlying_sink->write(script_state, CreateAudioData(script_state), nullptr,
                         dummy_exception_state);
  EXPECT_TRUE(dummy_exception_state.HadException());
  EXPECT_EQ(dummy_exception_state.Code(),
            static_cast<ExceptionCode>(DOMExceptionCode::kInvalidStateError));

  WebMediaStreamAudioSink::RemoveFromAudioTrack(
      &mock_sink, WebMediaStreamTrack(media_stream_component_.Get()));
}

TEST_F(MediaStreamAudioTrackUnderlyingSinkTest, WriteInvalidDataFails) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* sink = CreateUnderlyingSink(script_state);
  ScriptValue v8_integer = ScriptValue::From(script_state, 0);

  // Writing something that is not an AudioData to the sink should fail.
  DummyExceptionStateForTesting dummy_exception_state;
  sink->write(script_state, v8_integer, nullptr, dummy_exception_state);
  EXPECT_TRUE(dummy_exception_state.HadException());

  // Writing a null value to the sink should fail.
  dummy_exception_state.ClearException();
  EXPECT_FALSE(dummy_exception_state.HadException());
  sink->write(script_state, ScriptValue::CreateNull(v8_scope.GetIsolate()),
              nullptr, dummy_exception_state);
  EXPECT_TRUE(dummy_exception_state.HadException());

  // Writing a closed AudioData to the sink should fail.
  dummy_exception_state.ClearException();
  AudioData* audio_data = nullptr;
  auto chunk = CreateAudioData(script_state, &audio_data);
  audio_data->close();
  EXPECT_FALSE(dummy_exception_state.HadException());
  sink->write(script_state, chunk, nullptr, dummy_exception_state);
  EXPECT_TRUE(dummy_exception_state.HadException());
}

TEST_F(MediaStreamAudioTrackUnderlyingSinkTest, WriteToAbortedSinkFails) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* underlying_sink = CreateUnderlyingSink(script_state);
  auto* writable_stream = WritableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_sink, 1u);

  NonThrowableExceptionState exception_state;
  ScriptPromiseTester abort_tester(
      script_state, writable_stream->abort(script_state, exception_state));
  abort_tester.WaitUntilSettled();

  // Writing to the sink after the stream closes should fail.
  DummyExceptionStateForTesting dummy_exception_state;
  underlying_sink->write(script_state, CreateAudioData(script_state), nullptr,
                         dummy_exception_state);
  EXPECT_TRUE(dummy_exception_state.HadException());
  EXPECT_EQ(dummy_exception_state.Code(),
            static_cast<ExceptionCode>(DOMExceptionCode::kInvalidStateError));
}

}  // namespace blink
