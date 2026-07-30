// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/clipboard/system_clipboard.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/set_selection_options.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/keywords.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/mock_clipboard_host.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

// Paste in kSelection mode (the state ExecutePasteGlobalSelection sets on
// middle-click) must not leak an image planted on the kStandard buffer
// through GetFragmentFromClipboard's image fallback.
TEST(ClipboardCommandsPasteTest, PasteInSelectionModeDoesNotLeakStandardImage) {
  ScopedClipboardPasteImageRespectBufferForTest scoped_feature(true);
  test::TaskEnvironment task_environment;
  auto page_holder = std::make_unique<DummyPageHolder>(gfx::Size(1, 1));
  LocalFrame& frame = page_holder->GetFrame();

  PageTestBase::MockClipboardHostProvider mock_clipboard_host_provider(
      frame.GetBrowserInterfaceBroker());

  HTMLElement* body = page_holder->GetDocument().body();
  body->setAttribute(html_names::kContenteditableAttr, keywords::kTrue);
  body->Focus();
  frame.GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kTest);
  frame.Selection().SetSelection(
      SelectionInDomTree::Builder().SelectAllChildren(*body).Build(),
      SetSelectionOptions());
  ASSERT_TRUE(
      frame.Selection().ComputeVisibleSelectionInDomTree().IsContentEditable());

  SkBitmap bitmap;
  ASSERT_TRUE(bitmap.tryAllocPixelsFlags(
      SkImageInfo::Make(4, 3, kN32_SkColorType, kOpaque_SkAlphaType), 0));
  mojom::blink::ClipboardHost* clipboard_host =
      mock_clipboard_host_provider.clipboard_host();
  clipboard_host->WriteImage(bitmap);
  clipboard_host->CommitWrite();
  test::RunPendingTasks();

  frame.GetSystemClipboard()->SetSelectionMode(true);
  frame.GetEditor().ExecuteCommand("Paste");
  frame.GetSystemClipboard()->SetSelectionMode(false);

  const String html = body->GetInnerHTMLString();
  EXPECT_FALSE(html.contains("data:image/png"))
      << "Image fallback leaked kStandard PNG while buffer_ was kSelection: "
      << html.Utf8();
}

}  // namespace blink
