// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/demo_mode/demo_session.h"

#include <algorithm>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/locale_update_controller.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/hash/md5.h"
#include "base/i18n/string_compare.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/syslog_logging.h"
#include "base/task/thread_pool.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/apps/platform_apps/app_load_service.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/login/demo_mode/demo_components.h"
#include "chrome/browser/ash/login/demo_mode/demo_mode_dimensions.h"
#include "chrome/browser/ash/login/demo_mode/demo_mode_window_closer.h"
#include "chrome/browser/ash/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/ash/login/users/chrome_user_manager.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/system_tray_client_impl.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/language/core/browser/pref_names.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/user_manager/user.h"
#include "components/variations/synthetic_trials.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/common/constants.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/icu/source/common/unicode/uloc.h"
#include "third_party/icu/source/i18n/unicode/coll.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

// The splash screen should be removed either when this timeout passes or the
// screensaver app is shown, whichever comes first.
constexpr base::TimeDelta kRemoveSplashScreenTimeout = base::Seconds(10);

// Global DemoSession instance.
DemoSession* g_demo_session = nullptr;

// Type of demo config forced on for tests.
absl::optional<DemoSession::DemoModeConfig> g_force_demo_config;

// Path relative to the path at which offline demo resources are loaded that
// contains sample photos.
constexpr char kPhotosPath[] = "media/photos";

// Path relative to the path at which offline demo resources are loaded that
// contains splash screen images.
constexpr char kSplashScreensPath[] = "media/splash_screens";

// Returns the list of apps normally pinned by Demo Mode policy that shouldn't
// be pinned if the device is offline.
std::vector<std::string> GetIgnorePinPolicyApps() {
  return {
      // Popular third-party game preinstalled in Demo Mode that is
      // online-only, so shouldn't be featured in the shelf when offline.
      "com.pixonic.wwr.chbkdemo",
      // TODO(michaelpg): YouTube is also pinned as a *default* app.
      extension_misc::kYoutubeAppId,
  };
}

// Copies photos into the Downloads directory.
// TODO(michaelpg): Test this behavior (requires overriding the Downloads
// directory).
void InstallDemoMedia(const base::FilePath& offline_resources_path,
                      const base::FilePath& dest_path) {
  if (offline_resources_path.empty()) {
    LOG(ERROR) << "Offline resources not loaded - no media available.";
    return;
  }

  base::FilePath src_path = offline_resources_path.Append(kPhotosPath);
  if (!base::CopyDirectory(src_path, dest_path, false /* recursive */))
    LOG(ERROR) << "Failed to install demo mode media.";
}

std::string GetSwitchOrDefault(const base::StringPiece& switch_string,
                               const std::string& default_value) {
  auto* const command_line = base::CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(switch_string)) {
    return command_line->GetSwitchValueASCII(switch_string);
  }
  return default_value;
}

// If the current locale is not the default one, ensure it is reverted to the
// default when demo session restarts (i.e. user-selected locale is only allowed
// to be used for a single session).
void RestoreDefaultLocaleForNextSession() {
  auto* user = user_manager::UserManager::Get()->GetActiveUser();
  // Tests may not have an active user.
  if (!user)
    return;
  if (!user->is_profile_created()) {
    user->AddProfileCreatedObserver(
        base::BindOnce(&RestoreDefaultLocaleForNextSession));
    return;
  }
  Profile* profile = ProfileManager::GetActiveUserProfile();
  DCHECK(profile);
  const std::string current_locale =
      profile->GetPrefs()->GetString(language::prefs::kApplicationLocale);
  if (current_locale.empty()) {
    LOG(WARNING) << "Current locale read from kApplicationLocale is empty!";
    return;
  }
  const std::string default_locale =
      g_browser_process->local_state()->GetString(
          prefs::kDemoModeDefaultLocale);
  if (default_locale.empty()) {
    // If the default locale is uninitialized, consider the current locale to be
    // the default. This is safe because users are not allowed to change the
    // locale prior to introduction of this code.
    g_browser_process->local_state()->SetString(prefs::kDemoModeDefaultLocale,
                                                current_locale);
    return;
  }
  if (current_locale != default_locale) {
    // If the user has changed the locale, request to change it back (which will
    // take effect when the session restarts).
    profile->ChangeAppLocale(default_locale,
                             Profile::APP_LOCALE_CHANGED_VIA_DEMO_SESSION);
  }
}

