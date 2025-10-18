#ifndef COBALT_TESTING_BROWSER_TESTS_BROWSER_SHELL_TEST_PLATFORM_DELEGATE_H_
#define COBALT_TESTING_BROWSER_TESTS_BROWSER_SHELL_TEST_PLATFORM_DELEGATE_H_

#include "cobalt/shell/browser/shell_platform_delegate.h"

namespace content {

class ShellTestPlatformDelegate : public ShellPlatformDelegate {
 public:
  ShellTestPlatformDelegate() = default;
  ~ShellTestPlatformDelegate() override = default;

 protected:
  std::unique_ptr<views::ViewsDelegate> CreateViewsDelegate() override;
};

}  // namespace content

#endif  // COBALT_TESTING_BROWSER_TESTS_BROWSER_SHELL_TEST_PLATFORM_DELEGATE_H_
