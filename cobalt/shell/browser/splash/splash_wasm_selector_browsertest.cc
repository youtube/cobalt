// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/shell/browser/splash/splash_wasm_selector.h"

#include <string>
#include <vector>

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "cobalt/shell/browser/splash/testdata/test_selector_wasm.h"
#include "cobalt/testing/browser_tests/browser/test_shell.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "cobalt/testing/browser_tests/content_browser_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"

namespace content {

namespace {

std::string ToJsArray(base::span<const uint8_t> bytes) {
  std::string out = "[";
  for (uint8_t b : bytes) {
    out += base::NumberToString(b);
    out += ",";
  }
  out += "]";
  return out;
}

std::string ToString(const std::vector<uint8_t>& bytes) {
  return std::string(bytes.begin(), bytes.end());
}

}  // namespace

// Plays the web app's part, putting a selector and videos into the splash
// cache from JavaScript, then reads them back natively with
// SplashWasmSelector, the way Cobalt would at its next launch.
// Like Cobalt, these tests run in single-process mode, so V8 has been
// initialized by the in-process renderer by the time the selector runs.
class SplashWasmSelectorBrowserTest : public ContentBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    // The cache belongs to the page's origin, so stay on it.
    ASSERT_TRUE(
        NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  }

  void PutInCache(const std::string& key,
                  base::span<const uint8_t> bytes,
                  const std::string& mime_type) {
    std::string script = base::StringPrintf(
        R"((async () => {
          const cache = await caches.open('default');
          await cache.put('%s', new Response(new Uint8Array(%s),
                          {headers: {'Content-Type': '%s'}}));
          return 'ok';
        })())",
        key.c_str(), ToJsArray(bytes).c_str(), mime_type.c_str());
    ASSERT_EQ("ok", EvalJs(shell(), script));
  }

  void PutVideo(const std::string& key, const std::string& content) {
    PutInCache(key, base::as_byte_span(content), "video/webm");
  }

  void PutSelector(base::span<const uint8_t> wasm) {
    PutInCache(SplashWasmSelector::kSelectorKey, wasm, "application/wasm");
  }

  SplashSelection Select(int screen_height, const std::string& topic = "") {
    SplashWasmSelector selector(shell()->web_contents()->GetBrowserContext(),
                                embedded_test_server()->base_url(), u"default");
    SplashSelectorInput input;
    input.topic = topic;
    input.screen_width = screen_height * 16 / 9;
    input.screen_height = screen_height;
    SplashSelection selection;
    base::RunLoop run_loop;
    selector.Select(input, base::BindLambdaForTesting([&](SplashSelection s) {
                      selection = std::move(s);
                      run_loop.Quit();
                    }));
    run_loop.Run();
    return selection;
  }

  void PutSelectorAndVideos() {
    PutSelector(kTestSelectorWasm);
    PutVideo("splash.webm", "FALLBACK");
    PutVideo("splash_1080.webm", "VIDEO_1080");
    PutVideo("splash_2160.webm", "VIDEO_2160");
    PutVideo("music.webm", "VIDEO_MUSIC");
  }
};

IN_PROC_BROWSER_TEST_F(SplashWasmSelectorBrowserTest, SelectsVideoWithWasm) {
  PutSelectorAndVideos();

  SplashSelection selection = Select(2160);
  EXPECT_EQ("", selection.selector_error);
  EXPECT_TRUE(selection.selected_by_wasm);
  EXPECT_EQ("splash_2160.webm", selection.key);
  EXPECT_EQ("VIDEO_2160", ToString(selection.video));

  selection = Select(1080);
  EXPECT_EQ("splash_1080.webm", selection.key);
  EXPECT_EQ("VIDEO_1080", ToString(selection.video));

  selection = Select(1080, "music");
  EXPECT_EQ("music.webm", selection.key);
  EXPECT_EQ("VIDEO_MUSIC", ToString(selection.video));

  perf_test::PrintResult("SplashWasmSelector", "", "read_selector",
                         selection.read_selector_time.InMillisecondsF(), "ms",
                         true);
  perf_test::PrintResult("SplashWasmSelector", "", "run_selector",
                         selection.run_selector_time.InMillisecondsF(), "ms",
                         true);
  perf_test::PrintResult("SplashWasmSelector", "", "read_video",
                         selection.read_video_time.InMillisecondsF(), "ms",
                         true);
}

IN_PROC_BROWSER_TEST_F(SplashWasmSelectorBrowserTest,
                       FallsBackWithoutSelector) {
  PutVideo("splash.webm", "FALLBACK");

  SplashSelection selection = Select(2160);
  EXPECT_FALSE(selection.selected_by_wasm);
  EXPECT_EQ("Selector not found", selection.selector_error);
  EXPECT_EQ("splash.webm", selection.key);
  EXPECT_EQ("FALLBACK", ToString(selection.video));
}

IN_PROC_BROWSER_TEST_F(SplashWasmSelectorBrowserTest,
                       FallsBackOnBrokenSelector) {
  const uint8_t kGarbage[] = {'n', 'o', 't', 'w', 'a', 's', 'm'};
  PutSelector(kGarbage);
  PutVideo("splash.webm", "FALLBACK");

  SplashSelection selection = Select(2160);
  EXPECT_FALSE(selection.selected_by_wasm);
  EXPECT_NE(std::string::npos, selection.selector_error.find("compile"))
      << selection.selector_error;
  EXPECT_EQ("splash.webm", selection.key);
  EXPECT_EQ("FALLBACK", ToString(selection.video));
}

IN_PROC_BROWSER_TEST_F(SplashWasmSelectorBrowserTest,
                       FallsBackWhenSelectedVideoIsMissing) {
  PutSelector(kTestSelectorWasm);
  PutVideo("splash.webm", "FALLBACK");

  SplashSelection selection = Select(2160);
  EXPECT_FALSE(selection.selected_by_wasm);
  EXPECT_EQ("Selected video splash_2160.webm not found",
            selection.selector_error);
  EXPECT_EQ("splash.webm", selection.key);
  EXPECT_EQ("FALLBACK", ToString(selection.video));
}

IN_PROC_BROWSER_TEST_F(SplashWasmSelectorBrowserTest, NothingCached) {
  SplashSelection selection = Select(2160);
  EXPECT_FALSE(selection.selected_by_wasm);
  EXPECT_EQ("", selection.key);
  EXPECT_TRUE(selection.video.empty());
}

}  // namespace content
