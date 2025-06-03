// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/support_tool/support_tool_util.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/i18n/time_formatting.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/feedback/system_logs/log_sources/chrome_internal_log_source.h"
#include "chrome/browser/feedback/system_logs/log_sources/crash_ids_source.h"
#include "chrome/browser/feedback/system_logs/log_sources/device_event_log_source.h"
#include "chrome/browser/feedback/system_logs/log_sources/memory_details_log_source.h"
#include "chrome/browser/feedback/system_logs/log_sources/performance_log_source.h"
#include "chrome/browser/support_tool/data_collection_module.pb.h"
#include "chrome/browser/support_tool/policy_data_collector.h"
#include "chrome/browser/support_tool/signin_data_collector.h"
#include "chrome/browser/support_tool/support_tool_handler.h"
#include "chrome/browser/support_tool/system_log_source_data_collector_adaptor.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/system_logs/app_service_log_source.h"
#include "chrome/browser/ash/system_logs/bluetooth_log_source.h"
#include "chrome/browser/ash/system_logs/command_line_log_source.h"
#include "chrome/browser/ash/system_logs/connected_input_devices_log_source.h"
#include "chrome/browser/ash/system_logs/crosapi_system_log_source.h"
#include "chrome/browser/ash/system_logs/dbus_log_source.h"
#include "chrome/browser/ash/system_logs/iwlwifi_dump_log_source.h"
#include "chrome/browser/ash/system_logs/touch_log_source.h"
#include "chrome/browser/ash/system_logs/traffic_counters_log_source.h"
#include "chrome/browser/ash/system_logs/virtual_keyboard_log_source.h"
#include "chrome/browser/feedback/system_logs/log_sources/lacros_log_files_log_source.h"
#include "chrome/browser/support_tool/ash/chrome_user_logs_data_collector.h"
#include "chrome/browser/support_tool/ash/network_health_data_collector.h"
#include "chrome/browser/support_tool/ash/network_routes_data_collector.h"
#include "chrome/browser/support_tool/ash/shill_data_collector.h"
#include "chrome/browser/support_tool/ash/system_logs_data_collector.h"
#include "chrome/browser/support_tool/ash/system_state_data_collector.h"
#include "chrome/browser/support_tool/ash/ui_hierarchy_data_collector.h"

#if BUILDFLAG(IS_CHROMEOS_WITH_HW_DETAILS)
#include "chrome/browser/ash/system_logs/reven_log_source.h"
#endif  // BUILDFLAG(IS_CHROMEOS_WITH_HW_DETAILS)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

// Data collector types that can work on every platform.
constexpr support_tool::DataCollectorType kDataCollectors[] = {
    support_tool::CHROME_INTERNAL,       support_tool::CRASH_IDS,
    support_tool::MEMORY_DETAILS,        support_tool::POLICIES,
    support_tool::CHROMEOS_DEVICE_EVENT, support_tool::PERFORMANCE,
    support_tool::SIGN_IN_STATE};

// Data collector types can only work on Chrome OS Ash.
constexpr support_tool::DataCollectorType kDataCollectorsChromeosAsh[] = {
    support_tool::CHROMEOS_UI_HIERARCHY,
    support_tool::CHROMEOS_COMMAND_LINE,
    support_tool::CHROMEOS_IWL_WIFI_DUMP,
    support_tool::CHROMEOS_TOUCH_EVENTS,
    support_tool::CHROMEOS_DBUS,
    support_tool::CHROMEOS_NETWORK_ROUTES,
    support_tool::CHROMEOS_SHILL,
    support_tool::CHROMEOS_SYSTEM_STATE,
    support_tool::CHROMEOS_SYSTEM_LOGS,
    support_tool::CHROMEOS_CHROME_USER_LOGS,
    support_tool::CHROMEOS_BLUETOOTH_FLOSS,
    support_tool::CHROMEOS_CONNECTED_INPUT_DEVICES,
    support_tool::CHROMEOS_TRAFFIC_COUNTERS,
    support_tool::CHROMEOS_VIRTUAL_KEYBOARD,
    support_tool::CHROMEOS_NETWORK_HEALTH,
    support_tool::CHROMEOS_APP_SERVICE};

