// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_RENDERER_WEB_FRAME_TEST_PROXY_H_
#define CONTENT_WEB_TEST_RENDERER_WEB_FRAME_TEST_PROXY_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "content/renderer/render_frame_impl.h"
#include "content/web_test/common/web_test.mojom.h"
#include "content/web_test/renderer/accessibility_controller.h"
#include "content/web_test/renderer/text_input_controller.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/platform/web_effective_connection_type.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/test/frame_widget_test_helper.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_event_intent.h"

namespace content {
class SpellCheckClient;
class TestRunner;

// WebFrameTestProxy is used during running web tests instead of a
// RenderFrameImpl to inject test-only behaviour by overriding methods in the
// base class.
class WebFrameTestProxy : public RenderFrameImpl,
                          public mojom::WebTestRenderFrame {
 public:
  WebFrameTestProxy(RenderFrameImpl::CreateParams params,
                    TestRunner* test_runner);

  WebFrameTestProxy(const WebFrameTestProxy&) = delete;
  WebFrameTestProxy& operator=(const WebFrameTestProxy&) = delete;

  ~WebFrameTestProxy() override;

  // RenderFrameImpl overrides.
  void Initialize(blink::WebFrame* parent) override;

  // Reset state between tests.
  void Reset();

  // Returns a frame name that can be used in the output of web tests
  // (the name is derived from the frame's unique name).
  std::string GetFrameNameForWebTests();
  // Returns a description of the frame, including the name from
  // GetFrameNameForWebTests(), that can be used in the output of web
  // tests.
  std::string GetFrameDescriptionForWebTests();

  // Returns the test helper of WebFrameWidget for the local root of this frame.
  blink::FrameWidgetTestHelper* GetLocalRootFrameWidgetTestHelper();

  // WebLocalFrameClient implementation.
  blink::WebPlugin* CreatePlugin(const blink::WebPluginParams& params) override;
  void DidAddMessageToConsole(const blink::WebConsoleMessage& message,
                              const blink::WebString& source_name,
                              unsigned source_line,
                              const blink::WebString& stack_trace) override;
  void DidStartLoading() override;
  void DidStopLoading() override;
  void DidChangeSelection(bool is_selection_empty,
                          blink::SyncCondition force_sync) override;
  void DidChangeContents() override;
  blink::WebEffectiveConnectionType GetEffectiveConnectionType() override;
  void UpdateContextMenuDataForTesting(
      const blink::ContextMenuData& context_menu_data,
      const absl::optional<gfx::Point>&) override;
  void DidDispatchPingLoader(const blink::WebURL& url) override;
  void WillSendRequest(blink::WebURLRequest& request,
                       ForRedirect for_redirect) override;
  void BeginNavigation(std::unique_ptr<blink::WebNavigationInfo> info) override;
  void PostAccessibilityEvent(const ui::AXEvent& event) override;
  void NotifyWebAXObjectMarkedDirty(const blink::WebAXObject& object) override;
  void CheckIfAudioSinkExistsAndIsAuthorized(
      const blink::WebString& sink_id,
      blink::WebSetSinkIdCompleteCallback completion_callback) override;
  void DidClearWindowObject() override;

  // mojom::WebTestRenderFrame implementation.
  void SynchronouslyCompositeAfterTest(
      SynchronouslyCompositeAfterTestCallback callback) override;
  void DumpFrameLayout(DumpFrameLayoutCallback callback) override;
  void SetTestConfiguration(mojom::WebTestRunTestConfigurationPtr config,
                            bool starting_test) override;
  void OnDeactivated() override;
  void OnReactivated() override;

 private:
  void BindReceiver(
      mojo::PendingAssociatedReceiver<mojom::WebTestRenderFrame> receiver);

  void HandleWebAccessibilityEvent(
      const blink::WebAXObject& object,
      const char* event_name,
      const std::vector<ui::AXEventIntent>& event_intents);

  TestRunner* test_runner();

  TestRunner* const test_runner_;

  std::unique_ptr<SpellCheckClient> spell_check_;

  TextInputController text_input_controller_{this};

  AccessibilityController accessibility_controller_{this};

  mojo::AssociatedReceiver<mojom::WebTestRenderFrame>
      web_test_render_frame_receiver_{this};
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_RENDERER_WEB_FRAME_TEST_PROXY_H_
