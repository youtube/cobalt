// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/runtime/chrome_runtime_api_delegate.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "components/update_client/update_query_params.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/warning_service.h"
#include "extensions/browser/warning_set.h"
#include "extensions/common/api/runtime.h"
#include "extensions/common/constants.h"
#include "extensions/common/manifest.h"
#include "net/base/backoff_entry.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/components/kiosk/kiosk_utils.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#endif

using extensions::Extension;
using extensions::ExtensionSystem;
using extensions::ExtensionUpdater;

using extensions::api::runtime::PlatformInfo;

namespace {

// If an extension reloads itself within this many milliseconds of reloading
// itself, the reload is considered suspiciously fast.
const int kFastReloadTime = 10000;

// Same as above, but we shorten the fast reload interval for unpacked
// extensions for ease of testing.
const int kUnpackedFastReloadTime = 1000;

// A holder class for the policy we use for exponential backoff of update check
// requests.
class BackoffPolicy {
 public:
  BackoffPolicy();
  ~BackoffPolicy();

  // Returns the actual policy to use.
  static const net::BackoffEntry::Policy* Get();

 private:
  net::BackoffEntry::Policy policy_;
};

// We use a LazyInstance since one of the the policy values references an
// extern symbol, which would cause a static initializer to be generated if we
// just declared the policy struct as a static variable.
base::LazyInstance<BackoffPolicy>::DestructorAtExit g_backoff_policy =
    LAZY_INSTANCE_INITIALIZER;

BackoffPolicy::BackoffPolicy() {
  policy_ = {
      // num_errors_to_ignore
      0,

      // initial_delay_ms (note that we set 'always_use_initial_delay' to false
      // below)
      1000 * extensions::kDefaultUpdateFrequencySeconds,

      // multiply_factor
      1,

      // jitter_factor
      0.1,

      // maximum_backoff_ms (-1 means no maximum)
      -1,

      // entry_lifetime_ms (-1 means never discard)
      -1,

      // always_use_initial_delay
      false,
  };
}

BackoffPolicy::~BackoffPolicy() = default;

// static
const net::BackoffEntry::Policy* BackoffPolicy::Get() {
  return &g_backoff_policy.Get().policy_;
}

const base::TickClock* g_test_clock = nullptr;

}  // namespace

struct ChromeRuntimeAPIDelegate::UpdateCheckInfo {
  std::unique_ptr<net::BackoffEntry> backoff =
      std::make_unique<net::BackoffEntry>(BackoffPolicy::Get(), g_test_clock);
  std::vector<UpdateCheckCallback> callbacks;
};

ChromeRuntimeAPIDelegate::ChromeRuntimeAPIDelegate(
    content::BrowserContext* context)
    : browser_context_(context), registered_for_updates_(false) {
  extension_registry_observation_.Observe(
      extensions::ExtensionRegistry::Get(browser_context_));
}

ChromeRuntimeAPIDelegate::~ChromeRuntimeAPIDelegate() = default;

// static
void ChromeRuntimeAPIDelegate::set_tick_clock_for_tests(
    const base::TickClock* clock) {
  g_test_clock = clock;
}

void ChromeRuntimeAPIDelegate::AddUpdateObserver(
    extensions::UpdateObserver* observer) {
  registered_for_updates_ = true;
  ExtensionSystem::Get(browser_context_)
      ->extension_service()
      ->AddUpdateObserver(observer);
}

void ChromeRuntimeAPIDelegate::RemoveUpdateObserver(
    extensions::UpdateObserver* observer) {
  if (registered_for_updates_) {
    ExtensionSystem::Get(browser_context_)
        ->extension_service()
        ->RemoveUpdateObserver(observer);
  }
}

