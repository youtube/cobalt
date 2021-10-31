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

#include <memory>

#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/testing/stub_environment_settings.h"
#include "cobalt/dom/testing/stub_window.h"
#include "cobalt/media_capture/media_devices.h"
#include "cobalt/media_stream/microphone_audio_source.h"
#include "cobalt/media_stream/testing/mock_media_stream_audio_source.h"
#include "cobalt/script/global_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::unique_ptr<cobalt::script::EnvironmentSettings> CreateDOMSettings() {
  cobalt::dom::DOMSettings::Options options;
#if defined(ENABLE_FAKE_MICROPHONE)
  options.microphone_options.enable_fake_microphone = true;
#endif  // defined(ENABLE_FAKE_MICROPHONE)

  return std::unique_ptr<cobalt::script::EnvironmentSettings>(
      new cobalt::dom::testing::StubEnvironmentSettings(options));
}

}  // namespace.

namespace cobalt {
namespace media_capture {

class GetUserMediaTest : public ::testing::Test {
 protected:
  GetUserMediaTest()
      : window_(CreateDOMSettings()),
        media_devices_(new MediaDevices(
            window_.environment_settings(),
            window_.global_environment()->script_value_factory())) {}

  media_stream::MicrophoneAudioSource* GetMicrophoneAudioSource() {
    return base::polymorphic_downcast<media_stream::MicrophoneAudioSource*>(
        media_devices_->audio_source_.get());
  }

  dom::testing::StubWindow window_;
  scoped_refptr<MediaDevices> media_devices_;
};

TEST_F(GetUserMediaTest, TestEmptyParameters) {
  script::Handle<MediaDevices::MediaStreamPromise> media_stream_promise =
      media_devices_->GetUserMedia();
  ASSERT_FALSE(media_stream_promise.IsEmpty());
  EXPECT_EQ(cobalt::script::PromiseState::kRejected,
            media_stream_promise->State());
}

TEST_F(GetUserMediaTest, NoMediaSources) {
  media_stream::MediaStreamConstraints constraints;
  constraints.set_audio(false);
  script::Handle<MediaDevices::MediaStreamPromise> media_stream_promise =
      media_devices_->GetUserMedia(constraints);
  ASSERT_FALSE(media_stream_promise.IsEmpty());
  EXPECT_EQ(cobalt::script::PromiseState::kRejected,
            media_stream_promise->State());
}

TEST_F(GetUserMediaTest, PendingPromise) {
  media_stream::MediaStreamConstraints constraints;
  constraints.set_audio(true);
  script::Handle<MediaDevices::MediaStreamPromise> media_stream_promise =
      media_devices_->GetUserMedia(constraints);
  ASSERT_FALSE(media_stream_promise.IsEmpty());
  EXPECT_EQ(cobalt::script::PromiseState::kPending,
            media_stream_promise->State());
}

TEST_F(GetUserMediaTest, MicrophoneStoppedRejectedPromise) {
  media_stream::MediaStreamConstraints constraints;
  constraints.set_audio(true);
  script::Handle<MediaDevices::MediaStreamPromise> media_stream_promise =
      media_devices_->GetUserMedia(constraints);
  ASSERT_FALSE(media_stream_promise.IsEmpty());
  EXPECT_EQ(cobalt::script::PromiseState::kPending,
            media_stream_promise->State());
  media_devices_->audio_source_->StopSource();
  EXPECT_EQ(cobalt::script::PromiseState::kRejected,
            media_stream_promise->State());
}

TEST_F(GetUserMediaTest, MicrophoneErrorRejectedPromise) {
  media_stream::MediaStreamConstraints constraints;
  constraints.set_audio(true);
  script::Handle<MediaDevices::MediaStreamPromise> media_stream_promise =
      media_devices_->GetUserMedia(constraints);
  ASSERT_FALSE(media_stream_promise.IsEmpty());
  EXPECT_EQ(cobalt::script::PromiseState::kPending,
            media_stream_promise->State());
  media_devices_->OnMicrophoneError(
      speech::MicrophoneManager::MicrophoneError::kAborted, "Aborted");
  EXPECT_EQ(cobalt::script::PromiseState::kRejected,
            media_stream_promise->State());
}

TEST_F(GetUserMediaTest, MicrophoneSuccessFulfilledPromise) {
  media_stream::MediaStreamConstraints constraints;
  constraints.set_audio(true);
  script::Handle<MediaDevices::MediaStreamPromise> media_stream_promise =
      media_devices_->GetUserMedia(constraints);
  ASSERT_FALSE(media_stream_promise.IsEmpty());
  EXPECT_EQ(cobalt::script::PromiseState::kPending,
            media_stream_promise->State());
  media_devices_->OnMicrophoneSuccess();
  media_devices_->audio_source_->StopSource();
  EXPECT_EQ(cobalt::script::PromiseState::kFulfilled,
            media_stream_promise->State());
}

TEST_F(GetUserMediaTest, MultipleMicrophoneSuccessFulfilledPromise) {
  media_stream::MediaStreamConstraints constraints;
  constraints.set_audio(true);
  std::vector<script::Handle<MediaDevices::MediaStreamPromise>>
      media_stream_promises;

  for (size_t i = 0; i < 2; ++i) {
    media_stream_promises.push_back(media_devices_->GetUserMedia(constraints));
    ASSERT_FALSE(media_stream_promises.back().IsEmpty());
    EXPECT_EQ(cobalt::script::PromiseState::kPending,
              media_stream_promises.back()->State());
    media_devices_->OnMicrophoneSuccess();
  }

  media_devices_->audio_source_->StopSource();

  for (size_t i = 0; i < media_stream_promises.size(); ++i) {
    EXPECT_EQ(cobalt::script::PromiseState::kFulfilled,
              media_stream_promises[i]->State());
  }
}

}  // namespace media_capture
}  // namespace cobalt
