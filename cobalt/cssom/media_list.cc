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

#include "cobalt/cssom/media_list.h"

#include <limits>

#include "base/logging.h"
#include "cobalt/cssom/css_parser.h"
#include "cobalt/cssom/media_query.h"

namespace cobalt {
namespace cssom {

MediaList::MediaList(CSSParser* css_parser) : css_parser_(css_parser) {}

scoped_refptr<MediaQuery> MediaList::Item(unsigned int index) const {
  return index < media_queries_.size() ? media_queries_[index] : NULL;
}

// Returns the number of MediaQuery objects represented by the collection.
unsigned int MediaList::length() const {
  CHECK_LE(media_queries_.size(), std::numeric_limits<unsigned int>::max());
  return static_cast<unsigned int>(media_queries_.size());
}

// Inserts a new media query string into the current style sheet. This Web API
// takes a string as input and parses it into a MediaQuery.
void MediaList::AppendMedium(const std::string& medium) {
  scoped_refptr<MediaQuery> media_query = css_parser_->ParseMediaQuery(
      medium, base::SourceLocation("[object CSSStyleSheet]", 1, 1));

  if (!media_query) {
    return;
  }

  // TODO(***REMOVED***): Don't add the query if the same one already exists in the
  // collection. See http://www.w3.org/TR/cssom/#medialist for details.
  media_queries_.push_back(media_query);
}

void MediaList::Append(const scoped_refptr<MediaQuery>& media_query) {
  media_queries_.push_back(media_query);
}

}  // namespace cssom
}  // namespace cobalt