// Returns the list of locales (and related info) supported by demo mode.
std::vector<LocaleInfo> GetSupportedLocales() {
  const base::flat_set<std::string> kSupportedLocales(
      {"da", "de", "en-GB", "en-US", "es", "fi", "fr", "fr-CA", "it", "ja",
       "nb", "nl", "sv"});

  const std::vector<std::string>& available_locales =
      l10n_util::GetUserFacingUILocaleList();
  const std::string current_locale_iso_code =
      ProfileManager::GetActiveUserProfile()->GetPrefs()->GetString(
          language::prefs::kApplicationLocale);
  std::vector<LocaleInfo> supported_locales;
  for (const std::string& locale : available_locales) {
    if (!kSupportedLocales.contains(locale))
      continue;
    LocaleInfo locale_info;
    locale_info.iso_code = locale;
    locale_info.display_name = l10n_util::GetDisplayNameForLocale(
        locale, current_locale_iso_code, true /* is_for_ui */);
    const std::u16string native_display_name =
        l10n_util::GetDisplayNameForLocale(locale, locale,
                                           true /* is_for_ui */);
    if (locale_info.display_name != native_display_name) {
      locale_info.display_name += u" - " + native_display_name;
    }
    supported_locales.push_back(std::move(locale_info));
  }
  return supported_locales;
}

void RecordDemoModeDimensions() {
  SYSLOG(INFO) << "Demo mode country: " << demo_mode::Country();
  SYSLOG(INFO) << "Demo mode retailer: " << demo_mode::RetailerName();
  SYSLOG(INFO) << "Demo mode store: " << demo_mode::StoreNumber();
}

}  // namespace

// static
constexpr char DemoSession::kSupportedCountries[][3];

constexpr char DemoSession::kCountryNotSelectedId[];

// static
std::string DemoSession::DemoConfigToString(
    DemoSession::DemoModeConfig config) {
  switch (config) {
    case DemoSession::DemoModeConfig::kNone:
      return "none";
    case DemoSession::DemoModeConfig::kOnline:
      return "online";
    case DemoSession::DemoModeConfig::kOfflineDeprecated:
      return "offlineDeprecated";
  }
  NOTREACHED() << "Unknown demo mode configuration";
  return std::string();
}

// static
bool DemoSession::IsDeviceInDemoMode() {
  if (!InstallAttributes::IsInitialized()) {
    return false;
  }
  bool is_demo_device_mode = InstallAttributes::Get()->GetMode() ==
                             policy::DeviceMode::DEVICE_MODE_DEMO;
  bool is_demo_device_domain =
      InstallAttributes::Get()->GetDomain() == policy::kDemoModeDomain;

  // We check device mode and domain to allow for dev/test
  // setup that is done by manual enrollment into demo domain. Device mode is
  // not set to DeviceMode::DEVICE_MODE_DEMO then.
  return is_demo_device_mode || is_demo_device_domain;
}

// static
DemoSession::DemoModeConfig DemoSession::GetDemoConfig() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (g_force_demo_config.has_value())
    return *g_force_demo_config;

  const PrefService* prefs = g_browser_process->local_state();

  // The testing browser process might not have local state.
  if (!prefs)
    return DemoModeConfig::kNone;

  // Demo mode config preference is set at the end of the demo setup after
  // device is enrolled.
  auto demo_config = DemoModeConfig::kNone;
  int demo_config_pref = prefs->GetInteger(prefs::kDemoModeConfig);
  if (demo_config_pref >= static_cast<int>(DemoModeConfig::kNone) &&
      demo_config_pref <= static_cast<int>(DemoModeConfig::kLast)) {
    demo_config = static_cast<DemoModeConfig>(demo_config_pref);
  }

  bool is_demo_mode = IsDeviceInDemoMode();
  if (is_demo_mode && demo_config == DemoModeConfig::kNone) {
    LOG(WARNING) << "Device mode is demo, but no demo mode config set";
  } else if (!is_demo_mode && demo_config != DemoModeConfig::kNone) {
    LOG(WARNING) << "Device mode is not demo, but demo mode config is set";
  }

  return is_demo_mode ? demo_config : DemoModeConfig::kNone;
}

