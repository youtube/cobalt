#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/test/launcher/test_launcher.h"
#include "cobalt/testing/browser_tests/content_browser_test_shell_main_delegate.h"
#include "content/public/test/content_test_suite_base.h"
#include "content/public/test/test_launcher.h"
#include "starboard/event.h"
#include "ui/ozone/platform/starboard/platform_event_source_starboard.h"
#include "ui/ozone/public/ozone_platform.h"

namespace content {

// Global variables for the main delegate.
ContentBrowserTestShellMainDelegate* g_main_delegate = nullptr;

class ContentBrowserTestSuite : public ContentTestSuiteBase {
 public:
  ContentBrowserTestSuite(int argc, char** argv)
      : ContentTestSuiteBase(argc, argv) {}

 protected:
  void Initialize() override {
    ContentTestSuiteBase::Initialize();
    ui::OzonePlatform::InitParams params;
    params.single_process = true;
    ui::OzonePlatform::InitializeForUI(params);
  }
};

// A basic implementation of TestLauncherDelegate that runs the test suite.
class CobaltTestLauncherDelegate : public TestLauncherDelegate {
 public:
  explicit CobaltTestLauncherDelegate(int argc, char** argv)
      : test_suite_(std::make_unique<ContentBrowserTestSuite>(argc, argv)) {}

  int RunTestSuite(int argc, char** argv) override {
    return test_suite_->Run();
  }

 protected:
  ContentMainDelegate* CreateContentMainDelegate() override {
    // The main delegate is created in SbEventHandle.
    return g_main_delegate;
  }

 private:
  std::unique_ptr<ContentBrowserTestSuite> test_suite_;
};

}  // namespace content

void SbEventHandle(const SbEvent* event) {
  using starboard::PlatformEventSourceStarboard;
  static base::AtExitManager* g_exit_manager = nullptr;
  static PlatformEventSourceStarboard* g_platform_event_source = nullptr;

  switch (event->type) {
    case kSbEventTypePreload:
    case kSbEventTypeStart: {
      SbEventStartData* data = static_cast<SbEventStartData*>(event->data);

      g_exit_manager = new base::AtExitManager();
      content::g_main_delegate =
          new content::ContentBrowserTestShellMainDelegate();
      g_platform_event_source = new PlatformEventSourceStarboard();

      // We need to set the ozone platform before initializing the UI.
      base::CommandLine::Init(data->argument_count,
                              (const char**)data->argument_values);
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          "ozone-platform", "starboard");

      content::CobaltTestLauncherDelegate launcher_delegate(
          data->argument_count, (char**)data->argument_values);
      content::LaunchTests(&launcher_delegate,
                           base::NumParallelJobs(/*cores_per_job=*/2),
                           data->argument_count,
                           (char**)data->argument_values);
      break;
    }
    case kSbEventTypeStop: {
      delete content::g_main_delegate;
      content::g_main_delegate = nullptr;
      delete g_platform_event_source;
      g_platform_event_source = nullptr;
      delete g_exit_manager;
      g_exit_manager = nullptr;
      break;
    }
    case kSbEventTypeInput:
    case kSbEventTypeWindowSizeChanged:
      if (g_platform_event_source) {
        g_platform_event_source->HandleEvent(event);
      }
      break;
    default:
      break;
  }
}

int main(int argc, char** argv) {
  return SbRunStarboardMain(argc, argv, SbEventHandle);
}
