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

#include "cobalt/shell/app/shell_main_delegate.h"

#include <iostream>
#include <tuple>
#include <utility>

#include "base/base_paths.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/cpu.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/current_process.h"
#include "base/trace_event/trace_log.h"
#include "build/build_config.h"
#if !BUILDFLAG(IS_ANDROIDTV)
#include "components/crash/core/common/crash_key.h"
#endif
#include "cobalt/shell/app/shell_crash_reporter_client.h"
#include "cobalt/shell/browser/shell_content_browser_client.h"
#include "cobalt/shell/browser/shell_paths.h"
#include "cobalt/shell/common/shell_content_client.h"
#include "cobalt/shell/common/shell_switches.h"
#include "cobalt/shell/renderer/shell_content_renderer_client.h"
#include "components/memory_system/initializer.h"
#include "components/memory_system/parameters.h"
#include "content/common/content_constants_internal.h"
#include "content/public/app/initialize_mojo_core.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/url_constants.h"
#include "ipc/ipc_buildflags.h"
#include "net/cookies/cookie_monster.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/resource/resource_bundle.h"

#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
#define IPC_MESSAGE_MACROS_LOG_ENABLED
#include "content/public/common/content_ipc_logging.h"
#define IPC_LOG_TABLE_ADD_ENTRY(msg_id, logger) \
  content::RegisterIPCLogger(msg_id, logger)
#endif

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_STARBOARD)
#include "content/web_test/browser/web_test_browser_main_runner.h"  // nogncheck
#include "content/web_test/browser/web_test_content_browser_client.h"  // nogncheck
#include "content/web_test/renderer/web_test_content_renderer_client.h"  // nogncheck
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/apk_assets.h"
#include "base/posix/global_descriptors.h"
#include "cobalt/shell/android/shell_descriptors.h"
#include "content/public/browser/android/compositor.h"
#endif

#if !BUILDFLAG(IS_ANDROIDTV)
#include "components/crash/core/app/crashpad.h"  // nogncheck
#endif

#if BUILDFLAG(IS_APPLE)
#include "cobalt/shell/app/paths_mac.h"
#endif

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
#include "v8/include/v8-wasm-trap-handler-posix.h"
#endif

#if BUILDFLAG(IS_IOS)
#include "cobalt/shell/app/ios/shell_application_ios.h"
#endif

#if defined(RUN_BROWSER_TESTS)
#include "cobalt/shell/common/shell_test_switches.h"            // nogncheck
#include "cobalt/shell/utility/shell_content_utility_client.h"  // nogncheck
#endif  // defined(RUN_BROWSER_TESTS)

namespace {

#if !BUILDFLAG(IS_ANDROIDTV)
base::LazyInstance<content::ShellCrashReporterClient>::Leaky
    g_shell_crash_client = LAZY_INSTANCE_INITIALIZER;
#endif

void InitLogging(const base::CommandLine& command_line) {
  base::FilePath log_filename =
      command_line.GetSwitchValuePath(switches::kLogFile);
  if (log_filename.empty()) {
#if BUILDFLAG(IS_IOS)
    base::PathService::Get(base::DIR_TEMP, &log_filename);
#else
    base::PathService::Get(base::DIR_EXE, &log_filename);
#endif
#if BUILDFLAG(IS_ANDROID)
    log_filename = log_filename.AppendASCII("cobalt_shell.log");
#else
    log_filename = log_filename.AppendASCII("content_shell.log");
#endif
  }

  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_ALL;
  settings.log_file_path = log_filename.value().c_str();
  settings.delete_old = logging::DELETE_OLD_LOG_FILE;
  logging::InitLogging(settings);
  logging::SetLogItems(true /* Process ID */, true /* Thread ID */,
                       true /* Timestamp */, false /* Tick count */);
}

}  // namespace

namespace content {

ShellMainDelegate::ShellMainDelegate(bool is_content_browsertests)
    : is_content_browsertests_(is_content_browsertests) {}

ShellMainDelegate::~ShellMainDelegate() {}

absl::optional<int> ShellMainDelegate::BasicStartupComplete() {
  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();

#if defined(RUN_BROWSER_TESTS)
  if (command_line.HasSwitch("run-layout-test")) {
    std::cerr << std::string(79, '*') << "\n"
              << "* The flag --run-layout-test is obsolete. Please use --"
              << switches::kRunWebTests << " instead. *\n"
              << std::string(79, '*') << "\n";
    command_line.AppendSwitch(switches::kRunWebTests);
  }
#endif  // defined(RUN_BROWSER_TESTS)

#if BUILDFLAG(IS_ANDROID)
  Compositor::Initialize();
#endif

  InitLogging(command_line);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_STARBOARD)
  if (switches::IsRunWebTestsSwitchPresent()) {
    const bool browser_process =
        command_line.GetSwitchValueASCII(switches::kProcessType).empty();
    if (browser_process) {
      web_test_runner_ = std::make_unique<WebTestBrowserMainRunner>();
      web_test_runner_->Initialize();
    }
  }
#endif

