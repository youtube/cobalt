// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/media_capture/media_recorder.h"

#include <memory>

#include "cobalt/dom/dom_exception.h"
#include "cobalt/dom/testing/mock_event_listener.h"
#include "cobalt/dom/testing/stub_window.h"
#include "cobalt/media_capture/media_recorder_options.h"
#include "cobalt/media_stream/media_stream.h"
#include "cobalt/media_stream/media_stream_audio_source.h"
#include "cobalt/media_stream/media_stream_audio_track.h"
#include "cobalt/media_stream/testing/mock_media_stream_audio_track.h"
#include "cobalt/script/testing/fake_script_value.h"
#include "cobalt/script/testing/mock_exception_state.h"
#include "testing/gtest/include/gtest/gtest.h"

using cobalt::dom::EventListener;
using cobalt::dom::testing::MockEventListener;
using cobalt::script::testing::FakeScriptValue;
using cobalt::script::testing::MockExceptionState;
using ::testing::_;
using ::testing::Eq;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::SaveArg;
using ::testing::StrictMock;

namespace {
void PushData(cobalt::media_capture::MediaRecorder* media_recorder) {
  const int kSampleRate = 16000;
  cobalt::media_stream::AudioParameters params(1, kSampleRate, 16);
  media_recorder->OnSetFormat(params);

  std::vector<int16> frames;
  frames.push_back(1);
  frames.push_back(-1);
  frames.push_back(-32768);
  frames.push_back(32767);
  frames.push_back(1000);
  frames.push_back(0);
  cobalt::media_stream::MediaStreamAudioTrack::AudioBus audio_bus(
      1, frames.size(), frames.data());
  base::TimeTicks current_time = base::TimeTicks::Now();
  media_recorder->OnData(audio_bus, current_time);
}

}  // namespace

namespace cobalt {
namespace media_stream {

class FakeMediaStreamAudioSource : public MediaStreamAudioSource {
 public:
  MOCK_METHOD0(EnsureSourceIsStarted, bool());
  MOCK_METHOD0(EnsureSourceIsStopped, void());

  void DeliverDataToTracks(const MediaStreamAudioTrack::AudioBus& audio_bus,
                           base::TimeTicks reference_time) {
    MediaStreamAudioSource::DeliverDataToTracks(audio_bus, reference_time);
  }

  void SetFormat(const media_stream::AudioParameters& params) {
    MediaStreamAudioSource::SetFormat(params);
  }
};

}  // namespace media_stream

namespace media_capture {

class MediaRecorderTest : public ::testing::Test {
 protected:
  MediaRecorderTest() {
    audio_track_ = new StrictMock<media_stream::MockMediaStreamAudioTrack>(
        stub_window_.environment_settings());
    auto audio_track = base::WrapRefCounted(audio_track_);
    media_stream::MediaStream::TrackSequences sequences;
    sequences.push_back(audio_track);
    audio_track->Start(base::Closure(base::Bind([]() {} /*Do nothing*/)));
    auto stream = base::WrapRefCounted(new media_stream::MediaStream(
        stub_window_.environment_settings(), sequences));
    media_source_ = new StrictMock<media_stream::FakeMediaStreamAudioSource>();
    EXPECT_CALL(*media_source_, EnsureSourceIsStarted());
    EXPECT_CALL(*media_source_, EnsureSourceIsStopped());
    media_source_->ConnectToTrack(audio_track);
    media_recorder_ =
        new MediaRecorder(stub_window_.environment_settings(), stream,
                          MediaRecorderOptions(), &exception_state_);
  }

