#include "cobalt/testing/browser_tests/browser/test_shell_platform_delegate.h"

#include "ui/views/test/desktop_test_views_delegate.h"

namespace content {

TestShellPlatformDelegate::TestShellPlatformDelegate() = default;
TestShellPlatformDelegate::~TestShellPlatformDelegate() = default;

std::unique_ptr<views::ViewsDelegate>
TestShellPlatformDelegate::CreateViewsDelegate() {
  return std::make_unique<views::DesktopTestViewsDelegate>();
}

}  // namespace content
