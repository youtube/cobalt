// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/cssom/media_list.h"

#include <limits>

#include "base/logging.h"
#include "cobalt/cssom/css_parser.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/media_query.h"
#include "cobalt/cssom/property_value.h"

namespace cobalt {
namespace cssom {

MediaList::MediaList() : css_parser_(NULL) {}

MediaList::MediaList(CSSParser* css_parser) : css_parser_(css_parser) {}

std::string MediaList::media_text() const {
  std::string media;
  size_t mediaitem = 0;
  while (true) {
    media += media_queries_[mediaitem]->media_query();
    if (++mediaitem < media_queries_.size())
      media += ", ";
    else
      break;
  }
  return media;
}

void MediaList::set_media_text(const std::string& css_text) {
  DCHECK(css_parser_);
  scoped_refptr<MediaList> media_list =
      css_parser_->ParseMediaList(css_text, GetInlineSourceLocation());

  media_queries_.swap(media_list->media_queries_);
}

// Returns the number of MediaQuery objects represented by the collection.
unsigned int MediaList::length() const {
  CHECK_LE(media_queries_.size(), std::numeric_limits<unsigned int>::max());
  return static_cast<unsigned int>(media_queries_.size());
}

std::string MediaList::Item(unsigned int index) const {
  return index < media_queries_.size() ? media_queries_[index]->media_query()
                                       : "";
}

// Inserts a new media query string into the current style sheet. This Web API
// takes a string as input and parses it into a MediaQuery.
void MediaList::AppendMedium(const std::string& medium) {
  DCHECK(css_parser_);
  scoped_refptr<MediaQuery> media_query =
      css_parser_->ParseMediaQuery(medium, GetInlineSourceLocation());

  // TODO: Don't add the query if the same one already exists in the
  // collection. See https://www.w3.org/TR/cssom/#medialist for details.
  media_queries_.push_back(media_query);
}

void MediaList::Append(const scoped_refptr<MediaQuery>& media_query) {
  media_queries_.push_back(media_query);
}

bool MediaList::EvaluateConditionValue(const math::Size& viewport_size) {
  for (MediaQueries::iterator it = media_queries_.begin();
       it != media_queries_.end(); ++it) {
    if ((*it)->EvaluateConditionValue(viewport_size)) {
      return true;
    }
  }
  return false;
}

void MediaList::TraceMembers(script::Tracer* tracer) {
  tracer->TraceItems(media_queries_);
}

MediaList::~MediaList() {}

}  // namespace cssom
}  // namespace cobalt
