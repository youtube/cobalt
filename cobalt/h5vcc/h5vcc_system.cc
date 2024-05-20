// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/h5vcc/h5vcc_system.h"

#include <utility>

#include "base/strings/stringprintf.h"
#include "cobalt/configuration/configuration.h"
#include "cobalt/version.h"
#include "cobalt/web/environment_settings_helper.h"
#include "cobalt_build_id.h"  // NOLINT(build/include_subdir)
#include "starboard/common/system_property.h"
#include "starboard/extension/ifa.h"
#include "starboard/system.h"

using starboard::kSystemPropertyMaxLength;

namespace cobalt {
namespace h5vcc {

H5vccSystem::H5vccSystem(
#if SB_IS(EVERGREEN)
    H5vccUpdater* updater)
    : updater_(updater),
#else
    )
    :
#endif
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      ifa_extension_(static_cast<const StarboardExtensionIfaApi*>(
          SbSystemGetExtension(kStarboardExtensionIfaName))) {
#if defined(COBALT_ENABLE_EXTENDED_IFA)
  if (ifa_extension_ && ifa_extension_->version >= 2) {
    ifa_extension_->RegisterTrackingAuthorizationCallback(
        this, [](void* context) {
          DCHECK(context) << "Callback called with NULL context";
          if (context) {
            static_cast<H5vccSystem*>(context)
                ->ReceiveTrackingAuthorizationComplete();
          }
        });
  }
#endif  // defined(COBALT_ENABLE_EXTENDED_IFA)
}

H5vccSystem::~H5vccSystem() {
#if defined(COBALT_ENABLE_EXTENDED_IFA)
  if (ifa_extension_ && ifa_extension_->version >= 2) {
    ifa_extension_->UnregisterTrackingAuthorizationCallback();
  }
#endif  // defined(COBALT_ENABLE_EXTENDED_IFA)
}

bool H5vccSystem::are_keys_reversed() const {
  return SbSystemHasCapability(kSbSystemCapabilityReversedEnterAndBack);
}

std::string H5vccSystem::build_id() const {
  return base::StringPrintf("Built at version #%s",
                            COBALT_BUILD_VERSION_NUMBER);
}

std::string H5vccSystem::platform() const {
  char property[512];

  std::string result;
  if (!SbSystemGetProperty(kSbSystemPropertyPlatformName, property,
                           SB_ARRAY_SIZE_INT(property))) {
    DLOG(FATAL) << "Failed to get kSbSystemPropertyPlatformName.";
  } else {
    result = property;
  }

  return result;
}

std::string H5vccSystem::advertising_id() const {
  std::string result;
  char property[kSystemPropertyMaxLength] = {0};
#if SB_API_VERSION >= 14
  if (!SbSystemGetProperty(kSbSystemPropertyAdvertisingId, property,
                           SB_ARRAY_SIZE_INT(property))) {
    DLOG(INFO) << "Failed to get kSbSystemPropertyAdvertisingId.";
  } else {
    result = property;
  }
#else
  if (ifa_extension_ && ifa_extension_->version >= 1) {
    if (!ifa_extension_->GetAdvertisingId(property,
                                          SB_ARRAY_SIZE_INT(property))) {
      DLOG(FATAL) << "Failed to get AdvertisingId from IFA extension.";
    } else {
      result = property;
    }
  }
#endif
  return result;
}
bool H5vccSystem::limit_ad_tracking() const {
  bool result = false;
  char property[kSystemPropertyMaxLength] = {0};
#if SB_API_VERSION >= 14
  if (!SbSystemGetProperty(kSbSystemPropertyLimitAdTracking, property,
                           SB_ARRAY_SIZE_INT(property))) {
    DLOG(INFO) << "Failed to get kSbSystemPropertyAdvertisingId.";
  } else {
    result = std::atoi(property);
  }
#else
  if (ifa_extension_ && ifa_extension_->version >= 1) {
    if (!ifa_extension_->GetLimitAdTracking(property,
                                            SB_ARRAY_SIZE_INT(property))) {
      DLOG(FATAL) << "Failed to get LimitAdTracking from IFA extension.";
    } else {
      result = std::atoi(property);
    }
  }
#endif
  return result;
}

#if defined(COBALT_ENABLE_EXTENDED_IFA)

std::string H5vccSystem::tracking_authorization_status() const {
  std::string result = "UNKNOWN";
  char property[kSystemPropertyMaxLength] = {0};
  if (ifa_extension_ && ifa_extension_->version >= 2) {
    if (!ifa_extension_->GetTrackingAuthorizationStatus(
            property, SB_ARRAY_SIZE_INT(property))) {
      DLOG(FATAL)
          << "Failed to get TrackingAuthorizationStatus from IFA extension.";
    } else {
      result = property;
    }
  }
  return result;
}

void H5vccSystem::ReceiveTrackingAuthorizationComplete() {
  // May be called by another thread.
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&H5vccSystem::ReceiveTrackingAuthorizationComplete,
                   base::Unretained(this)));
    return;
  }
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // Mark all promises complete and release the references.
  for (auto& promise : request_tracking_authorization_promises_) {
    promise->value().Resolve();
  }
  request_tracking_authorization_promises_.clear();
}

