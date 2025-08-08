// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/start_host_main.h"

#include <stddef.h>
#include <stdio.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/message_loop/message_pump_type.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "mojo/core/embedder/embedder.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/base/breakpad.h"
#include "remoting/base/logging.h"
#include "remoting/base/url_request_context_getter.h"
#include "remoting/host/setup/corp_host_starter.h"
#include "remoting/host/setup/host_starter.h"
#include "remoting/host/setup/pin_validator.h"
#include "remoting/host/usage_stats_consent.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/transitional_url_loader_factory_owner.h"

#if BUILDFLAG(IS_POSIX)
#include <termios.h>
#include <unistd.h>
#endif  // BUILDFLAG(IS_POSIX)

#if BUILDFLAG(IS_LINUX)
#include "remoting/host/setup/daemon_controller_delegate_linux.h"
#include "remoting/host/setup/start_host_as_root.h"
#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN)
#include "base/process/process_info.h"

#include <windows.h>
#endif  // BUILDFLAG(IS_WIN)

namespace remoting {

namespace {

// True if the host was started successfully.
bool g_started = false;

base::SingleThreadTaskExecutor* g_main_thread_task_executor = nullptr;

// The active RunLoop.
base::RunLoop* g_active_run_loop = nullptr;

void PrintHelpMessage(const char* process_name) {
  // Optional args are shown first as the most common issue is needing to
  // generate the auth-code again and this ordering makes it easy to fix the
  // command line to rerun the tool.
  fprintf(stderr,
          "Please visit https://remotedesktop.google.com/headless for "
          "instructions on running this tool and help generating the command "
          "line arguments.\n"
          "\n"
          "Usage: %s [--name=<host display name>] [--pin=<6+ digit PIN>] "
          "[--host-owner=<host owner email>]"
          "--redirect-url=<redirectURL> --code=<authCode>\n",
          process_name);
}

void PrintCorpHelpMessage(const char* process_name) {
  fprintf(stdout,
          "Too many arguments provided.\nSetting up a machine for a corp user "
          "requires the email address of that user and an optional display "
          "name.\nExample usage: %s --corp-user=<user_email_address> "
          "[--display-name=corp-machine-name]\n",
          process_name);
}

// Lets us hide the PIN that a user types.
void SetEcho(bool echo) {
#if BUILDFLAG(IS_WIN)
  DWORD mode;
  HANDLE console_handle = GetStdHandle(STD_INPUT_HANDLE);
  if (!GetConsoleMode(console_handle, &mode)) {
    LOG(ERROR) << "GetConsoleMode failed";
    return;
  }
  SetConsoleMode(console_handle,
                 (mode & ~ENABLE_ECHO_INPUT) | (echo ? ENABLE_ECHO_INPUT : 0));
#else
  termios term;
  tcgetattr(STDIN_FILENO, &term);
  if (echo) {
    term.c_lflag |= ECHO;
  } else {
    term.c_lflag &= ~ECHO;
  }
  tcsetattr(STDIN_FILENO, TCSANOW, &term);
#endif  // !BUILDFLAG(IS_WIN)
}

// Reads a newline-terminated string from stdin.
std::string ReadString(bool no_echo) {
  if (no_echo) {
    SetEcho(false);
  }
  const int kMaxLen = 1024;
  std::string str(kMaxLen, 0);
  char* result = fgets(&str[0], kMaxLen, stdin);
  if (no_echo) {
    printf("\n");
    SetEcho(true);
  }
  if (!result) {
    return std::string();
  }
  size_t newline_index = str.find('\n');
  if (newline_index != std::string::npos) {
    str[newline_index] = '\0';
  }
  str.resize(strlen(&str[0]));
  return str;
}

// Called when the HostStarter has finished.
void OnDone(HostStarter::Result result) {
  if (!g_main_thread_task_executor->task_runner()->BelongsToCurrentThread()) {
    g_main_thread_task_executor->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&OnDone, result));
    return;
  }
  switch (result) {
    case HostStarter::START_COMPLETE:
      g_started = true;
      printf("Host started successfully.\n");
      break;
    case HostStarter::NETWORK_ERROR:
      fprintf(stderr, "Couldn't start host: network error.\n");
      break;
    case HostStarter::OAUTH_ERROR:
      fprintf(stderr, "Couldn't start host: OAuth error.\n");
      break;
    case HostStarter::START_ERROR:
      fprintf(stderr, "Couldn't start host.\n");
      break;
    default:
      NOTREACHED() << "Unexpected HostStarter result: " << result;
  }

  g_active_run_loop->Quit();
}

bool InitializeHostStarterParams(HostStarter::Params& params,
                                 const base::CommandLine* command_line) {
  params.id = command_line->GetSwitchValueASCII("host-id");
  params.name = command_line->GetSwitchValueASCII("name");
  params.pin = command_line->GetSwitchValueASCII("pin");
  params.auth_code = command_line->GetSwitchValueASCII("code");
  params.redirect_url = command_line->GetSwitchValueASCII("redirect-url");
  params.owner_email =
      base::ToLowerASCII(command_line->GetSwitchValueASCII("host-owner"));
  params.enable_crash_reporting =
      !command_line->HasSwitch("disable-crash-reporting");

  if (params.auth_code.empty() || params.redirect_url.empty()) {
    return false;
  }

  if (params.name.empty()) {
    fprintf(stdout, "Enter a name for this computer: ");
    fflush(stdout);
    params.name = ReadString(false);
  }

  if (params.pin.empty()) {
    while (true) {
      fprintf(stdout, "Enter a PIN of at least six digits: ");
      fflush(stdout);
      params.pin = ReadString(true);
      if (!remoting::IsPinValid(params.pin)) {
        fprintf(stdout,
                "Please use a PIN consisting of at least six digits.\n");
        fflush(stdout);
        continue;
      }
      std::string pin_confirmation;
      fprintf(stdout, "Enter the same PIN again: ");
      fflush(stdout);
      pin_confirmation = ReadString(true);
      if (params.pin != pin_confirmation) {
        fprintf(stdout, "You entered different PINs.\n");
        fflush(stdout);
        continue;
      }
      break;
    }
  } else {
    if (!remoting::IsPinValid(params.pin)) {
      fprintf(stderr, "Please use a PIN consisting of at least six digits.\n");
      return false;
    }
  }

  return true;
}