// static
void DemoSession::SetDemoConfigForTesting(DemoModeConfig demo_config) {
  g_force_demo_config = demo_config;
}

// static
void DemoSession::ResetDemoConfigForTesting() {
  g_force_demo_config = absl::nullopt;
}

// static
DemoSession* DemoSession::StartIfInDemoMode() {
  if (!IsDeviceInDemoMode())
    return nullptr;

  if (g_demo_session && g_demo_session->started())
    return g_demo_session;

  if (!g_demo_session)
    g_demo_session = new DemoSession();

  g_demo_session->started_ = true;
  return g_demo_session;
}

// static
void DemoSession::ShutDownIfInitialized() {
  if (!g_demo_session)
    return;

  DemoSession* demo_session = g_demo_session;
  g_demo_session = nullptr;
  delete demo_session;
}

// static
DemoSession* DemoSession::Get() {
  return g_demo_session;
}

// static
std::string DemoSession::GetScreensaverAppId() {
  return GetSwitchOrDefault(switches::kDemoModeScreensaverApp,
                            extension_misc::kScreensaverAppId);
}

// static
bool DemoSession::ShouldShowExtensionInAppLauncher(const std::string& app_id) {
  if (!IsDeviceInDemoMode())
    return true;
  return app_id != GetScreensaverAppId() &&
         app_id != extensions::kWebStoreAppId;
}

// Static function to default region from VPD.
static std::string GetDefaultRegion() {
  const absl::optional<base::StringPiece> region_code =
      system::StatisticsProvider::GetInstance()->GetMachineStatistic(
          system::kRegionKey);
  if (region_code) {
    std::string region_code_upper_case =
        base::ToUpperASCII(region_code.value());
    std::string region_upper_case =
        region_code_upper_case.substr(0, region_code_upper_case.find("."));
    return region_upper_case.length() == 2 ? region_upper_case : "";
  }

  return "";
}

// static
bool DemoSession::ShouldShowWebApp(const std::string& app_id) {
  if (IsDeviceInDemoMode() &&
      content::GetNetworkConnectionTracker()->IsOffline()) {
    GURL app_id_as_url(app_id);
    return !app_id_as_url.SchemeIsHTTPOrHTTPS();
  }

  return true;
}

// static
base::Value::List DemoSession::GetCountryList() {
  base::Value::List country_list;
  std::string region(GetDefaultRegion());
  bool country_selected = false;

  for (CountryCodeAndFullNamePair pair :
       GetSortedCountryCodeAndNamePairList()) {
    std::string country = pair.country_id;
    base::Value::Dict dict;
    dict.Set("value", country);
    dict.Set("title", pair.country_name);
    if (country == region) {
      dict.Set("selected", true);
      g_browser_process->local_state()->SetString(prefs::kDemoModeCountry,
                                                  country);
      country_selected = true;
    } else {
      dict.Set("selected", false);
    }
    country_list.Append(std::move(dict));
  }

  if (!country_selected) {
    base::Value::Dict countryNotSelectedDict;
    countryNotSelectedDict.Set("value", DemoSession::kCountryNotSelectedId);
    countryNotSelectedDict.Set(
        "title",
        l10n_util::GetStringUTF16(
            IDS_OOBE_DEMO_SETUP_PREFERENCES_SCREEN_COUNTRY_NOT_SELECTED_TITLE));
    countryNotSelectedDict.Set("selected", true);
    country_list.Append(std::move(countryNotSelectedDict));
  }
  return country_list;
}

// static
void DemoSession::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kDemoModeDefaultLocale, std::string());
  registry->RegisterStringPref(prefs::kDemoModeCountry, kSupportedCountries[0]);
  registry->RegisterStringPref(prefs::kDemoModeRetailerId, std::string());
  registry->RegisterStringPref(prefs::kDemoModeStoreId, std::string());
  registry->RegisterStringPref(prefs::kDemoModeAppVersion, std::string());
  registry->RegisterStringPref(prefs::kDemoModeResourcesVersion, std::string());
}