script::HandlePromiseVoid H5vccSystem::RequestTrackingAuthorization(
    script::EnvironmentSettings* environment_settings) {
  auto* global_wrappable = web::get_global_wrappable(environment_settings);
  script::HandlePromiseVoid promise =
      web::get_script_value_factory(environment_settings)
          ->CreateBasicPromise<void>();

  auto promise_reference =
      std::make_unique<script::ValuePromiseVoid::Reference>(global_wrappable,
                                                            promise);
  if (ifa_extension_ && ifa_extension_->version >= 2) {
    request_tracking_authorization_promises_.emplace_back(
        std::move(promise_reference));
    ifa_extension_->RequestTrackingAuthorization();
  } else {
    // Reject the promise since the API isn't available.
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](std::unique_ptr<script::ValuePromiseVoid::Reference>
                   promise_reference) { promise_reference->value().Reject(); },
            std::move(promise_reference)));
  }

  return promise;
}

#endif  // defined(COBALT_ENABLE_EXTENDED_IFA)

std::string H5vccSystem::region() const {
  // No region information.
  return "";
}

std::string H5vccSystem::version() const { return COBALT_VERSION; }

// In the future some platforms may launch custom help dialogs.
// return false to indicate the client should launch their own dialog.
bool H5vccSystem::TriggerHelp() const { return false; }

uint32 H5vccSystem::user_on_exit_strategy() const {
  // Convert from the Cobalt gyp setting variable's enum options to the H5VCC
  // interface enum options.
  std::string exit_strategy_str(
      configuration::Configuration::GetInstance()->CobaltUserOnExitStrategy());
  if (exit_strategy_str == "stop") {
    return static_cast<UserOnExitStrategy>(kUserOnExitStrategyClose);
  } else if (exit_strategy_str == "suspend") {
#if SB_IS(EVERGREEN)
    // Note: The status string used here must be synced with the
    // ComponentState::kUpdated status string defined in updater_module.cc.
    if (updater_->GetUpdateStatus() == "Update installed, pending restart") {
      return static_cast<UserOnExitStrategy>(kUserOnExitStrategyClose);
    }
#endif
    return static_cast<UserOnExitStrategy>(kUserOnExitStrategyMinimize);
  } else if (exit_strategy_str == "noexit") {
    return static_cast<UserOnExitStrategy>(kUserOnExitStrategyNoExit);
  } else {
    NOTREACHED() << "Unknown gyp-defined exit strategy.";
    return static_cast<UserOnExitStrategy>(kUserOnExitStrategyClose);
  }
}

}  // namespace h5vcc
}  // namespace cobalt
