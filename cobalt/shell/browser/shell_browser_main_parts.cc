// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/shell/browser/shell_browser_main_parts.h"

#include <utility>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/current_thread.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "cobalt/shell/android/shell_descriptors.h"
#include "cobalt/shell/browser/shell.h"
#include "cobalt/shell/browser/shell_browser_context.h"
#include "cobalt/shell/browser/shell_devtools_manager_delegate.h"
#include "cobalt/shell/browser/shell_platform_delegate.h"
#include "cobalt/shell/common/shell_switches.h"
#include "components/performance_manager/embedder/graph_features.h"
#include "components/performance_manager/embedder/performance_manager_lifetime.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/first_party_sets_handler.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/url_constants.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "net/base/filename_util.h"
#include "net/base/net_module.h"
#include "net/grit/net_resources.h"
#include "ui/base/buildflags.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/crash/content/browser/child_exit_observer_android.h"
#include "components/crash/content/browser/child_process_crash_observer_android.h"
#include "net/android/network_change_notifier_factory_android.h"
#include "net/base/network_change_notifier.h"
#endif

#if defined(USE_AURA) && (BUILDFLAG(IS_LINUX))
#include "ui/base/ime/init/input_method_initializer.h"  // nogncheck
#endif

#if BUILDFLAG(IS_LINUX)
#include "device/bluetooth/dbus/dbus_bluez_manager_wrapper_linux.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "ui/linux/linux_ui.h"          // nogncheck
#include "ui/linux/linux_ui_factory.h"  // nogncheck
#endif

<<<<<<< HEAD
#if defined(RUN_BROWSER_TESTS)
#include "cobalt/shell/common/shell_test_switches.h"  // nogncheck
#endif  // defined(RUN_BROWSER_TESTS)

#if BUILDFLAG(IS_STARBOARD)
#include "cobalt/shell/common/device_authentication.h"
#endif

=======
>>>>>>> 701c780c680 (Remove test logics and GN flag from Cobalt target (#8982))
namespace content {

namespace {
GURL GetStartupURL() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kBrowserTest)) {
    return GURL();
  }

#if BUILDFLAG(IS_ANDROID)
  // Delay renderer creation on Android until surface is ready.
  return GURL();
#else
  GURL initial_url = GURL(switches::kDefaultURL);
  const base::CommandLine::StringVector& args = command_line->GetArgs();
  if (!args.empty()) {
    std::string url_string = args[0];
    GURL url = GURL(url_string);
    if (url.is_valid() && url.has_scheme()) {
      initial_url = url;
    } else {
      LOG(WARNING) << "URL \"" << url_string
                   << "\" from parameter is not valid, open it as a file.";
      return net::FilePathToFileURL(
          base::MakeAbsoluteFilePath(base::FilePath(url_string)));
    }
  }

#if BUILDFLAG(IS_STARBOARD)
  initial_url = GetDeviceAuthenticationSignedURL(initial_url);
#endif
  return initial_url;
#endif
}

scoped_refptr<base::RefCountedMemory> PlatformResourceProvider(int key) {
  if (key == IDR_DIR_HEADER_HTML) {
    return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
        IDR_DIR_HEADER_HTML);
  }
  return nullptr;
}

}  // namespace

ShellBrowserMainParts::ShellBrowserMainParts() = default;

ShellBrowserMainParts::~ShellBrowserMainParts() = default;

void ShellBrowserMainParts::PostCreateMainMessageLoop() {
#if BUILDFLAG(IS_LINUX)
  bluez::DBusBluezManagerWrapperLinux::Initialize();
#endif
}

int ShellBrowserMainParts::PreEarlyInitialization() {
#if defined(USE_AURA) && (BUILDFLAG(IS_LINUX))
  ui::InitializeInputMethodForTesting();
#endif
#if BUILDFLAG(IS_ANDROID)
  net::NetworkChangeNotifier::SetFactory(
      new net::NetworkChangeNotifierFactoryAndroid());
#endif
  return RESULT_CODE_NORMAL_EXIT;
}

void ShellBrowserMainParts::InitializeBrowserContexts() {
  set_browser_context(new ShellBrowserContext(false));
  set_off_the_record_browser_context(new ShellBrowserContext(true));
}

void ShellBrowserMainParts::InitializeMessageLoopContext() {
  Shell::CreateNewWindow(browser_context_.get(), GetStartupURL(), nullptr,
                         gfx::Size(),
#if BUILDFLAG(IS_ANDROID)
                         false /* create_splash_screen_web_contents */
#else
                         switches::ShouldCreateSplashScreen()
#endif  // BUILDFLAG(IS_ANDROID)
  );
}

void ShellBrowserMainParts::ToolkitInitialized() {
#if BUILDFLAG(IS_LINUX)
  ui::LinuxUi::SetInstance(ui::GetDefaultLinuxUi());
#endif
}

int ShellBrowserMainParts::PreCreateThreads() {
#if BUILDFLAG(IS_ANDROID)
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  child_exit_observer_ = std::make_unique<crash_reporter::ChildExitObserver>();
  if (command_line->HasSwitch(switches::kEnableCrashReporter)) {
    child_exit_observer_->RegisterClient(
        std::make_unique<crash_reporter::ChildProcessCrashObserver>());
  }
#endif
  return 0;
}

void ShellBrowserMainParts::PostCreateThreads() {
  performance_manager_lifetime_ =
      std::make_unique<performance_manager::PerformanceManagerLifetime>(
          performance_manager::GraphFeatures::WithMinimal(), base::DoNothing());
}

int ShellBrowserMainParts::PreMainMessageLoopRun() {
  InitializeBrowserContexts();
  Shell::Initialize(CreateShellPlatformDelegate());
  net::NetModule::SetResourceProvider(PlatformResourceProvider);
  ShellDevToolsManagerDelegate::StartHttpHandler(browser_context_.get());
  InitializeMessageLoopContext();
  return 0;
}

void ShellBrowserMainParts::WillRunMainMessageLoop(
    std::unique_ptr<base::RunLoop>& run_loop) {
  Shell::SetMainMessageLoopQuitClosure(run_loop->QuitClosure());
}

void ShellBrowserMainParts::PostMainMessageLoopRun() {
  DCHECK_EQ(Shell::windows().size(), 0u);
  ShellDevToolsManagerDelegate::StopHttpHandler();
  browser_context_.reset();
  off_the_record_browser_context_.reset();
#if BUILDFLAG(IS_LINUX)
  ui::LinuxUi::SetInstance(nullptr);
#endif
  performance_manager_lifetime_.reset();
}

void ShellBrowserMainParts::PostDestroyThreads() {
#if BUILDFLAG(IS_LINUX)
  device::BluetoothAdapterFactory::Shutdown();
  bluez::DBusBluezManagerWrapperLinux::Shutdown();
#endif
}

std::unique_ptr<ShellPlatformDelegate>
ShellBrowserMainParts::CreateShellPlatformDelegate() {
  return std::make_unique<ShellPlatformDelegate>();
}

}  // namespace content