void DemoSession::EnsureResourcesLoaded(base::OnceClosure load_callback) {
  if (!components_)
    components_ = std::make_unique<DemoComponents>(GetDemoConfig());
  components_->LoadResourcesComponent(std::move(load_callback));
}

// static
void DemoSession::RecordAppLaunchSourceIfInDemoMode(AppLaunchSource source) {
  if (IsDeviceInDemoMode())
    UMA_HISTOGRAM_ENUMERATION("DemoMode.AppLaunchSource", source);
}

bool DemoSession::ShouldShowAndroidOrChromeAppInShelf(
    const std::string& app_id_or_package) {
  if (!g_demo_session || !g_demo_session->started())
    return true;

  // TODO(michaelpg): Update shelf when network status changes.
  // TODO(michaelpg): Also check for captive portal.
  if (!content::GetNetworkConnectionTracker()->IsOffline())
    return true;

  // Ignore for specified chrome/android apps.
  return !base::Contains(ignore_pin_policy_offline_apps_, app_id_or_package);
}

void DemoSession::SetExtensionsExternalLoader(
    scoped_refptr<DemoExtensionsExternalLoader> extensions_external_loader) {
  extensions_external_loader_ = extensions_external_loader;
}

void DemoSession::OverrideIgnorePinPolicyAppsForTesting(
    std::vector<std::string> apps) {
  ignore_pin_policy_offline_apps_ = std::move(apps);
}

void DemoSession::SetTimerForTesting(
    std::unique_ptr<base::OneShotTimer> timer) {
  remove_splash_screen_fallback_timer_ = std::move(timer);
}

base::OneShotTimer* DemoSession::GetTimerForTesting() {
  return remove_splash_screen_fallback_timer_.get();
}

void DemoSession::ActiveUserChanged(user_manager::User* active_user) {
  const base::RepeatingClosure hide_web_store_icon = base::BindRepeating([]() {
    ProfileManager::GetActiveUserProfile()->GetPrefs()->SetBoolean(
        policy::policy_prefs::kHideWebStoreIcon, true);
  });
  active_user->AddProfileCreatedObserver(hide_web_store_icon);
}

DemoSession::DemoSession()
    : ignore_pin_policy_offline_apps_(GetIgnorePinPolicyApps()),
      remove_splash_screen_fallback_timer_(
          std::make_unique<base::OneShotTimer>()) {
  // SessionManager may be unset in unit tests.
  if (session_manager::SessionManager::Get()) {
    session_manager_observation_.Observe(
        session_manager::SessionManager::Get());
    OnSessionStateChanged();
  }
  ChromeUserManager::Get()->AddSessionStateObserver(this);
}

DemoSession::~DemoSession() {
  ChromeUserManager::Get()->RemoveSessionStateObserver(this);
}

std::vector<CountryCodeAndFullNamePair>
DemoSession::GetSortedCountryCodeAndNamePairList() {
  const std::string current_locale = g_browser_process->GetApplicationLocale();
  std::vector<CountryCodeAndFullNamePair> result;
  for (const std::string country : kSupportedCountries) {
    result.push_back({country, l10n_util::GetDisplayNameForCountry(
                                   country, current_locale)});
  }
  UErrorCode error_code = U_ZERO_ERROR;
  std::unique_ptr<icu::Collator> collator(
      icu::Collator::createInstance(error_code));
  DCHECK(U_SUCCESS(error_code));

  std::sort(result.begin(), result.end(),
            [&collator](CountryCodeAndFullNamePair pair1,
                        CountryCodeAndFullNamePair pair2) {
              return base::i18n::CompareString16WithCollator(
                         *collator, pair1.country_name, pair2.country_name) < 0;
            });
  return result;
}

void DemoSession::InstallDemoResources() {
  DCHECK(components_->resources_component_loaded());

  Profile* const profile = ProfileManager::GetActiveUserProfile();
  DCHECK(profile);
  const base::FilePath downloads =
      file_manager::util::GetDownloadsFolderForProfile(profile);
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&InstallDemoMedia, components_->resources_component_path(),
                     downloads));
}