void ChromeRuntimeAPIDelegate::ReloadExtension(
    const std::string& extension_id) {
  const Extension* extension =
      extensions::ExtensionRegistry::Get(browser_context_)
          ->GetInstalledExtension(extension_id);
  int fast_reload_time = kFastReloadTime;
  int fast_reload_count = extensions::RuntimeAPI::kFastReloadCount;

  // If an extension is unpacked, we allow for a faster reload interval
  // and more fast reload attempts before terminating the extension.
  // This is intended to facilitate extension testing for developers.
  if (extensions::Manifest::IsUnpackedLocation(extension->location())) {
    fast_reload_time = kUnpackedFastReloadTime;
    fast_reload_count = extensions::RuntimeAPI::kUnpackedFastReloadCount;
  }

  std::pair<base::TimeTicks, int>& reload_info =
      last_reload_time_[extension_id];
  base::TimeTicks now =
      g_test_clock ? g_test_clock->NowTicks() : base::TimeTicks::Now();
  if (reload_info.first.is_null() ||
      (now - reload_info.first).InMilliseconds() > fast_reload_time) {
    reload_info.second = 0;
  } else {
    reload_info.second++;
  }
  if (!reload_info.first.is_null()) {
    UMA_HISTOGRAM_LONG_TIMES("Extensions.RuntimeReloadTime",
                             now - reload_info.first);
  }
  UMA_HISTOGRAM_COUNTS_100("Extensions.RuntimeReloadFastCount",
                           reload_info.second);
  reload_info.first = now;

  extensions::ExtensionService* service =
      ExtensionSystem::Get(browser_context_)->extension_service();

  if (reload_info.second >= fast_reload_count) {
    // Unloading an extension clears all warnings, so first terminate the
    // extension, and then add the warning. Since this is called from an
    // extension function unloading the extension has to be done
    // asynchronously. Fortunately PostTask guarentees FIFO order so just
    // post both tasks.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&extensions::ExtensionService::TerminateExtension,
                       service->AsExtensionServiceWeakPtr(), extension_id));
    extensions::WarningSet warnings;
    warnings.insert(
        extensions::Warning::CreateReloadTooFrequentWarning(extension_id));
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&extensions::WarningService::NotifyWarningsOnUI,
                       browser_context_, warnings));
  } else {
    // We can't call ReloadExtension directly, since when this method finishes
    // it tries to decrease the reference count for the extension, which fails
    // if the extension has already been reloaded; so instead we post a task.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&extensions::ExtensionService::ReloadExtension,
                       service->AsExtensionServiceWeakPtr(), extension_id));
  }
}

bool ChromeRuntimeAPIDelegate::CheckForUpdates(const std::string& extension_id,
                                               UpdateCheckCallback callback) {
  ExtensionSystem* system = ExtensionSystem::Get(browser_context_);
  extensions::ExtensionService* service = system->extension_service();
  ExtensionUpdater* updater = service->updater();
  if (!updater) {
    return false;
  }

  UpdateCheckInfo& info = update_check_info_[extension_id];

  // If not enough time has elapsed, or we have 10 or more outstanding calls,
  // return a status of throttled.
  if (info.backoff->ShouldRejectRequest() || info.callbacks.size() >= 10) {
    UpdateCheckResult result = UpdateCheckResult(
        extensions::api::runtime::RequestUpdateCheckStatus::kThrottled, "");
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
  } else {
    info.callbacks.push_back(std::move(callback));

    extensions::ExtensionUpdater::CheckParams params;
    params.ids = {extension_id};
    params.update_found_callback =
        base::BindRepeating(&ChromeRuntimeAPIDelegate::OnExtensionUpdateFound,
                            base::Unretained(this));
    params.callback =
        base::BindOnce(&ChromeRuntimeAPIDelegate::UpdateCheckComplete,
                       base::Unretained(this), extension_id);
    updater->CheckNow(std::move(params));
  }
  return true;
}

void ChromeRuntimeAPIDelegate::OpenURL(const GURL& uninstall_url) {
  Profile* profile = Profile::FromBrowserContext(browser_context_);
  Browser* browser = chrome::FindLastActiveWithProfile(profile);
  if (!browser) {
    browser = Browser::Create(Browser::CreateParams(profile, false));
  }
  if (!browser) {
    return;
  }

  NavigateParams params(browser, uninstall_url,
                        ui::PAGE_TRANSITION_CLIENT_REDIRECT);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.user_gesture = false;
  Navigate(&params);
}