// Data collector types that can only work on if IS_CHROMEOS_WITH_HW_DETAILS
// flag is turned on. IS_CHROMEOS_WITH_HW_DETAILS flag will be turned on for
// Chrome OS Flex devices.
constexpr support_tool::DataCollectorType kDataCollectorsChromeosHwDetails[] = {
    support_tool::CHROMEOS_REVEN};

// Data collector types that may be available on the device depending on other
// components or flags. Currently consists of data collactors that collect
// logs for Lacros.
constexpr support_tool::DataCollectorType kOptionalDataCollectors[] = {
    support_tool::CHROMEOS_CROS_API, support_tool::CHROMEOS_LACROS};

}  // namespace

std::unique_ptr<SupportToolHandler> GetSupportToolHandler(
    std::string case_id,
    std::string email_address,
    std::string issue_description,
    Profile* profile,
    std::set<support_tool::DataCollectorType> included_data_collectors) {
  std::unique_ptr<SupportToolHandler> handler =
      std::make_unique<SupportToolHandler>(case_id, email_address,
                                           issue_description);
  for (const auto& data_collector_type : included_data_collectors) {
    switch (data_collector_type) {
      case support_tool::CHROME_INTERNAL:
        handler->AddDataCollector(
            std::make_unique<SystemLogSourceDataCollectorAdaptor>(
                "Fetches internal Chrome logs.",
                std::make_unique<system_logs::ChromeInternalLogSource>()));
        break;
      case support_tool::CRASH_IDS:
        handler->AddDataCollector(std::make_unique<
                                  SystemLogSourceDataCollectorAdaptor>(
            "Extracts the most recent crash IDs (if any) and exports them into "
            "crash_report_ids and all_crash_report_ids files.",
            std::make_unique<system_logs::CrashIdsSource>()));
        break;
      case support_tool::MEMORY_DETAILS:
        handler->AddDataCollector(std::make_unique<
                                  SystemLogSourceDataCollectorAdaptor>(
            "Fetches memory usage details and exports them into mem_usage and "
            "mem_usage_with_title files.",
            std::make_unique<system_logs::MemoryDetailsLogSource>()));
        break;
      case support_tool::POLICIES:
        handler->AddDataCollector(
            std::make_unique<PolicyDataCollector>(profile));
        break;
      case support_tool::CHROMEOS_DEVICE_EVENT:
        handler->AddDataCollector(std::make_unique<
                                  SystemLogSourceDataCollectorAdaptor>(
            "Fetches entries for 'network_event_log' and 'device_event_log'.",
            std::make_unique<system_logs::DeviceEventLogSource>()));
        break;
      case support_tool::PERFORMANCE:
        handler->AddDataCollector(
            std::make_unique<SystemLogSourceDataCollectorAdaptor>(
                "Gathers performance relevant data such as if high efficiency "
                "or battery saver mode is active and details about current "
                "battery state.",
                std::make_unique<system_logs::PerformanceLogSource>()));
        break;
      case support_tool::SIGN_IN_STATE:
        handler->AddDataCollector(
            std::make_unique<SigninDataCollector>(profile));
        break;
#if BUILDFLAG(IS_CHROMEOS_ASH)
      case support_tool::CHROMEOS_UI_HIERARCHY:
        handler->AddDataCollector(std::make_unique<UiHierarchyDataCollector>());
        break;
      case support_tool::CHROMEOS_NETWORK_ROUTES:
        handler->AddDataCollector(
            std::make_unique<NetworkRoutesDataCollector>());
        break;
      case support_tool::CHROMEOS_COMMAND_LINE:
        handler->AddDataCollector(
            std::make_unique<SystemLogSourceDataCollectorAdaptor>(
                "Gathers log data from various scripts/programs. Creates and "
                "exports data into these files: alsa controls, cras, "
                "audio_diagnostics, env, disk_usage.",
                std::make_unique<system_logs::CommandLineLogSource>()));
        break;
      case support_tool::CHROMEOS_IWL_WIFI_DUMP:
        handler->AddDataCollector(std::make_unique<
                                  SystemLogSourceDataCollectorAdaptor>(
            "Fetches debug dump information from Intel Wi-Fi NICs that will be "
            "produced when those NICs have issues such as firmware crashes. "
            "Exports the logs into a file named iwlwifi_dump.",
            std::make_unique<system_logs::IwlwifiDumpLogSource>()));
        break;
      case support_tool::CHROMEOS_TOUCH_EVENTS:
        handler->AddDataCollector(
            std::make_unique<SystemLogSourceDataCollectorAdaptor>(
                "Fetches touch events, touchscreen and touchpad logs.",
                std::make_unique<system_logs::TouchLogSource>()));
        break;
      case support_tool::CHROMEOS_DBUS:
        handler->AddDataCollector(
            std::make_unique<SystemLogSourceDataCollectorAdaptor>(
                "Fetches DBus usage statistics. Creates and exports data into "
                "these files: dbus_details, dbus_summary.",
                std::make_unique<system_logs::DBusLogSource>()));
        break;
      case support_tool::CHROMEOS_CROS_API:
        if (crosapi::BrowserManager::Get()->IsRunning() &&
            crosapi::BrowserManager::Get()->GetFeedbackDataSupported()) {
          handler->AddDataCollector(std::make_unique<
                                    SystemLogSourceDataCollectorAdaptor>(
              "Gets Lacros system information log data if Lacros is running "
              "and the crosapi version supports the Lacros remote data source.",
              std::make_unique<system_logs::CrosapiSystemLogSource>()));
        }
        break;
      case support_tool::CHROMEOS_LACROS:
        if (crosapi::browser_util::IsLacrosEnabled()) {
          // Lacros logs are saved in the user data directory, so we provide
          // that path to the LacrosLogFilesLogSource.
          base::FilePath log_base_path =
              crosapi::browser_util::GetUserDataDir();
          std::string lacrosUserLogKey = "lacros_user_log";
          handler->AddDataCollector(std::make_unique<
                                    SystemLogSourceDataCollectorAdaptor>(
              "Gets Lacros system information log data if Lacros is running "
              "and the crosapi version supports the Lacros remote data source.",
              std::make_unique<system_logs::LacrosLogFilesLogSource>(
                  log_base_path, lacrosUserLogKey)));
        }
        break;
      case support_tool::CHROMEOS_SHILL:
        handler->AddDataCollector(std::make_unique<ShillDataCollector>());
        break;
      case support_tool::CHROMEOS_SYSTEM_STATE:
        handler->AddDataCollector(std::make_unique<SystemStateDataCollector>());
        break;
      case support_tool::CHROMEOS_SYSTEM_LOGS:
        // Set `requested_logs` as empty to request all log for now.
        handler->AddDataCollector(std::make_unique<SystemLogsDataCollector>(
            /*requested_logs=*/std::set<base::FilePath>()));
        break;
      case support_tool::CHROMEOS_CHROME_USER_LOGS:
        // TODO(b:294844737): Add testing for this check.
        // User session must be active to read user data from Cryptohome.
        if (ash::IsUserBrowserContext(profile)) {
          handler->AddDataCollector(
              std::make_unique<ChromeUserLogsDataCollector>());
        }
        break;
      case support_tool::CHROMEOS_BLUETOOTH_FLOSS:
        handler->AddDataCollector(
            std::make_unique<SystemLogSourceDataCollectorAdaptor>(
                "Fetches the Bluetooth Floss enabled or disabled information "
                "on the device.",
                std::make_unique<system_logs::BluetoothLogSource>()));
        break;
      case support_tool::CHROMEOS_CONNECTED_INPUT_DEVICES:
        handler->AddDataCollector(std::make_unique<
                                  SystemLogSourceDataCollectorAdaptor>(
            "Fetches the information about connected input devices.",
            std::make_unique<system_logs::ConnectedInputDevicesLogSource>()));
        break;
      case support_tool::CHROMEOS_TRAFFIC_COUNTERS:
        handler->AddDataCollector(
            std::make_unique<SystemLogSourceDataCollectorAdaptor>(
                "Fetches traffic counters information.",
                std::make_unique<system_logs::TrafficCountersLogSource>()));
        break;
      case support_tool::CHROMEOS_VIRTUAL_KEYBOARD:
        handler->AddDataCollector(
            std::make_unique<SystemLogSourceDataCollectorAdaptor>(
                "Fetches the virtual keyboard information.",
                std::make_unique<system_logs::VirtualKeyboardLogSource>()));
        break;
      case support_tool::CHROMEOS_NETWORK_HEALTH:
        handler->AddDataCollector(
            std::make_unique<NetworkHealthDataCollector>());
        break;
      case support_tool::CHROMEOS_APP_SERVICE:
        handler->AddDataCollector(
            std::make_unique<SystemLogSourceDataCollectorAdaptor>(
                "Gathers information from app service about installed and "
                "running apps.",
                std::make_unique<system_logs::AppServiceLogSource>()));
        break;
      case support_tool::CHROMEOS_REVEN:
#if BUILDFLAG(IS_CHROMEOS_WITH_HW_DETAILS)
        handler->AddDataCollector(
            std::make_unique<SystemLogSourceDataCollectorAdaptor>(
                "Collect Hardware data for ChromeOS Flex devices via "
                "cros_healthd calls.",
                std::make_unique<system_logs::RevenLogSource>()));
#endif  // BUILDFLAG(IS_CHROMEOS_WITH_HW_DETAILS)
        break;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
      default:
        break;
    }
  }
  return handler;
}

