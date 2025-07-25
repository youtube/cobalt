// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/background_tracing_utils.h"

#include <memory>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/tracing/common/tracing_switches.h"
#include "content/public/browser/background_tracing_manager.h"
#include "content/public/browser/browser_thread.h"

namespace tracing {

BASE_FEATURE(kFieldTracing, "FieldTracing", base::FEATURE_DISABLED_BY_DEFAULT);

namespace {

const base::FeatureParam<std::string> kFieldTracingConfig{&kFieldTracing,
                                                          "config", ""};

bool BlockingWriteTraceToFile(const base::FilePath& output_file,
                              std::string file_contents) {
  if (base::WriteFile(output_file, file_contents)) {
    LOG(ERROR) << "Background trace written to "
               << output_file.LossyDisplayName();
    return true;
  }
  LOG(ERROR) << "Failed to write background trace to "
             << output_file.LossyDisplayName();
  return false;
}

void WriteTraceToFile(
    const base::FilePath& output_file,
    std::string file_contents,
    content::BackgroundTracingManager::FinishedProcessingCallback
        done_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&BlockingWriteTraceToFile, output_file,
                     std::move(file_contents)),
      std::move(done_callback));
}

std::unique_ptr<content::BackgroundTracingConfig>
GetBackgroundTracingConfigFromFile(const base::FilePath& config_file) {
  std::string config_text;
  if (!base::ReadFileToString(config_file, &config_text) ||
      config_text.empty()) {
    LOG(ERROR) << "Failed to read background tracing config file "
               << config_file.value();
    return nullptr;
  }

  auto value_with_error = base::JSONReader::ReadAndReturnValueWithError(
      config_text, base::JSON_ALLOW_TRAILING_COMMAS);
  if (!value_with_error.has_value()) {
    LOG(ERROR) << "Background tracing has incorrect config: "
               << value_with_error.error().message;
    return nullptr;
  }

  if (!value_with_error->is_dict()) {
    LOG(ERROR) << "Background tracing config is not a dict";
    return nullptr;
  }

  auto config = content::BackgroundTracingConfig::FromDict(
      std::move(*value_with_error).TakeDict());

  if (!config) {
    LOG(ERROR) << "Background tracing config dict has invalid contents";
    return nullptr;
  }

  return config;
}

}  // namespace

void RecordDisallowedMetric(TracingFinalizationDisallowedReason reason) {
  UMA_HISTOGRAM_ENUMERATION("Tracing.Background.FinalizationDisallowedReason",
                            reason);
}

bool SetupBackgroundTracingFromJsonConfigFile(
    const base::FilePath& config_file) {
  std::unique_ptr<content::BackgroundTracingConfig> config =
      GetBackgroundTracingConfigFromFile(config_file);
  if (!config) {
    return false;
  }

  // NO_DATA_FILTERING is set because the trace is saved to a local output file
  // instead of being uploaded to a metrics server, so there are no PII
  // concerns.
  return content::BackgroundTracingManager::GetInstance().SetActiveScenario(
      std::move(config), content::BackgroundTracingManager::NO_DATA_FILTERING);
}

bool SetupBackgroundTracingFromProtoConfigFile(
    const base::FilePath& config_file) {
  perfetto::protos::gen::ChromeFieldTracingConfig config;

  std::string config_text;
  if (!base::ReadFileToString(config_file, &config_text) ||
      config_text.empty() || !config.ParseFromString(config_text)) {
    LOG(ERROR) << "Failed to read field tracing config file "
               << config_file.value() << "."
               << "Make sure to provide a serialized proto, or use "
               << "--enable-legacy-background-tracing to provide a "
               << "JSON config.";
    return false;
  }

  // NO_DATA_FILTERING is set because the trace is saved to a local output file
  // instead of being uploaded to a metrics server, so there are no PII
  // concerns.
  return content::BackgroundTracingManager::GetInstance().InitializeScenarios(
      std::move(config), content::BackgroundTracingManager::NO_DATA_FILTERING);
}