void DemoSession::SetKeyboardBrightnessToOneHundredPercentFromCurrentLevel(
    absl::optional<double> keyboard_brightness_percentage) {
  // Map of current keyboard brightness percentage to times needed to call
  // IncreaseKeyboardBrightness to reach max brightness level.
  const base::flat_map<int, int> kTimesToIncreaseKeyboardBrightnessToMax = {
      {0, 5}, {10, 4}, {20, 3}, {40, 2}, {60, 1}, {100, 0}};

  if (keyboard_brightness_percentage.has_value()) {
    const auto timesToIncreaseKeyboardBrightness =
        kTimesToIncreaseKeyboardBrightnessToMax.find(
            keyboard_brightness_percentage.value());

    if (kTimesToIncreaseKeyboardBrightnessToMax.end() !=
        timesToIncreaseKeyboardBrightness) {
      for (int i = 0; i < timesToIncreaseKeyboardBrightness->second; i++) {
        chromeos::PowerManagerClient::Get()->IncreaseKeyboardBrightness();
      }
    }
  }
}

void DemoSession::RegisterDemoModeAAExperiment() {
  if (demo_mode::Country() == std::string("US")) {
    // The hashing salt for the AA experiment.
    std::string demo_mode_aa_experiment_hashing_salt = "fae448044d545f9c";

    std::vector<std::string> best_buy_retailer_names = {"bby", "bestbuy",
                                                        "bbt"};
    std::vector<std::string>::iterator it;

    it = std::find(best_buy_retailer_names.begin(),
                   best_buy_retailer_names.end(), demo_mode::RetailerName());
    if (it != best_buy_retailer_names.end()) {
      std::string store_number_and_hash_salt =
          demo_mode::StoreNumber() + demo_mode_aa_experiment_hashing_salt;
      std::string md5_store_number =
          base::MD5String(store_number_and_hash_salt);

      char& last_char = md5_store_number.back();
      int md5_last_char_int =
          (last_char >= 'a') ? (last_char - 'a' + 10) : (last_char - '0');

      ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
          "DemoModeAAExperimentBasedOnStoreId",
          md5_last_char_int % 2 ? "Experiment" : "Control",
          variations::SyntheticTrialAnnotationMode::kCurrentLog);
    }
  }
}