std::vector<support_tool::DataCollectorType> GetAllDataCollectors() {
  std::vector<support_tool::DataCollectorType> data_collectors;
  for (const auto& type : kDataCollectors) {
    data_collectors.push_back(type);
  }
  for (const auto& type : kDataCollectorsChromeosAsh) {
    data_collectors.push_back(type);
  }
  for (const auto& type : kDataCollectorsChromeosHwDetails) {
    data_collectors.push_back(type);
  }
  for (const auto& type : kOptionalDataCollectors) {
    data_collectors.push_back(type);
  }
  return data_collectors;
}

std::vector<support_tool::DataCollectorType>
GetAllAvailableDataCollectorsOnDevice() {
  std::vector<support_tool::DataCollectorType> data_collectors;
  for (const auto& type : kDataCollectors) {
    data_collectors.push_back(type);
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  for (const auto& type : kDataCollectorsChromeosAsh) {
    data_collectors.push_back(type);
  }
  if (crosapi::browser_util::IsLacrosEnabled()) {
    data_collectors.push_back(support_tool::CHROMEOS_LACROS);
  }
  if (crosapi::BrowserManager::Get()->IsRunning() &&
      crosapi::BrowserManager::Get()->GetFeedbackDataSupported()) {
    data_collectors.push_back(support_tool::CHROMEOS_CROS_API);
  }
#if BUILDFLAG(IS_CHROMEOS_WITH_HW_DETAILS)
  for (const auto& type : kDataCollectorsChromeosHwDetails) {
    data_collectors.push_back(type);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_WITH_HW_DETAILS)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  return data_collectors;
}

base::FilePath GetFilepathToExport(base::FilePath target_directory,
                                   const std::string& filename_prefix,
                                   const std::string& case_id,
                                   base::Time timestamp) {
  std::string filename = filename_prefix + "_";
  if (!case_id.empty()) {
    filename += case_id + "_";
  }
  return target_directory.AppendASCII(
      filename + base::UnlocalizedTimeFormatWithPattern(
                     timestamp, "'UTC'yyyyMMdd_HHmm", icu::TimeZone::getGMT()));
}
