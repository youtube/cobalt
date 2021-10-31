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

#ifndef COBALT_DOM_EME_MEDIA_KEY_STATUS_MAP_H_
#define COBALT_DOM_EME_MEDIA_KEY_STATUS_MAP_H_

#include <map>
#include <string>

#include "base/memory/ref_counted.h"
#include "cobalt/dom/buffer_source.h"
#include "cobalt/dom/eme/media_key_status.h"
#include "cobalt/script/callback_function.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {
namespace eme {

// Represents a read-only map of key IDs to the current status of the associated
// key.
//   https://www.w3.org/TR/encrypted-media/#mediakeystatusmap-interface
class MediaKeyStatusMap : public script::Wrappable {
 public:
  typedef script::CallbackFunction<void(
      const std::string&, const BufferSource&,
      const scoped_refptr<MediaKeyStatusMap>&)>
      ForEachCallback;
  typedef script::ScriptValue<ForEachCallback> ForEachCallbackArg;

  // Custom, not in any spec.
  //
  MediaKeyStatusMap() {}

  void Clear() { key_statuses_.clear(); }

  void Add(const std::string& key_id, MediaKeyStatus key_status);

  // Web IDL: MediaKeyStatusMap.
  //
  uint32_t size() const { return static_cast<uint32_t>(key_statuses_.size()); }
  MediaKeyStatus Get(const BufferSource& key_id) const {
    std::string key_id_copy = GetStringFromBufferSource(key_id);
    const auto& iter = key_statuses_.find(key_id_copy);
    // TODO: Return "undefined" if `key_id` cannot be found.
    return iter == key_statuses_.end() ? kMediaKeyStatusInternalError
                                       : iter->second;
  }

  bool Has(const BufferSource& key_id) const {
    std::string key_id_copy = GetStringFromBufferSource(key_id);
    return key_statuses_.find(key_id_copy) != key_statuses_.end();
  }

  void ForEach(script::EnvironmentSettings* settings,
               const ForEachCallbackArg& callback);

  DEFINE_WRAPPABLE_TYPE(MediaKeyStatusMap);

 private:
  static std::string GetStringFromBufferSource(
      const BufferSource& buffer_source) {
    const uint8* buffer;
    int buffer_size;

    GetBufferAndSize(buffer_source, &buffer, &buffer_size);

    DCHECK(buffer);
    DCHECK_GE(buffer_size, 0);
    if (buffer && buffer_size > 0) {
      return std::string(buffer, buffer + buffer_size);
    }
    return "";
  }

  std::map<std::string, MediaKeyStatus> key_statuses_;

  DISALLOW_COPY_AND_ASSIGN(MediaKeyStatusMap);
};

}  // namespace eme
}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_EME_MEDIA_KEY_STATUS_MAP_H_