  RegisterShellPathProvider();

  return absl::nullopt;
}

bool ShellMainDelegate::ShouldCreateFeatureList(InvokedIn invoked_in) {
  return absl::holds_alternative<InvokedInChildProcess>(invoked_in);
}

bool ShellMainDelegate::ShouldInitializeMojo(InvokedIn invoked_in) {
  return ShouldCreateFeatureList(invoked_in);
}

void ShellMainDelegate::PreSandboxStartup() {
#if defined(ARCH_CPU_ARM_FAMILY) && \
    (BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX))
  // Create an instance of the CPU class to parse /proc/cpuinfo and cache
  // cpu_brand info.
  base::CPU cpu_info;
#endif

// Disable platform crash handling and initialize the crash reporter, if
// requested.
// TODO(crbug.com/1226159): Implement crash reporter integration for Fuchsia.
#if !BUILDFLAG(IS_ANDROIDTV)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableCrashReporter)) {
    std::string process_type =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kProcessType);
    crash_reporter::SetCrashReporterClient(g_shell_crash_client.Pointer());
    // Reporting for sub-processes will be initialized in ZygoteForked.
    if (process_type != switches::kZygoteProcess) {
      crash_reporter::InitializeCrashpad(process_type.empty(), process_type);
#if BUILDFLAG(IS_LINUX)
      crash_reporter::SetFirstChanceExceptionHandler(
          v8::TryHandleWebAssemblyTrapPosix);
#endif
    }
  }
#endif  // !BUILDFLAG(IS_ANDROIDTV)

#if !BUILDFLAG(IS_ANDROIDTV)
  crash_reporter::InitializeCrashKeys();
#endif  // !BUILDFLAG(IS_ANDROIDTV)

  InitializeResourceBundle();
}

absl::variant<int, MainFunctionParams> ShellMainDelegate::RunProcess(
    const std::string& process_type,
    MainFunctionParams main_function_params) {
  // For non-browser process, return and have the caller run the main loop.
  if (!process_type.empty()) {
    return std::move(main_function_params);
  }

  base::CurrentProcess::GetInstance().SetProcessType(
      base::CurrentProcessType::PROCESS_BROWSER);
  base::trace_event::TraceLog::GetInstance()->SetProcessSortIndex(
      kTraceEventBrowserProcessSortIndex);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS) || BUILDFLAG(IS_STARBOARD)
  // On Android and iOS, we defer to the system message loop when the stack
  // unwinds. So here we only create (and leak) a BrowserMainRunner. The
  // shutdown of BrowserMainRunner doesn't happen in Chrome Android/iOS and
  // doesn't work properly on Android/iOS at all.
  std::unique_ptr<BrowserMainRunner> main_runner = BrowserMainRunner::Create();
  // In browser tests, the |main_function_params| contains a |ui_task| which
  // will execute the testing. The task will be executed synchronously inside
  // Initialize() so we don't depend on the BrowserMainRunner being Run().
  int initialize_exit_code =
      main_runner->Initialize(std::move(main_function_params));
  DCHECK_LT(initialize_exit_code, 0)
      << "BrowserMainRunner::Initialize failed in ShellMainDelegate";
  std::ignore = main_runner.release();
  // Return 0 as BrowserMain() should not be called after this, bounce up to
  // the system message loop for ContentShell, and we're already done thanks
  // to the |ui_task| for browser tests.
  return 0;
#else
  if (switches::IsRunWebTestsSwitchPresent()) {
    // Web tests implement their own BrowserMain() replacement.
    web_test_runner_->RunBrowserMain(std::move(main_function_params));
    web_test_runner_.reset();
    // Returning 0 to indicate that we have replaced BrowserMain() and the
    // caller should not call BrowserMain() itself. Web tests do not ever
    // return an error.
    return 0;
  }

  // On non-Android, we can return the |main_function_params| back and have the
  // caller run BrowserMain() normally.
  return std::move(main_function_params);
#endif
}

#if BUILDFLAG(IS_LINUX)
void ShellMainDelegate::ZygoteForked() {
#if !BUILDFLAG(IS_ANDROIDTV)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableCrashReporter)) {
    std::string process_type =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kProcessType);
    crash_reporter::InitializeCrashpad(false, process_type);
    crash_reporter::SetFirstChanceExceptionHandler(
        v8::TryHandleWebAssemblyTrapPosix);
  }
#endif  // !BUILDFLAG(IS_ANDROIDTV)
}
#endif  // BUILDFLAG(IS_LINUX)