 protected:
  dom::testing::StubWindow stub_window_;
  scoped_refptr<media_capture::MediaRecorder> media_recorder_;
  StrictMock<media_stream::MockMediaStreamAudioTrack>* audio_track_;
  scoped_refptr<StrictMock<media_stream::FakeMediaStreamAudioSource>>
      media_source_;
  StrictMock<MockExceptionState> exception_state_;
};

TEST_F(MediaRecorderTest, CanSupportMimeType) {
  EXPECT_TRUE(MediaRecorder::IsTypeSupported("audio/L16"));
  EXPECT_TRUE(MediaRecorder::IsTypeSupported("audio/l16"));
  EXPECT_TRUE(
      MediaRecorder::IsTypeSupported("audio/l16;endianness=little-endian"));

  EXPECT_FALSE(MediaRecorder::IsTypeSupported("audio/pcm"));
  EXPECT_FALSE(MediaRecorder::IsTypeSupported("audio/webm"));
  EXPECT_FALSE(MediaRecorder::IsTypeSupported("pcm"));
  EXPECT_FALSE(MediaRecorder::IsTypeSupported("L16"));
  EXPECT_FALSE(MediaRecorder::IsTypeSupported("L16"));
}

TEST_F(MediaRecorderTest, StartStopStartStop) {
  EXPECT_CALL(*audio_track_, Stop()).Times(2);
  media_recorder_->Start(&exception_state_);
  media_recorder_->Stop(&exception_state_);
  media_recorder_->Start(&exception_state_);
  media_recorder_->Stop(&exception_state_);
}

TEST_F(MediaRecorderTest, ExceptionOnStartingTwiceWithoutStop) {
  scoped_refptr<script::ScriptException> exception;

  EXPECT_CALL(exception_state_, SetException(_))
      .WillOnce(SaveArg<0>(&exception));

  media_recorder_->Start(&exception_state_);
  media_recorder_->Start(&exception_state_);

  ASSERT_TRUE(exception.get());
  dom::DOMException& dom_exception(
      *base::polymorphic_downcast<dom::DOMException*>(exception.get()));
  EXPECT_EQ(dom::DOMException::kInvalidStateErr, dom_exception.code());
  EXPECT_EQ(dom_exception.message(), "Recording state must be inactive.");
  EXPECT_CALL(*audio_track_, Stop());
}

TEST_F(MediaRecorderTest, ExceptionOnStoppingTwiceWithoutStartingInBetween) {
  scoped_refptr<script::ScriptException> exception;

  EXPECT_CALL(exception_state_, SetException(_))
      .WillOnce(SaveArg<0>(&exception));

  media_recorder_->Start(&exception_state_);
  EXPECT_CALL(*audio_track_, Stop());
  media_recorder_->Stop(&exception_state_);
  media_recorder_->Stop(&exception_state_);

  ASSERT_TRUE(exception.get());
  dom::DOMException& dom_exception(
      *base::polymorphic_downcast<dom::DOMException*>(exception.get()));
  EXPECT_EQ(dom::DOMException::kInvalidStateErr, dom_exception.code());
  EXPECT_EQ(dom_exception.message(), "Recording state must NOT be inactive.");
}

TEST_F(MediaRecorderTest, RecordL16Frames) {
  std::unique_ptr<MockEventListener> listener = MockEventListener::Create();
  FakeScriptValue<EventListener> script_object(listener.get());
  media_recorder_->set_ondataavailable(script_object);
  EXPECT_CALL(*listener,
              HandleEvent(Eq(media_recorder_),
                          Pointee(Property(&dom::Event::type,
                                           base::Tokens::dataavailable())),
                          _));

  media_recorder_->Start(&exception_state_);

  const int kSampleRate = 16000;
  media_stream::AudioParameters params(1, kSampleRate, 16);
  media_recorder_->OnSetFormat(params);

  std::vector<int16> frames;
  frames.push_back(1);
  frames.push_back(-1);
  frames.push_back(-32768);
  frames.push_back(32767);
  frames.push_back(1000);
  frames.push_back(0);
  media_stream::MediaStreamAudioTrack::AudioBus audio_bus(1, frames.size(),
                                                          frames.data());
  base::TimeTicks current_time = base::TimeTicks::Now();
  media_recorder_->OnData(audio_bus, current_time);
  current_time += base::TimeDelta::FromSecondsD(frames.size() / kSampleRate);
  media_recorder_->OnData(audio_bus, current_time);
  current_time += base::TimeDelta::FromSecondsD(frames.size() / kSampleRate);
  media_recorder_->OnData(audio_bus, current_time);

  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(*audio_track_, Stop());
  media_recorder_->Stop(&exception_state_);
}

TEST_F(MediaRecorderTest, DifferentThreadForAudioSource) {
  std::unique_ptr<MockEventListener> listener = MockEventListener::Create();
  FakeScriptValue<EventListener> script_object(listener.get());
  media_recorder_->set_ondataavailable(script_object);
  EXPECT_CALL(*listener,
              HandleEvent(Eq(media_recorder_),
                          Pointee(Property(&dom::Event::type,
                                           base::Tokens::dataavailable())),
                          _));

  media_recorder_->Start(&exception_state_);

  base::Thread t("MediaStrmAudio");
  t.Start();
  // media_recorder_ is a ref-counted object, binding it to PushData that will
  // later be executed on another thread violates the thread-unsafe assumption
  // of a ref-counted object; base::Bind also prohibits binding ref-counted
  // object using raw pointer. So we have to use scoped_refptr<MediaRecorder>&
  // to access media_recorder from another thread. In non-test code, accessing
  // MediaRecorder from non-javascript thread can only be done by binding its
  // member functions with base::Unretained() or weak pointer.
  // Creates media_recorder_ref just to make it clear that no copy happened
  // during base::Bind().
  t.message_loop()->task_runner()->PostBlockingTask(
      FROM_HERE,
      base::Bind(&PushData, base::Unretained(media_recorder_.get())));
  t.Stop();

  base::RunLoop().RunUntilIdle();
  EXPECT_CALL(*audio_track_, Stop());
  media_recorder_->Stop(&exception_state_);
}

TEST_F(MediaRecorderTest, StartEvent) {
  std::unique_ptr<MockEventListener> listener = MockEventListener::Create();
  FakeScriptValue<EventListener> script_object(listener.get());
  media_recorder_->set_onstart(script_object);
  EXPECT_CALL(
      *listener,
      HandleEvent(Eq(media_recorder_),
                  Pointee(Property(&dom::Event::type, base::Tokens::start())),
                  _));

  media_recorder_->Start(&exception_state_);
  EXPECT_CALL(*audio_track_, Stop());
}

TEST_F(MediaRecorderTest, StopEvent) {
  std::unique_ptr<MockEventListener> listener = MockEventListener::Create();
  FakeScriptValue<EventListener> script_object(listener.get());
  media_recorder_->Start(&exception_state_);
  media_recorder_->set_onstop(script_object);

  EXPECT_CALL(
      *listener,
      HandleEvent(Eq(media_recorder_),
                  Pointee(Property(&dom::Event::type, base::Tokens::stop())),
                  _));

  EXPECT_CALL(*audio_track_, Stop());
  media_recorder_->Stop(&exception_state_);
}

TEST_F(MediaRecorderTest, ReleaseRecorderBeforeStoppingSource) {
  // This test ensures that media recorder can be destroyed
  // before the audio source is destroyed. It should not crash.
  media_recorder_ = nullptr;
}

}  // namespace media_capture
}  // namespace cobalt
