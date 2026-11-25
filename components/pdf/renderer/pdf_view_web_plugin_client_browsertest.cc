// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/renderer/pdf_view_web_plugin_client.h"

#include "base/values.h"
#include "content/public/test/render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace pdf {

// Renderer-level test for PdfViewWebPluginClient. Verifies that PostMessage()
// returns safely when the plugin container is null (no frame available).
// Regression test for crbug.com/524398415.
class PdfViewWebPluginClientTest : public content::RenderViewTest {
 public:
  PdfViewWebPluginClientTest() = default;
  ~PdfViewWebPluginClientTest() override = default;

  void SetUp() override {
    content::RenderViewTest::SetUp();
    client_ = std::make_unique<PdfViewWebPluginClient>(GetMainRenderFrame());
  }

  void TearDown() override {
    client_.reset();
    content::RenderViewTest::TearDown();
  }

 protected:
  std::unique_ptr<PdfViewWebPluginClient> client_;
};

// Verifies that PostMessage() does not crash when `plugin_container_` is null
// (e.g., after the plugin has been destroyed but a callback still fires).
// Without the fix, GetFrame() dereferences null `plugin_container_` and
// crashes.
TEST_F(PdfViewWebPluginClientTest, PostMessageWithNoContainerDoesNotCrash) {
  base::DictValue message;
  message.Set("type", "printPreviewLoaded");
  client_->PostMessage(std::move(message));
}

// Verifies that PostMessage() does not crash after SetPluginContainer() has
// been called with nullptr (simulating the Destroy() path).
TEST_F(PdfViewWebPluginClientTest,
       PostMessageAfterContainerClearedDoesNotCrash) {
  client_->SetPluginContainer(nullptr);

  base::DictValue message;
  message.Set("type", "printPreviewLoaded");
  client_->PostMessage(std::move(message));
}

}  // namespace pdf