void ShellMainDelegate::InitializeResourceBundle() {
#if BUILDFLAG(IS_ANDROID)
  // On Android, the renderer runs with a different UID and can never access
  // the file system. Use the file descriptor passed in at launch time.
  auto* global_descriptors = base::GlobalDescriptors::GetInstance();
  int pak_fd = global_descriptors->MaybeGet(kShellPakDescriptor);
  base::MemoryMappedFile::Region pak_region;
  if (pak_fd >= 0) {
    pak_region = global_descriptors->GetRegion(kShellPakDescriptor);
  } else {
    pak_fd =
        base::android::OpenApkAsset("assets/cobalt_shell.pak", &pak_region);
    // Loaded from disk for browsertests.
    if (pak_fd < 0) {
      base::FilePath pak_file;
      bool r = base::PathService::Get(base::DIR_ANDROID_APP_DATA, &pak_file);
      DCHECK(r);
      pak_file = pak_file.Append(FILE_PATH_LITERAL("paks"));
      pak_file = pak_file.Append(FILE_PATH_LITERAL("cobalt_shell.pak"));
      int flags = base::File::FLAG_OPEN | base::File::FLAG_READ;
      pak_fd = base::File(pak_file, flags).TakePlatformFile();
      pak_region = base::MemoryMappedFile::Region::kWholeFile;
    }
    global_descriptors->Set(kShellPakDescriptor, pak_fd, pak_region);
  }
  DCHECK_GE(pak_fd, 0);
  // TODO(crbug.com/330930): A better way to prevent fdsan error from a double
  // close is to refactor GlobalDescriptors.{Get,MaybeGet} to return
  // "const base::File&" rather than fd itself.
  base::File android_pak_file(pak_fd);
  ui::ResourceBundle::InitSharedInstanceWithPakFileRegion(
      android_pak_file.Duplicate(), pak_region);
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromFileRegion(
      std::move(android_pak_file), pak_region, ui::k100Percent);
#elif BUILDFLAG(IS_APPLE)
  ui::ResourceBundle::InitSharedInstanceWithPakPath(GetResourcesPakFilePath());
#else
  base::FilePath pak_file;
  bool r = base::PathService::Get(base::DIR_ASSETS, &pak_file);
  DCHECK(r);
  pak_file = pak_file.Append(FILE_PATH_LITERAL("cobalt_shell.pak"));
  ui::ResourceBundle::InitSharedInstanceWithPakPath(pak_file);
#endif
}

absl::optional<int> ShellMainDelegate::PreBrowserMain() {
  absl::optional<int> exit_code =
      content::ContentMainDelegate::PreBrowserMain();
  if (exit_code.has_value()) {
    return exit_code;
  }

  return absl::nullopt;
}

absl::optional<int> ShellMainDelegate::PostEarlyInitialization(
    InvokedIn invoked_in) {
  if (!ShouldCreateFeatureList(invoked_in)) {
    // Apply field trial testing configuration since content did not.
    browser_client_->CreateFeatureListAndFieldTrials();
  }
  if (!ShouldInitializeMojo(invoked_in)) {
    InitializeMojoCore();
  }

  // ShellMainDelegate has GWP-ASan as well as Profiling Client disabled.
  // Consequently, we provide no parameters for these two. The memory_system
  // includes the PoissonAllocationSampler dynamically only if the Profiling
  // Client is enabled. However, we are not sure if this is the only user of
  // PoissonAllocationSampler in the ContentShell. Therefore, enforce inclusion
  // at the moment.
  //
  // TODO(https://crbug.com/1411454): Clarify which users of
  // PoissonAllocationSampler we have in the ContentShell. Do we really need to
  // enforce it?
  memory_system::Initializer()
      .SetDispatcherParameters(memory_system::DispatcherParameters::
                                   PoissonAllocationSamplerInclusion::kEnforce,
                               memory_system::DispatcherParameters::
                                   AllocationTraceRecorderInclusion::kIgnore)
      .Initialize(memory_system_);

  return absl::nullopt;
}

ContentClient* ShellMainDelegate::CreateContentClient() {
  content_client_ = std::make_unique<ShellContentClient>();
  return content_client_.get();
}

ContentBrowserClient* ShellMainDelegate::CreateContentBrowserClient() {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_STARBOARD)
  if (switches::IsRunWebTestsSwitchPresent()) {
    browser_client_ = std::make_unique<WebTestContentBrowserClient>();
    return browser_client_.get();
  }
#endif
  browser_client_ = std::make_unique<ShellContentBrowserClient>();
  return browser_client_.get();
}

ContentRendererClient* ShellMainDelegate::CreateContentRendererClient() {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_STARBOARD)
  if (switches::IsRunWebTestsSwitchPresent()) {
    renderer_client_ = std::make_unique<WebTestContentRendererClient>();
    return renderer_client_.get();
  }
#endif
  renderer_client_ = std::make_unique<ShellContentRendererClient>();
  return renderer_client_.get();
}
#if defined(RUN_BROWSER_TESTS)
ContentUtilityClient* ShellMainDelegate::CreateContentUtilityClient() {
  utility_client_ =
      std::make_unique<ShellContentUtilityClient>(is_content_browsertests_);
  return utility_client_.get();
}
#endif  // defined(RUN_BROWSER_TESTS)

}  // namespace content
