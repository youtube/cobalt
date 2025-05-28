// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/chrome_desktop_impl.h"

#include <stddef.h>
#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/kill.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/devtools_client_impl.h"
#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"
#include "chrome/test/chromedriver/chrome/devtools_http_client.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/target_utils.h"
#include "chrome/test/chromedriver/chrome/web_view_impl.h"
#include "chrome/test/chromedriver/constants/version.h"
#include "chrome/test/chromedriver/net/timeout.h"

#if BUILDFLAG(IS_POSIX)
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {

// Enables wifi and data only, not airplane mode.
const int kDefaultConnectionType = 6;

bool KillProcess(const base::Process& process, bool kill_gracefully) {
#if BUILDFLAG(IS_POSIX)
  if (!kill_gracefully) {
    kill(process.Pid(), SIGKILL);
    base::TimeTicks deadline = base::TimeTicks::Now() + base::Seconds(30);
    while (base::TimeTicks::Now() < deadline) {
      pid_t pid = HANDLE_EINTR(waitpid(process.Pid(), nullptr, WNOHANG));
      if (pid == process.Pid())
        return true;
      if (pid == -1) {
        if (errno == ECHILD) {
          // The wait may fail with ECHILD if another process also waited for
          // the same pid, causing the process state to get cleaned up.
          return true;
        }
        LOG(WARNING) << "Error waiting for process " << process.Pid();
      }
      base::PlatformThread::Sleep(base::Milliseconds(50));
    }
    return false;
  }
#endif

  if (!process.Terminate(0, true)) {
    int exit_code;
    return base::GetTerminationStatus(process.Handle(), &exit_code) !=
        base::TERMINATION_STATUS_STILL_RUNNING;
  }
  return true;
}

}  // namespace

ChromeDesktopImpl::ChromeDesktopImpl(
    BrowserInfo browser_info,
    std::set<WebViewInfo::Type> window_types,
    std::unique_ptr<DevToolsClient> websocket_client,
    std::vector<std::unique_ptr<DevToolsEventListener>>
        devtools_event_listeners,
    std::optional<MobileDevice> mobile_device,
    std::string page_load_strategy,
    base::Process process,
    const base::CommandLine& command,
    base::ScopedTempDir* user_data_dir,
    base::ScopedTempDir* extension_dir,
    bool network_emulation_enabled,
    bool autoaccept_beforeunload,
    bool enable_extension_targets)
    : ChromeImpl(std::move(browser_info),
                 std::move(window_types),
                 std::move(websocket_client),
                 std::move(devtools_event_listeners),
                 std::move(mobile_device),
                 page_load_strategy,
                 autoaccept_beforeunload,
                 enable_extension_targets),
      process_(std::move(process)),
      command_(command),
      network_connection_enabled_(network_emulation_enabled),
      network_connection_(kDefaultConnectionType) {
  if (user_data_dir->IsValid())
    CHECK(user_data_dir_.Set(user_data_dir->Take()));
  if (extension_dir->IsValid())
    CHECK(extension_dir_.Set(extension_dir->Take()));
}

ChromeDesktopImpl::~ChromeDesktopImpl() {
  if (!quit_) {
    base::FilePath user_data_dir = user_data_dir_.Take();
    base::FilePath extension_dir = extension_dir_.Take();
    LOG(WARNING) << kBrowserShortName
                 << " quit unexpectedly, leaving behind temporary directories"
                    "for debugging:";
    if (user_data_dir_.IsValid())
      LOG(WARNING) << kBrowserShortName
                   << " user data directory: " << user_data_dir.value();
    if (extension_dir_.IsValid())
      LOG(WARNING) << kChromeDriverProductShortName
                   << " automation extension directory: "
                   << extension_dir.value();
  }
}

Status ChromeDesktopImpl::WaitForExtensionPageToLoad(
    const std::string& url,
    const base::TimeDelta& timeout_raw,
    bool w3c_compliant) {
  Timeout timeout(timeout_raw);
  WebView* extension_page = nullptr;
  while (!timeout.IsExpired()) {
    std::list<std::string> tabview_ids;
    Status status = GetTopLevelWebViewIds(&tabview_ids, w3c_compliant);
    if (status.IsError()) {
      return status;
    }

    for (auto& tab_id : tabview_ids) {
      WebView* active_page = nullptr;
      status = GetActivePageByWebViewId(tab_id, &active_page,
                                        /*wait_for_page=*/false);
      if (status.IsError()) {
        if (status.code() == kNoActivePage) {
          continue;
        }
        return status;
      }

      std::string page_url = "";
      status = active_page->GetUrl(&page_url);
      if (status.IsError()) {
        return status;
      }

      if (base::StartsWith(page_url, url, base::CompareCase::SENSITIVE)) {
        extension_page = active_page;
        break;
      }
    }
    if (extension_page) {
      break;
    }
    base::PlatformThread::Sleep(base::Milliseconds(100));
  }
  if (extension_page == nullptr) {
    return Status(kUnknownError, "page could not be found: " + url);
  }

  Status status =
      extension_page->WaitForPendingNavigations(std::string(), timeout, false);
  return status;
}

Status ChromeDesktopImpl::GetAsDesktop(ChromeDesktopImpl** desktop) {
  *desktop = this;
  return Status(kOk);
}

std::string ChromeDesktopImpl::GetOperatingSystemName() {
  return base::SysInfo::OperatingSystemName();
}

bool ChromeDesktopImpl::IsMobileEmulationEnabled() const {
  return mobile_device_.has_value() &&
         mobile_device_->device_metrics.has_value();
}

bool ChromeDesktopImpl::HasTouchScreen() const {
  return IsMobileEmulationEnabled();
}

bool ChromeDesktopImpl::IsNetworkConnectionEnabled() const {
  return network_connection_enabled_;
}

Status ChromeDesktopImpl::QuitImpl() {
  // If the Chrome session uses a custom user data directory, try sending a
  // SIGTERM signal before SIGKILL, so that Chrome has a chance to write
  // everything back out to the user data directory and exit cleanly. If we're
  // using a temporary user data directory, we're going to delete the temporary
  // directory anyway, so just send SIGKILL immediately.
  bool kill_gracefully = !user_data_dir_.IsValid();
  // If the Chrome session is being run with --log-net-log, send SIGTERM first
  // to allow Chrome to write out all the net logs to the log path.
  kill_gracefully = kill_gracefully || command_.HasSwitch("log-net-log");
  if (kill_gracefully) {
    Status status = devtools_websocket_client_->SendCommandAndIgnoreResponse(
        "Browser.close", base::Value::Dict());
    // If status is not okay, we will try the old method of KillProcess
    if (status.IsOk() &&
        process_.WaitForExitWithTimeout(base::Seconds(10), nullptr)) {
      return status;
    }
  }

  if (!KillProcess(process_, kill_gracefully))
    return Status(kUnknownError,
                  base::StringPrintf("cannot kill %s", kBrowserShortName));
  return Status(kOk);
}

const base::CommandLine& ChromeDesktopImpl::command() const {
  return command_;
}

int ChromeDesktopImpl::GetNetworkConnection() const {
  return network_connection_;
}

void ChromeDesktopImpl::SetNetworkConnection(
    int network_connection) {
  network_connection_ = network_connection;
}
