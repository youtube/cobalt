#include "cobalt/testing/browser_tests/browser/shell_test_platform_delegate.h"

#include "ui/views/test/desktop_test_views_delegate.h"

namespace content {

// The overridden method provides the test-specific ViewsDelegate.
// The base class's Initialize() method will call this version.
std::unique_ptr<views::ViewsDelegate>
ShellTestPlatformDelegate::CreateViewsDelegate() {
  return std::make_unique<views::DesktopTestViewsDelegate>();
}

}  // namespace content
