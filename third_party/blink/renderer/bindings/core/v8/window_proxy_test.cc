// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

using WindowProxyTest = SimTest;

// Tests that a WindowProxy is reinitialized after a navigation, even if the new
// Document does not use any scripting.
TEST_F(WindowProxyTest, ReinitializedAfterNavigation) {
  // TODO(dcheng): It's nicer to use TestingPlatformSupportWithMockScheduler,
  // but that leads to random DCHECKs in loading code.

  SimRequest main_resource("https://example.com/index.html", "text/html");
  LoadURL("https://example.com/index.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <html><head><script>
    var childWindow;
    function runTest() {
      childWindow = window[0];
      document.querySelector('iframe').onload = runTest2;
      childWindow.location = 'data:text/plain,Initial.';
    }
    function runTest2() {
      try {
        childWindow.location = 'data:text/plain,Final.';
        console.log('PASSED');
      } catch (e) {
        console.log('FAILED');
      }
      document.querySelector('iframe').onload = null;
    }
    </script></head><body onload='runTest()'>
    <iframe></iframe></body></html>
  )HTML");

  // Wait for the first data: URL to load
  test::RunPendingTasks();

  // Wait for the second data: URL to load.
  test::RunPendingTasks();

  ASSERT_GT(ConsoleMessages().size(), 0U);
  EXPECT_EQ("PASSED", ConsoleMessages()[0]);
}

TEST_F(WindowProxyTest, IsolatedWorldReinitializedAfterNavigation) {
  SimRequest main_resource("https://example.com/index.html", "text/html");
  LoadURL("https://example.com/index.html");
  main_resource.Complete(R"HTML(
    <!DOCTYPE html>
    <html><body><iframe></iframe></body></html>
  )HTML");

  ASSERT_TRUE(MainFrame().FirstChild());

  v8::HandleScope scope(Window().GetIsolate());

  const int32_t kIsolatedWorldId = 42;

  // Save a reference to the top `window` in the isolated world.
  v8::Local<v8::Value> window_top =
      MainFrame().ExecuteScriptInIsolatedWorldAndReturnValue(
          kIsolatedWorldId, WebScriptSource("window"),
          BackForwardCacheAware::kAllow);
  ASSERT_TRUE(window_top->IsObject());

  // Save a reference to the child frame's window proxy in the isolated world.
  v8::Local<v8::Value> saved_child_window =
      MainFrame().ExecuteScriptInIsolatedWorldAndReturnValue(
          kIsolatedWorldId, WebScriptSource("saved = window[0]"),
          BackForwardCacheAware::kAllow);
  ASSERT_TRUE(saved_child_window->IsObject());

  frame_test_helpers::LoadFrame(MainFrame().FirstChild()->ToWebLocalFrame(),
                                "data:text/html,<body><p>Hello</p></body>");
  ASSERT_TRUE(MainFrame().FirstChild());

  // Test if the window proxy of the navigated frame was reinitialized. The
  // `top` attribute of the saved child frame's window proxy reference should
  // refer to the same object as the top-level window proxy reference that was
  // cached earlier.
  v8::Local<v8::Value> top_via_saved =
      MainFrame().ExecuteScriptInIsolatedWorldAndReturnValue(
          kIsolatedWorldId, WebScriptSource("saved.top"),
          BackForwardCacheAware::kAllow);
  EXPECT_TRUE(top_via_saved->IsObject());
  EXPECT_TRUE(window_top->StrictEquals(top_via_saved));
}

}  // namespace blink
