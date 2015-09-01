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

#ifndef CSSOM_MEDIA_LIST_H_
#define CSSOM_MEDIA_LIST_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace cssom {

class CSSParser;
class MediaQuery;

typedef std::vector<scoped_refptr<MediaQuery> > MediaQueries;

// The MediaList interface represents a list of Media Queries
//   http://www.w3.org/TR/cssom/#medialist

class MediaList : public script::Wrappable {
 public:
  explicit MediaList(CSSParser* css_parser);

  // Web API: MediaList
  //
  std::string media_text() const;
  void set_media_text(const std::string& css_text);

  // Returns the number of MediaQuery objects represented by the collection.
  unsigned int length() const;
  // Returns the index-th MediaQuery object in the collection.
  // Returns null if there is no index-th object in the collection.
  std::string Item(unsigned int index) const;

  // Inserts a new media query string into the current style sheet. This Web API
  // takes a string as input and parses it into a MediaQuery.
  void AppendMedium(const std::string& medium);

  void Append(const scoped_refptr<MediaQuery>& media_query);

  DEFINE_WRAPPABLE_TYPE(MediaList);

 private:
  MediaQueries media_queries_;

  CSSParser* const css_parser_;

  DISALLOW_COPY_AND_ASSIGN(MediaList);
};

}  // namespace cssom
}  // namespace cobalt

#endif  // CSSOM_MEDIA_LIST_H_