bool SetupBackgroundTracingFromCommandLine() {
  auto* command_line = base::CommandLine::ForCurrentProcess();

  if (tracing::HasBackgroundTracingOutputFile() &&
      !tracing::SetBackgroundTracingOutputFile()) {
    return false;
  }

  switch (GetBackgroundTracingSetupMode()) {
    case BackgroundTracingSetupMode::kDisabledInvalidCommandLine:
      return false;
    case BackgroundTracingSetupMode::kFromJsonConfigFile:
      return SetupBackgroundTracingFromJsonConfigFile(
          command_line->GetSwitchValuePath(
              switches::kEnableLegacyBackgroundTracing));
    case BackgroundTracingSetupMode::kFromProtoConfigFile:
      return SetupBackgroundTracingFromProtoConfigFile(
          command_line->GetSwitchValuePath(switches::kEnableBackgroundTracing));
    case BackgroundTracingSetupMode::kFromFieldTrial:
      return false;
  }
}

bool HasBackgroundTracingOutputFile() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kBackgroundTracingOutputFile);
}

bool SetBackgroundTracingOutputFile() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->GetSwitchValuePath(switches::kBackgroundTracingOutputFile)
          .empty()) {
    LOG(ERROR) << "--background-tracing-output-file needs an output file path";
    return false;
  }
  auto output_file =
      command_line->GetSwitchValuePath(switches::kBackgroundTracingOutputFile);

  auto receive_callback = base::BindRepeating(&WriteTraceToFile, output_file);
  content::BackgroundTracingManager::GetInstance().SetReceiveCallback(
      std::move(receive_callback));
  return true;
}

BackgroundTracingSetupMode GetBackgroundTracingSetupMode() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kEnableBackgroundTracing) &&
      !command_line->HasSwitch(switches::kEnableLegacyBackgroundTracing)) {
    return BackgroundTracingSetupMode::kFromFieldTrial;
  }

  if (command_line->HasSwitch(switches::kEnableBackgroundTracing) &&
      command_line->HasSwitch(switches::kEnableLegacyBackgroundTracing)) {
    LOG(ERROR) << "Can't specify both --enable-background-tracing and "
                  "--enable-legacy-background-tracing";
    return BackgroundTracingSetupMode::kDisabledInvalidCommandLine;
  }

  if (command_line->HasSwitch(switches::kEnableBackgroundTracing) &&
      command_line->GetSwitchValueNative(switches::kEnableBackgroundTracing)
          .empty()) {
    LOG(ERROR) << "--enable-background-tracing needs a config file path";
    return BackgroundTracingSetupMode::kDisabledInvalidCommandLine;
  }

  if (command_line->HasSwitch(switches::kEnableLegacyBackgroundTracing) &&
      command_line
          ->GetSwitchValueNative(switches::kEnableLegacyBackgroundTracing)
          .empty()) {
    LOG(ERROR) << "--enable-legacy-background-tracing needs a config file path";
    return BackgroundTracingSetupMode::kDisabledInvalidCommandLine;
  }

  if (command_line->HasSwitch(switches::kEnableBackgroundTracing)) {
    return BackgroundTracingSetupMode::kFromProtoConfigFile;
  }
  return BackgroundTracingSetupMode::kFromJsonConfigFile;
}

absl::optional<perfetto::protos::gen::ChromeFieldTracingConfig>
GetFieldTracingConfig() {
  if (!base::FeatureList::IsEnabled(kFieldTracing)) {
    return absl::nullopt;
  }
  std::string serialized_config;
  if (!base::Base64Decode(kFieldTracingConfig.Get(), &serialized_config)) {
    return absl::nullopt;
  }
  perfetto::protos::gen::ChromeFieldTracingConfig config;
  if (config.ParseFromString(serialized_config)) {
    return config;
  }
  return absl::nullopt;
}

}  // namespace tracing