void DemoSession::OnSessionStateChanged() {
  TRACE_EVENT0("login", "DemoSession::OnSessionStateChanged");
  switch (session_manager::SessionManager::Get()->session_state()) {
    case session_manager::SessionState::LOGIN_PRIMARY:
      EnsureResourcesLoaded(
          base::BindOnce(&DemoSession::ConfigureAndStartSplashScreen,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    case session_manager::SessionState::ACTIVE:
      if (ShouldRemoveSplashScreen()) {
        RemoveSplashScreen();
      }

      // SystemTrayClientImpl may not exist in unit tests.
      if (SystemTrayClientImpl::Get()) {
        const std::string current_locale_iso_code =
            ProfileManager::GetActiveUserProfile()->GetPrefs()->GetString(
                language::prefs::kApplicationLocale);
        SystemTrayClientImpl::Get()->SetLocaleList(GetSupportedLocales(),
                                                   current_locale_iso_code);
        SYSLOG(INFO) << "Demo mode session current locale: "
                     << current_locale_iso_code;
      }

      RestoreDefaultLocaleForNextSession();

      if (chromeos::PowerManagerClient::Get()) {
        chromeos::PowerManagerClient::Get()->GetKeyboardBrightnessPercent(
            base::BindOnce(
                &DemoSession::
                    SetKeyboardBrightnessToOneHundredPercentFromCurrentLevel,
                weak_ptr_factory_.GetWeakPtr()));
      }

      // Download/update the Demo app component during session startup
      if (!components_) {
        components_ = std::make_unique<DemoComponents>(GetDemoConfig());
      }
      components_->LoadAppComponent(
          base::BindOnce(&DemoSession::OnDemoAppComponentLoaded,
                         weak_ptr_factory_.GetWeakPtr()));

      EnsureResourcesLoaded(base::BindOnce(&DemoSession::InstallDemoResources,
                                           weak_ptr_factory_.GetWeakPtr()));

      // Register the device with in the A/A experiment
      RegisterDemoModeAAExperiment();

      // Create the window closer.
      // TODO(b/302583338) Remove this when the issue with GMSCore gets fixed.
      if (ash::features::IsDemoModeGMSCoreWindowCloserEnabled()) {
        window_closer_ = std::make_unique<DemoModeWindowCloser>();
      }
      break;
    default:
      break;
  }

  RecordDemoModeDimensions();
}

base::FilePath DemoSession::GetDemoAppComponentPath() {
  DCHECK(components_);
  DCHECK(!components_->default_app_component_path().empty());
  return base::FilePath(
      GetSwitchOrDefault(switches::kDemoModeSwaContentDirectory,
                         components_->default_app_component_path().value()));
}

void LaunchDemoSystemWebApp() {
  // SystemWebAppManager won't run this callback if the profile is destroyed,
  // so we don't need to worry about there being no active user profile
  Profile* profile = ProfileManager::GetActiveUserProfile();
  LaunchSystemWebAppAsync(profile, SystemWebAppType::DEMO_MODE);
}

void DemoSession::OnDemoAppComponentLoaded() {
  const auto& app_component_version = components_->app_component_version();
  SYSLOG(INFO) << "Demo mode app component version: "
               << (app_component_version.has_value()
                       ? app_component_version.value().GetString()
                       : "");
  auto error = components_->app_component_error().value_or(
      component_updater::CrOSComponentManager::Error::NOT_FOUND);

  if (error != component_updater::CrOSComponentManager::Error::NONE) {
    LOG(WARNING) << "Error loading demo mode app component: "
                 << static_cast<int>(error);
    return;
  }
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (auto* swa_manager = SystemWebAppManager::Get(profile)) {
    swa_manager->on_apps_synchronized().Post(
        FROM_HERE, base::BindOnce(&LaunchDemoSystemWebApp));
  }
}

base::FilePath GetSplashScreenImagePath(base::FilePath localized_image_path,
                                        base::FilePath fallback_path) {
  return base::PathExists(localized_image_path) ? localized_image_path
                                                : fallback_path;
}

void DemoSession::ShowSplashScreen(base::FilePath image_path) {
  ash::WallpaperController::Get()->ShowOverrideWallpaper(
      image_path, /*always_on_top=*/true);
  remove_splash_screen_fallback_timer_->Start(
      FROM_HERE, kRemoveSplashScreenTimeout,
      base::BindOnce(&DemoSession::RemoveSplashScreen,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DemoSession::ConfigureAndStartSplashScreen() {
  const std::string current_locale = g_browser_process->GetApplicationLocale();
  base::FilePath localized_image_path = components_->resources_component_path()
                                            .Append(kSplashScreensPath)
                                            .Append(current_locale + ".jpg");
  base::FilePath fallback_path = components_->resources_component_path()
                                     .Append(kSplashScreensPath)
                                     .Append("en-US.jpg");
  const auto& version = components_->resources_component_version();
  SYSLOG(INFO) << "Demo mode resources version: "
               << (version.has_value() ? version.value().GetString() : "");

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&GetSplashScreenImagePath, localized_image_path,
                     fallback_path),
      base::BindOnce(&DemoSession::ShowSplashScreen,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DemoSession::RemoveSplashScreen() {
  if (splash_screen_removed_)
    return;
  ash::WallpaperController::Get()->RemoveOverrideWallpaper();
  remove_splash_screen_fallback_timer_.reset();
  app_window_registry_observations_.RemoveAllObservations();
  splash_screen_removed_ = true;
}

bool DemoSession::ShouldRemoveSplashScreen() {
  // TODO(crbug.com/934979): Launch screensaver after active session starts, so
  // that there's no need to check session state here.
  return session_manager::SessionManager::Get()->session_state() ==
             session_manager::SessionState::ACTIVE &&
         screensaver_activated_;
}

// TODO(b/250637035): Either delete this code or fix it to work with the Demo
// Mode SWA
void DemoSession::OnAppWindowActivated(extensions::AppWindow* app_window) {
  if (app_window->extension_id() != GetScreensaverAppId())
    return;
  screensaver_activated_ = true;
  if (ShouldRemoveSplashScreen())
    RemoveSplashScreen();
}

}  // namespace ash
