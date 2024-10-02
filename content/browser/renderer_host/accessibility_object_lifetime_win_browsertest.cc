// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "content/browser/renderer_host/legacy_render_widget_host_win.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "ui/accessibility/accessibility_switches.h"
#include "ui/accessibility/platform/ax_platform_node_win.h"
#include "ui/accessibility/platform/ax_system_caret_win.h"
#include "ui/base/win/hwnd_subclass.h"

namespace content {

class AccessibilityObjectLifetimeWinBrowserTest
    : public content::ContentBrowserTest {
 public:
  AccessibilityObjectLifetimeWinBrowserTest() = default;

  AccessibilityObjectLifetimeWinBrowserTest(
      const AccessibilityObjectLifetimeWinBrowserTest&) = delete;
  AccessibilityObjectLifetimeWinBrowserTest& operator=(
      const AccessibilityObjectLifetimeWinBrowserTest&) = delete;

  ~AccessibilityObjectLifetimeWinBrowserTest() override = default;

 protected:
  RenderWidgetHostViewAura* GetView() {
    return static_cast<RenderWidgetHostViewAura*>(
        shell()->web_contents()->GetRenderWidgetHostView());
  }

  LegacyRenderWidgetHostHWND* GetLegacyRenderWidgetHostHWND() {
    return GetView()->legacy_render_widget_host_HWND_;
  }

  void CacheRootNode(bool is_uia_request) {
    GetLegacyRenderWidgetHostHWND()
        ->GetOrCreateWindowRootAccessible(is_uia_request)
        ->QueryInterface(IID_PPV_ARGS(&test_node_));
  }

  void CacheCaretNode() {
    GetLegacyRenderWidgetHostHWND()
        ->ax_system_caret_->GetCaret()
        ->QueryInterface(IID_PPV_ARGS(&test_node_));
  }

  HWND GetHwnd() { return GetView()->AccessibilityGetAcceleratedWidget(); }

  Microsoft::WRL::ComPtr<ui::AXPlatformNodeWin> test_node_;
};

IN_PROC_BROWSER_TEST_F(AccessibilityObjectLifetimeWinBrowserTest,
                       RootDoesNotLeak) {
  testing::ScopedContentAXModeSetter ax_mode_setter(ui::kAXModeBasic.flags());

  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  // Cache a pointer to the root node we return to Windows.
  CacheRootNode(false);

  // Repeatedly call the public API to obtain an accessibility object. If our
  // code is leaking references, this will drive up the reference count.
  for (int i = 0; i < 10; i++) {
    Microsoft::WRL::ComPtr<IAccessible> root_accessible;
    EXPECT_HRESULT_SUCCEEDED(::AccessibleObjectFromWindow(
        GetHwnd(), OBJID_CLIENT, IID_PPV_ARGS(&root_accessible)));
    EXPECT_NE(root_accessible.Get(), nullptr);
  }

  // Close the main window.
  shell()->Close();

  // At this point our test reference should be the only one remaining.
  EXPECT_EQ(test_node_->m_dwRef, 1);
}

IN_PROC_BROWSER_TEST_F(AccessibilityObjectLifetimeWinBrowserTest,
                       CaretDoesNotLeak) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  // Cache a pointer to the object we return to Windows.
  CacheCaretNode();

  // Repeatedly call the public API to obtain an accessibility object. If our
  // code is leaking references, this will drive up the reference count.
  for (int i = 0; i < 10; i++) {
    Microsoft::WRL::ComPtr<IAccessible> caret_accessible;
    EXPECT_HRESULT_SUCCEEDED(::AccessibleObjectFromWindow(
        GetHwnd(), OBJID_CARET, IID_PPV_ARGS(&caret_accessible)));
    EXPECT_NE(caret_accessible.Get(), nullptr);
  }

  // Close the main window.
  shell()->Close();

  // At this point our test reference should be the only one remaining.
  EXPECT_EQ(test_node_->m_dwRef, 1);
}

