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

#include "cobalt/ifa/ifa.h"

#include <string>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/notreached.h"
#include "starboard/common/system_property.h"
#include "starboard/system.h"

namespace cobalt {
namespace ifa {

// static
Ifa* Ifa::GetInstance() {
  return base::Singleton<Ifa, base::LeakySingletonTraits<Ifa>>::get();
}

Ifa::Ifa()
    : task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      ifa_extension_(static_cast<const StarboardExtensionIfaApi*>(
          SbSystemGetExtension(kStarboardExtensionIfaName))) {
  if (!ifa_extension_) {
    return;
  }
  DCHECK_EQ(std::string(ifa_extension_->name), kStarboardExtensionIfaName)
      << "Unexpected extension name.";
  DCHECK_GE(ifa_extension_->version, 1) << "Unexpected extension version.";

  if (ifa_extension_->version >= 2) {
    ifa_extension_->RegisterTrackingAuthorizationCallback(
        this, [](void* context) {
          DCHECK(context) << "Callback called with NULL context";
          if (context) {
            static_cast<Ifa*>(context)->ReceiveTrackingAuthorizationComplete();
          }
        });
  }
}

Ifa::~Ifa() {
  if (ifa_extension_ && ifa_extension_->version >= 2) {
    ifa_extension_->UnregisterTrackingAuthorizationCallback();
  }
}

std::string Ifa::tracking_authorization_status() const {
  if (!ifa_extension_ || ifa_extension_->version < 2) {
    return "NOT_SUPPORTED";
  }

  std::string result = "UNKNOWN";
  char property[starboard::kSystemPropertyMaxLength] = {0};
  if (!ifa_extension_->GetTrackingAuthorizationStatus(
          property, starboard::kSystemPropertyMaxLength)) {
    DLOG(FATAL)
        << "Failed to get TrackingAuthorizationStatus from IFA extension.";
  } else {
    result = property;
  }
  return result;
}

bool Ifa::CanRequestTrackingAuthorization() const {
  return ifa_extension_ && ifa_extension_->version >= 2;
}

void Ifa::RequestTrackingAuthorization(
    base::OnceCallback<void(bool)> callback) {
  DCHECK(CanRequestTrackingAuthorization());

  request_tracking_authorization_callbacks_.emplace_back(std::move(callback));
  ifa_extension_->RequestTrackingAuthorization();
}

void Ifa::ReceiveTrackingAuthorizationComplete() {
  // May be called by another thread.
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Ifa::ReceiveTrackingAuthorizationComplete,
                                  base::Unretained(this)));
    return;
  }
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  // TODO: make sure this is running in the right thread (ref c25)

  // Mark all promises complete and release the references.
  for (auto& callback : request_tracking_authorization_callbacks_) {
    std::move(callback).Run(true);
  }
  request_tracking_authorization_callbacks_.clear();
}

}  // namespace ifa
}  // namespace cobalt
