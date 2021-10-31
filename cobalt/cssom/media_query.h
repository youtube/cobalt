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

#ifndef COBALT_CSSOM_MEDIA_QUERY_H_
#define COBALT_CSSOM_MEDIA_QUERY_H_

#include <memory>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "cobalt/cssom/media_feature.h"
#include "cobalt/cssom/viewport_size.h"
#include "cobalt/math/size.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace cssom {

// The media query represents the expression of the @media conditional rule
// for example: 'screen and (max-width: 640px) and (aspect-ratio: 4/3)'
//   https://www.w3.org/TR/css3-mediaqueries
class MediaQuery : public script::Wrappable {
 public:
  MediaQuery();
  explicit MediaQuery(bool evaluated_media_type);
  MediaQuery(bool evaluated_media_type,
             std::unique_ptr<MediaFeatures> media_features);

  // Custom, not in any spec.
  //
  std::string media_query();

  bool EvaluateConditionValue(const ViewportSize& viewport_size);

  DEFINE_WRAPPABLE_TYPE(MediaQuery);

 private:
  bool evaluated_media_type_;
  std::unique_ptr<MediaFeatures> media_features_;

  DISALLOW_COPY_AND_ASSIGN(MediaQuery);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_MEDIA_QUERY_H_
