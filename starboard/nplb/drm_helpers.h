// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_NPLB_DRM_HELPERS_H_
#define STARBOARD_NPLB_DRM_HELPERS_H_

#include "starboard/shared/starboard/drm/drm_test_helpers.h"

namespace starboard {
namespace nplb {

using ::starboard::shared::starboard::drm::DummyServerCertificateUpdatedFunc;
using ::starboard::shared::starboard::drm::DummySessionClosedFunc;
using ::starboard::shared::starboard::drm::DummySessionKeyStatusesChangedFunc;
using ::starboard::shared::starboard::drm::DummySessionUpdatedFunc;
using ::starboard::shared::starboard::drm::DummySessionUpdateRequestFunc;

using ::starboard::shared::starboard::drm::CreateDummyDrmSystem;

using ::starboard::shared::starboard::drm::kEncryptionSchemes;
using ::starboard::shared::starboard::drm::kKeySystems;
using ::starboard::shared::starboard::drm::kWidevineCertificate;

using ::starboard::shared::starboard::drm::kCencInitData;

}  // namespace nplb
}  // namespace starboard

#endif  // STARBOARD_NPLB_DRM_HELPERS_H_
