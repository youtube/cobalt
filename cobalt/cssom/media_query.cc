/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/cssom/media_query.h"

#include "base/memory/ref_counted.h"
#include "cobalt/cssom/media_feature.h"

namespace cobalt {
namespace cssom {

MediaQuery::MediaQuery() {}

MediaQuery::MediaQuery(MediaType media_type) : media_type_(media_type) {}

MediaQuery::MediaQuery(MediaType media_type,
                       scoped_ptr<MediaFeatureList> media_feature_list)
    : media_type_(media_type),
      media_feature_list_(media_feature_list.release()) {}

std::string MediaQuery::media_query() {
  // TODO(***REMOVED***): Implement serialization of MediaQuery.
  NOTIMPLEMENTED() << "Serialization of MediaQuery not implemented yet.";
  return "";
}

}  // namespace cssom
}  // namespace cobalt