bool ChromeRuntimeAPIDelegate::GetPlatformInfo(PlatformInfo* info) {
  const char* os = update_client::UpdateQueryParams::GetOS();
  if (strcmp(os, "mac") == 0) {
    info->os = extensions::api::runtime::PlatformOs::kMac;
  } else if (strcmp(os, "win") == 0) {
    info->os = extensions::api::runtime::PlatformOs::kWin;
  } else if (strcmp(os, "cros") == 0) {
    info->os = extensions::api::runtime::PlatformOs::kCros;
  } else if (strcmp(os, "linux") == 0) {
    info->os = extensions::api::runtime::PlatformOs::kLinux;
  } else if (strcmp(os, "openbsd") == 0) {
    info->os = extensions::api::runtime::PlatformOs::kOpenbsd;
  } else if (strcmp(os, "fuchsia") == 0) {
    info->os = extensions::api::runtime::PlatformOs::kFuchsia;
  } else {
    NOTREACHED() << "Platform not supported: " << os;
    return false;
  }

  const char* arch = update_client::UpdateQueryParams::GetArch();
  if (strcmp(arch, "arm") == 0) {
    info->arch = extensions::api::runtime::PlatformArch::kArm;
  } else if (strcmp(arch, "arm64") == 0) {
    info->arch = extensions::api::runtime::PlatformArch::kArm64;
  } else if (strcmp(arch, "x86") == 0) {
    info->arch = extensions::api::runtime::PlatformArch::kX86_32;
  } else if (strcmp(arch, "x64") == 0) {
    info->arch = extensions::api::runtime::PlatformArch::kX86_64;
  } else if (strcmp(arch, "mipsel") == 0) {
    info->arch = extensions::api::runtime::PlatformArch::kMips;
  } else if (strcmp(arch, "mips64el") == 0) {
    info->arch = extensions::api::runtime::PlatformArch::kMips64;
  } else {
    NOTREACHED();
    return false;
  }

  const char* nacl_arch = update_client::UpdateQueryParams::GetNaclArch();
  if (strcmp(nacl_arch, "arm") == 0) {
    info->nacl_arch = extensions::api::runtime::PlatformNaclArch::kArm;
  } else if (strcmp(nacl_arch, "x86-32") == 0) {
    info->nacl_arch = extensions::api::runtime::PlatformNaclArch::kX86_32;
  } else if (strcmp(nacl_arch, "x86-64") == 0) {
    info->nacl_arch = extensions::api::runtime::PlatformNaclArch::kX86_64;
  } else if (strcmp(nacl_arch, "mips32") == 0) {
    info->nacl_arch = extensions::api::runtime::PlatformNaclArch::kMips;
  } else if (strcmp(nacl_arch, "mips64") == 0) {
    info->nacl_arch = extensions::api::runtime::PlatformNaclArch::kMips64;
  } else {
    NOTREACHED();
    return false;
  }

  return true;
}

bool ChromeRuntimeAPIDelegate::RestartDevice(std::string* error_message) {
#if BUILDFLAG(IS_CHROMEOS)
  if (chromeos::IsKioskSession()) {
    chromeos::PowerManagerClient::Get()->RequestRestart(
        power_manager::REQUEST_RESTART_API, "chrome.runtime API");
    return true;
  }
#endif

  *error_message = "Function available only for ChromeOS kiosk mode.";
  return false;
}

bool ChromeRuntimeAPIDelegate::OpenOptionsPage(
    const Extension* extension,
    content::BrowserContext* browser_context) {
  return extensions::ExtensionTabUtil::OpenOptionsPageFromAPI(extension,
                                                              browser_context);
}

void ChromeRuntimeAPIDelegate::OnExtensionUpdateFound(
    const std::string& id,
    const base::Version& version) {
  if (version.IsValid()) {
    UpdateCheckResult result = UpdateCheckResult(
        extensions::api::runtime::RequestUpdateCheckStatus::kUpdateAvailable,
        version.GetString());
    CallUpdateCallbacks(id, std::move(result));
  }
}

void ChromeRuntimeAPIDelegate::OnExtensionInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update) {
  if (!is_update) {
    return;
  }
  auto info = update_check_info_.find(extension->id());
  if (info != update_check_info_.end()) {
    info->second.backoff->Reset();
  }
}

void ChromeRuntimeAPIDelegate::UpdateCheckComplete(
    const std::string& extension_id) {
  ExtensionSystem* system = ExtensionSystem::Get(browser_context_);
  extensions::ExtensionService* service = system->extension_service();
  const Extension* update = service->GetPendingExtensionUpdate(extension_id);
  UpdateCheckInfo& info = update_check_info_[extension_id];

  // We always inform the BackoffEntry of a "failure" here, because we only
  // want to consider an update check request a success from a throttling
  // standpoint once the extension goes on to actually update to a new
  // version. See OnExtensionInstalled for where we reset the BackoffEntry.
  info.backoff->InformOfRequest(false);

  if (update) {
    UpdateCheckResult result = UpdateCheckResult(
        extensions::api::runtime::RequestUpdateCheckStatus::kUpdateAvailable,
        update->VersionString());
    CallUpdateCallbacks(extension_id, std::move(result));
  } else {
    UpdateCheckResult result = UpdateCheckResult(
        extensions::api::runtime::RequestUpdateCheckStatus::kNoUpdate, "");
    CallUpdateCallbacks(extension_id, std::move(result));
  }
}

void ChromeRuntimeAPIDelegate::CallUpdateCallbacks(
    const std::string& extension_id,
    const UpdateCheckResult& result) {
  auto it = update_check_info_.find(extension_id);
  if (it == update_check_info_.end()) {
    return;
  }
  std::vector<UpdateCheckCallback> callbacks;
  it->second.callbacks.swap(callbacks);
  for (auto& callback : callbacks) {
    std::move(callback).Run(result);
  }
}
