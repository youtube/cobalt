#ifndef COBALT_TESTING_BROWSER_TESTS_BROWSER_SHELL_CONTENT_BROWSER_TEST_CLIENT_H_
#define COBALT_TESTING_BROWSER_TESTS_BROWSER_SHELL_CONTENT_BROWSER_TEST_CLIENT_H_

#include "cobalt/shell/browser/shell_content_browser_client.h"

namespace content {

class ShellContentBrowserTestClient : public ShellContentBrowserClient {
 public:
  ShellContentBrowserTestClient();
  ~ShellContentBrowserTestClient() override;

  // ShellContentBrowserClient overrides.
  void BindBrowserControlInterface(mojo::ScopedMessagePipeHandle pipe) override;
  bool HasCustomSchemeHandler(content::BrowserContext* browser_context,
                              const std::string& scheme) override;
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
  CreateURLLoaderThrottles(
      const network::ResourceRequest& request,
      BrowserContext* browser_context,
      const base::RepeatingCallback<WebContents*()>& wc_getter,
      NavigationUIData* navigation_ui_data,
      int frame_tree_node_id) override;
  void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                      int child_process_id) override;
  void CreateFeatureListAndFieldTrials() override;

 private:
  void SetUpFieldTrials();
};

// The delay for sending reports when running with --run-web-tests
constexpr base::TimeDelta kReportingDeliveryIntervalTimeForWebTests =
    base::Milliseconds(100);

}  // namespace content

#endif  // COBALT_TESTING_BROWSER_TESTS_BROWSER_SHELL_CONTENT_BROWSER_TEST_CLIENT_H_
