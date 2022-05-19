// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/dom/eme/media_key_status_map.h"

#include "base/logging.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/script/array_buffer.h"
#include "cobalt/script/array_buffer_view.h"
#include "cobalt/web/context.h"
#include "cobalt/web/environment_settings.h"

namespace cobalt {
namespace dom {
namespace eme {

namespace {

std::string ConvertKeyIdToAscii(const std::string& key_id) {
  const char kHexChars[] = "0123456789ABCDEF";
  std::string key_id_in_ascii;
  for (auto ch : key_id) {
    key_id_in_ascii += kHexChars[static_cast<unsigned char>(ch) / 16];
    key_id_in_ascii += kHexChars[static_cast<unsigned char>(ch) % 16];
  }
  return key_id_in_ascii;
}

std::string ConvertKeyStatusToString(MediaKeyStatus key_status) {
  switch (key_status) {
    case kMediaKeyStatusUsable:
      return "usable";
    case kMediaKeyStatusExpired:
      return "expired";
    case kMediaKeyStatusReleased:
      return "released";
    case kMediaKeyStatusOutputRestricted:
      return "output-restricted";
    case kMediaKeyStatusOutputDownscaled:
      return "output-downscaled";
    case kMediaKeyStatusStatusPending:
      return "status-pending";
    case kMediaKeyStatusInternalError:
      return "internal-error";
  }

  NOTREACHED();
  return "internal-error";
}

web::BufferSource ConvertStringToBufferSource(
    script::EnvironmentSettings* settings, const std::string& str) {
  DCHECK(settings);
  auto* global_environment =
      base::polymorphic_downcast<web::EnvironmentSettings*>(settings)
          ->context()
          ->global_environment();
  DCHECK(global_environment);
  script::Handle<script::ArrayBuffer> array_buffer =
      script::ArrayBuffer::New(global_environment, str.data(), str.size());
  return web::BufferSource(array_buffer);
}

}  // namespace

void MediaKeyStatusMap::Add(const std::string& key_id,
                            MediaKeyStatus key_status) {
  LOG(INFO) << "MediaKeyStatusMap::Add()  " << ConvertKeyIdToAscii(key_id)
            << " => " << ConvertKeyStatusToString(key_status);
  key_statuses_[key_id] = key_status;
}

void MediaKeyStatusMap::ForEach(script::EnvironmentSettings* settings,
                                const ForEachCallbackArg& callback) {
  TRACE_EVENT0("cobalt::dom::eme", "MediaKeyStatusMap::ForEach()");
  ForEachCallbackArg::Reference reference(this, callback);

  for (auto& key_status : key_statuses_) {
    reference.value().Run(
        ConvertKeyStatusToString(key_status.second),
        ConvertStringToBufferSource(settings, key_status.first), this);
  }
}

}  // namespace eme
}  // namespace dom
}  // namespace cobalt
