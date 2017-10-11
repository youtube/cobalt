// Copyright 2017 Google Inc. All Rights Reserved.
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

#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "cobalt/accessibility/screen_reader.h"
#include "cobalt/accessibility/text_alternative.h"
#include "cobalt/accessibility/tts_engine.h"
#include "cobalt/browser/web_module.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/window.h"
#include "cobalt/test/document_loader.h"
#include "starboard/window.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace cobalt {
namespace accessibility {
namespace {
struct TestInfo {
  TestInfo(const std::string& html_file_name,
           const std::string& expected_result,
           bool screen_reader_enabled)
      : html_file_name(html_file_name),
        expected_result(expected_result),
        screen_reader_enabled(screen_reader_enabled) {}
  std::string html_file_name;
  std::string expected_result;
  bool screen_reader_enabled;
};

// Enumerate the *.html files in the accessibility_test directory and build
// a list of input .html files and expected results.
std::vector<TestInfo> EnumerateTests(bool screen_reader_enabled,
                                     const std::string& subdir) {
  std::vector<TestInfo> infos;
  FilePath root_directory;
  PathService::Get(base::DIR_SOURCE_ROOT, &root_directory);
  root_directory = root_directory.Append("cobalt")
                       .Append("accessibility")
                       .Append("testdata")
                       .Append(subdir);
  file_util::FileEnumerator enumerator(root_directory, false,
                                       file_util::FileEnumerator::FILES);
  for (FilePath html_file = enumerator.Next(); !html_file.empty();
       html_file = enumerator.Next()) {
    if (html_file.Extension() == ".html") {
      FilePath expected_results_file = html_file.ReplaceExtension(".expected");
      std::string results;
      if (!file_util::ReadFileToString(expected_results_file, &results)) {
        DLOG(WARNING) << "Failed to read results from file: "
                      << expected_results_file.value();
        continue;
      }
      TrimWhitespaceASCII(results, TRIM_ALL, &results);
      infos.push_back(TestInfo(html_file.BaseName().value(),
                               results,
                               screen_reader_enabled));
    }
  }
  return infos;
}

class MockTTSEngine : public TTSEngine {
 public:
  MOCK_METHOD1(Speak, void(const std::string&));
  MOCK_METHOD1(SpeakNow, void(const std::string&));
};

class TextAlternativeTest : public ::testing::TestWithParam<TestInfo> {
 protected:
  test::DocumentLoader document_loader_;
};

class LiveRegionMutationTest : public ::testing::TestWithParam<TestInfo> {
 public:
  LiveRegionMutationTest() : quit_event_(true, false) {}
  void OnError(const GURL&, const std::string& error) {
    DLOG(ERROR) << error;
    Quit();
  }
  void Quit() {
    quit_event_.Signal();
    screen_reader_.reset();
  }
  void OnClose(base::TimeDelta close_time) {
    UNREFERENCED_PARAMETER(close_time);
    Quit();
  }

  scoped_refptr<script::Wrappable> CreateWindowAttribute(
      const scoped_refptr<dom::Window>& window,
      dom::MutationObserverTaskManager* mutation_observer_task_manager,
      script::GlobalEnvironment* global_environment) {
    UNREFERENCED_PARAMETER(global_environment);
    screen_reader_.reset(new accessibility::ScreenReader(
        window->document(), &tts_engine_, mutation_observer_task_manager));
    screen_reader_->set_enabled(GetParam().screen_reader_enabled);
    EXPECT_EQ(GetParam().screen_reader_enabled, screen_reader_->enabled());

    return NULL;
  }

  static void OnRenderTreeProducedStub(
      const browser::WebModule::LayoutResults&) {}

 protected:
  MockTTSEngine tts_engine_;
  base::WaitableEvent quit_event_;
  scoped_ptr<accessibility::ScreenReader> screen_reader_;
};

// Return a NULL SbWindow, since we do not need to pass a valid SbWindow to an
// on screen keyboard.
SbWindow GetNullSbWindow() { return NULL; }
}  // namespace

TEST_P(TextAlternativeTest, TextAlternativeTest) {
  GURL url(
      std::string("file:///cobalt/accessibility/testdata/text_alternative/" +
                  GetParam().html_file_name));
  document_loader_.Load(url);
  ASSERT_TRUE(document_loader_.document());
  scoped_refptr<dom::Element> element =
      document_loader_.document()->GetElementById("element_to_test");
  ASSERT_TRUE(element);
  EXPECT_EQ(GetParam().expected_result, ComputeTextAlternative(element));
}

TEST_P(LiveRegionMutationTest, LiveRegionMutationTest) {
  GURL url(std::string("file:///cobalt/accessibility/testdata/live_region/" +
                       GetParam().html_file_name));
  const math::Size kDefaultViewportSize(1280, 720);
  const float kDefaultVideoPixelRatio = 1.0f;
  const float kRefreshRate = 60.0f;

  network::NetworkModule network_module((network::NetworkModule::Options()));
  render_tree::ResourceProviderStub resource_provider;

  // Use test runner mode to allow the content itself to dictate when it is
  // ready for layout should be performed.  See cobalt/dom/test_runner.h.
  browser::WebModule::Options web_module_options;
  web_module_options.injected_window_attributes["test"] = base::Bind(
      &LiveRegionMutationTest::CreateWindowAttribute, base::Unretained(this));

  // Set expected result from mutation.
  std::string expected_speech = GetParam().expected_result;
  if (expected_speech.empty() || !GetParam().screen_reader_enabled) {
    EXPECT_CALL(tts_engine_, Speak(_)).Times(0);
  } else {
    EXPECT_CALL(tts_engine_, Speak(expected_speech));
  }

  // Create the webmodule and let it run.
  DLOG(INFO) << url.spec();
  browser::WebModule web_module(
      url, base::kApplicationStateStarted,
      base::Bind(&LiveRegionMutationTest::OnRenderTreeProducedStub),
      base::Bind(&LiveRegionMutationTest::OnError, base::Unretained(this)),
      base::Bind(&LiveRegionMutationTest::OnClose, base::Unretained(this)),
      base::Closure(), /* window_minimize_callback */
      base::Bind(&GetNullSbWindow), NULL /* can_play_type_handler */,
      NULL /* web_media_player_factory */, &network_module,
      kDefaultViewportSize, kDefaultVideoPixelRatio, &resource_provider,
      kRefreshRate, web_module_options);

  // Wait for the test to quit.
  quit_event_.Wait();
}

INSTANTIATE_TEST_CASE_P(
    TextAlternativeTest, TextAlternativeTest,
    ::testing::ValuesIn(EnumerateTests(true, "text_alternative")));

INSTANTIATE_TEST_CASE_P(
    LiveRegionMutationTestEnabled, LiveRegionMutationTest,
    ::testing::ValuesIn(EnumerateTests(true, "live_region")));

INSTANTIATE_TEST_CASE_P(
    LiveRegionMutationTestDisabled, LiveRegionMutationTest,
    ::testing::ValuesIn(EnumerateTests(false, "live_region")));
}  // namespace accessibility
}  // namespace cobalt
