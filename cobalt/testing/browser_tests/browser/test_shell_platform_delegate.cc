#include "cobalt/testing/browser_tests/browser/test_shell_platform_delegate.h"

#include "ui/views/test/desktop_test_views_delegate.h"

namespace content {

// The overridden method provides the test-specific ViewsDelegate.
// The base class's Initialize() method will call this version.
std::unique_ptr<views::ViewsDelegate>
TestShellPlatformDelegate::CreateViewsDelegate() {
  return std::make_unique<views::DesktopTestViewsDelegate>();
}

}  // namespace content