// Window subclassing message filter for the legacy window to allow us to
// examine state after the call to LegacyRenderWidgetHostHWND::Destroy() but
// before the window is torn down completely. Operating system hooks can run in
// this narrow window; see crbug.com/945584 for one example.
class AccessibilityTeardownTestMessageFilter : public ui::HWNDMessageFilter {
 public:
  AccessibilityTeardownTestMessageFilter(
      LegacyRenderWidgetHostHWND* legacy_render_widget_host_HWND)
      : legacy_render_widget_host_HWND_(legacy_render_widget_host_HWND) {
    HWND hwnd = legacy_render_widget_host_HWND->hwnd();
    CHECK(hwnd);
    ui::HWNDSubclass::AddFilterToTarget(hwnd, this);
  }
  ~AccessibilityTeardownTestMessageFilter() override = default;

  // ui::HWNDMessageFilter:
  bool FilterMessage(HWND hwnd,
                     UINT message,
                     WPARAM w_param,
                     LPARAM l_param,
                     LRESULT* l_result) override {
    if (message == WM_DESTROY) {
      // Verify that the legacy window does not crash when asked for an
      // accessibility object.
      legacy_render_widget_host_HWND_->GetOrCreateWindowRootAccessible(false);

      // Remove ourselves as a subclass.
      ui::HWNDSubclass::RemoveFilterFromAllTargets(this);
    }

    return true;
  }

 private:
  raw_ptr<LegacyRenderWidgetHostHWND, DanglingUntriaged>
      legacy_render_widget_host_HWND_;
};

IN_PROC_BROWSER_TEST_F(AccessibilityObjectLifetimeWinBrowserTest,
                       DoNotCrashDuringLegacyWindowDestroy) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  AccessibilityTeardownTestMessageFilter test_message_filter(
      GetLegacyRenderWidgetHostHWND());

  GetView()->Destroy();
}

class AccessibilityObjectLifetimeUiaWinBrowserTest
    : public AccessibilityObjectLifetimeWinBrowserTest {
 public:
  AccessibilityObjectLifetimeUiaWinBrowserTest() = default;

  AccessibilityObjectLifetimeUiaWinBrowserTest(
      const AccessibilityObjectLifetimeUiaWinBrowserTest&) = delete;
  AccessibilityObjectLifetimeUiaWinBrowserTest& operator=(
      const AccessibilityObjectLifetimeUiaWinBrowserTest&) = delete;

  ~AccessibilityObjectLifetimeUiaWinBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kEnableExperimentalUIAutomation);
  }
};

IN_PROC_BROWSER_TEST_F(AccessibilityObjectLifetimeUiaWinBrowserTest,
                       RootDoesNotLeak) {
  testing::ScopedContentAXModeSetter ax_mode_setter(ui::kAXModeBasic.flags());

  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  // Cache a pointer to the root node we return to Windows.
  CacheRootNode(false);

  Microsoft::WRL::ComPtr<IUIAutomation> uia;
  ASSERT_HRESULT_SUCCEEDED(CoCreateInstance(CLSID_CUIAutomation, nullptr,
                                            CLSCTX_INPROC_SERVER,
                                            IID_IUIAutomation, &uia));

  // Repeatedly call the public API to obtain an accessibility object. If our
  // code is leaking references, this will drive up the reference count.
  for (int i = 0; i < 10; i++) {
    Microsoft::WRL::ComPtr<IUIAutomationElement> root_element;
    EXPECT_HRESULT_SUCCEEDED(uia->ElementFromHandle(GetHwnd(), &root_element));
    EXPECT_NE(root_element.Get(), nullptr);

    // Raise an event on the root node. This will cause UIA to cache a pointer
    // to it.
    ::UiaRaiseStructureChangedEvent(
        test_node_.Get(), StructureChangeType_ChildrenInvalidated, nullptr, 0);
  }

  // Close the main window.
  shell()->Close();

  // At this point our test reference should be the only one remaining.
  EXPECT_EQ(test_node_->m_dwRef, 1);
}

}  // namespace content