bool InitializeCorpMachineParams(HostStarter::Params& params,
                                 const base::CommandLine* command_line) {
  size_t corp_arg_count = 1;
  params.owner_email =
      base::ToLowerASCII(command_line->GetSwitchValueASCII("corp-user"));
  // Crash reporting is always enabled for this flow.
  params.enable_crash_reporting = true;

  if (command_line->HasSwitch("display-name")) {
    corp_arg_count++;
    params.name = command_line->GetSwitchValueASCII("display-name");
  }

  // Allow debugging switches.
  if (command_line->HasSwitch("v")) {
    corp_arg_count++;
  }
  if (command_line->HasSwitch("vmodule")) {
    corp_arg_count++;
  }

  if (command_line->GetSwitches().size() > corp_arg_count) {
    return false;
  }

  return true;
}

}  // namespace

int StartHostMain(int argc, char** argv) {
#if BUILDFLAG(IS_LINUX)
  // Minimize the amount of code that runs as root on Posix systems.
  if (getuid() == 0) {
    return remoting::StartHostAsRoot(argc, argv);
  }
#endif  // BUILDFLAG(IS_LINUX)

  // google_apis::GetOAuth2ClientID/Secret need a static CommandLine.
  base::CommandLine::Init(argc, argv);
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  // This object instance is required by Chrome code (for example,
  // FilePath, LazyInstance, MessageLoop).
  base::AtExitManager exit_manager;

  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  logging::InitLogging(settings);

#if defined(REMOTING_ENABLE_BREAKPAD)
  if (IsUsageStatsAllowed()) {
    InitializeCrashReporting();
  }
#endif  // defined(REMOTING_ENABLE_BREAKPAD)

  base::ThreadPoolInstance::CreateAndStartWithDefaultParams(
      "RemotingHostSetup");

  mojo::core::Init();

#if BUILDFLAG(IS_LINUX)
  if (command_line->HasSwitch("no-start")) {
    // On Linux, registering the host with systemd and starting it is the only
    // reason start_host requires root. The --no-start options skips that final
    // step, allowing it to be run non-interactively if the parent process has
    // root and can do complete the setup itself. Since this functionality is
    // Linux-specific, it isn't plumbed through the platform-independent daemon
    // controller code, and must be configured on the Linux delegate explicitly.
    DaemonControllerDelegateLinux::set_start_host_after_setup(false);
  }
#endif  // BUILDFLAG(IS_LINUX)
#if BUILDFLAG(IS_WIN)
  // The tool must be run elevated on Windows so the host has access to the
  // directories used to store the configuration JSON files.
  if (!base::IsCurrentProcessElevated()) {
    fprintf(stderr, "Error: %s must be run as an elevated process.", argv[0]);
    return 1;
  }
#endif  // BUILDFLAG(IS_WIN)

  if (command_line->HasSwitch("help") || command_line->HasSwitch("h") ||
      command_line->HasSwitch("?") || !command_line->GetArgs().empty()) {
    PrintHelpMessage(argv[0]);
    return 1;
  }

  HostStarter::Params params;
  bool use_corp_machine_flow = command_line->HasSwitch("corp-user");
  if (use_corp_machine_flow) {
    if (!InitializeCorpMachineParams(params, command_line)) {
      PrintCorpHelpMessage(argv[0]);
      return 1;
    }
  } else if (!InitializeHostStarterParams(params, command_line)) {
    PrintHelpMessage(argv[0]);
    return 1;
  }

  // Provide SingleThreadTaskExecutor and threads for the
  // URLRequestContextGetter.
  base::SingleThreadTaskExecutor main_thread_task_executor;
  g_main_thread_task_executor = &main_thread_task_executor;
  base::Thread::Options io_thread_options(base::MessagePumpType::IO, 0);
  base::Thread io_thread("IO thread");
  io_thread.StartWithOptions(std::move(io_thread_options));

  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter(
      new remoting::URLRequestContextGetter(io_thread.task_runner()));
  network::TransitionalURLLoaderFactoryOwner url_loader_factory_owner(
      url_request_context_getter, /*is_trusted=*/use_corp_machine_flow);

  // Start the host.
  std::unique_ptr<HostStarter> host_starter;
  if (use_corp_machine_flow) {
    host_starter =
        ProvisionCorpMachine(url_loader_factory_owner.GetURLLoaderFactory());
  } else {
    host_starter =
        HostStarter::Create(url_loader_factory_owner.GetURLLoaderFactory());
  }

  host_starter->StartHost(std::move(params), base::BindOnce(&OnDone));

  // Run the task executor until the StartHost completion callback.
  base::RunLoop run_loop;
  g_active_run_loop = &run_loop;
  run_loop.Run();

  g_main_thread_task_executor = nullptr;
  g_active_run_loop = nullptr;

  // Destroy the HostStarter and URLRequestContextGetter before stopping the
  // IO thread.
  host_starter.reset();
  url_request_context_getter = nullptr;

  io_thread.Stop();

  return g_started ? 0 : 1;
}

}  // namespace remoting
