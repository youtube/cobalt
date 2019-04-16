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

#include <memory>

#include "cobalt/cssom/media_query.h"

#include "base/memory/ref_counted.h"
#include "cobalt/cssom/media_feature.h"
#include "cobalt/cssom/viewport_size.h"

namespace cobalt {
namespace cssom {

// If the media query list is empty (i.e. the declaration is the empty string or
// consists solely of whitespace) it evaluates to true.
//   https://www.w3.org/TR/css3-mediaqueries/#error-handling
MediaQuery::MediaQuery() : evaluated_media_type_(true) {}

MediaQuery::MediaQuery(bool evaluated_media_type)
    : evaluated_media_type_(evaluated_media_type) {}

MediaQuery::MediaQuery(bool evaluated_media_type,
                       std::unique_ptr<MediaFeatures> media_features)
    : evaluated_media_type_(evaluated_media_type),
      media_features_(media_features.release()) {}

std::string MediaQuery::media_query() {
  // TODO: Implement serialization of MediaQuery.
  NOTIMPLEMENTED() << "Serialization of MediaQuery not implemented yet.";
  return "";
}

bool MediaQuery::EvaluateConditionValue(const ViewportSize& viewport_size) {
  if (!evaluated_media_type_) {
    return false;
  }
  if (media_features_) {
    for (MediaFeatures::iterator it = media_features_->begin();
         it != media_features_->end(); ++it) {
      if (!(*it)->EvaluateConditionValue(viewport_size)) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace cssom
}  // namespace cobalt
