// Copyright 2018 Google Inc. All Rights Reserved.
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

#include "cobalt/media_capture/media_devices.h"

#include "base/memory/scoped_ptr.h"
#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/testing/stub_window.h"
#include "cobalt/script/global_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const int kMaxDomElementDepth = 8;

scoped_ptr<cobalt::script::EnvironmentSettings> CreateDOMSettings() {
  cobalt::dom::DOMSettings::Options options;
#if defined(ENABLE_FAKE_MICROPHONE)
  options.microphone_options.enable_fake_microphone = true;
#endif  // defined(ENABLE_FAKE_MICROPHONE)

  return make_scoped_ptr<cobalt::script::EnvironmentSettings>(
      new cobalt::dom::DOMSettings(kMaxDomElementDepth, nullptr, nullptr,
                                   nullptr, nullptr, nullptr, nullptr, nullptr,
                                   nullptr, nullptr, options));
}

}  // namespace.

namespace cobalt {
namespace media_capture {

class GetUserMediaTest : public ::testing::Test {
 protected:
  GetUserMediaTest()
      : window_(CreateDOMSettings()),
        media_devices_(new MediaDevices(
            window_.global_environment()->script_value_factory())) {
    media_devices_->SetEnvironmentSettings(window_.environment_settings());
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

TEST_F(GetUserMediaTest, AudioMediaSources) {
  media_stream::MediaStreamConstraints constraints;
  constraints.set_audio(true);
  script::Handle<MediaDevices::MediaStreamPromise> media_stream_promise =
      media_devices_->GetUserMedia(constraints);
  ASSERT_FALSE(media_stream_promise.IsEmpty());
  EXPECT_EQ(cobalt::script::PromiseState::kFulfilled,
            media_stream_promise->State());
}

}  // namespace media_capture
}  // namespace cobalt
