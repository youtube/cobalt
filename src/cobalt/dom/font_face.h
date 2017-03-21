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

#ifndef COBALT_DOM_FONT_FACE_H_
#define COBALT_DOM_FONT_FACE_H_

#include <set>
#include <string>
#include <vector>

#include "cobalt/render_tree/font.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace dom {

// FontFaceSource contains a single font-face source, representing either an
// external reference or a locally-installed font face name.
// https://www.w3.org/TR/css3-fonts/#descdef-src
class FontFaceSource {
 public:
  explicit FontFaceSource(const std::string& name) : name_(name) {}
  explicit FontFaceSource(const GURL& url) : url_(url) {}

  bool IsUrlSource() const { return !url_.is_empty(); }

  const std::string& GetName() const { return name_; }
  const GURL& GetUrl() const { return url_; }

  bool operator==(const FontFaceSource& other) const {
    return name_ == other.name_ && url_ == other.url_;
  }

 private:
  std::string name_;
  GURL url_;
};

typedef std::vector<FontFaceSource> FontFaceSources;

// FontFaceStyleSet contains a collection of @font-face rules with a matching
// font-family, providing the ability to add additional entries to the
// collection and to retrieve the entry from the collection that most closely
// matches a given style.
// https://www.w3.org/TR/css3-fonts/#font-face-rule
class FontFaceStyleSet {
 public:
  // FontFaceStyleSet::Entry contains the style and source information for a
  // single @font-face rule.
  // https://www.w3.org/TR/css3-fonts/#descdef-src
  // https://www.w3.org/TR/css3-fonts/#font-prop-desc
  struct Entry {
   public:
    bool operator==(const Entry& other) const {
      return style.weight == other.style.weight &&
             style.slant == other.style.slant && sources == other.sources;
    }

    render_tree::FontStyle style;
    FontFaceSources sources;
  };

  void AddEntry(const Entry& entry);

  // Walk all of the style set's entries, inserting any url sources encountered
  // into the set. All pre-existing url entries within the set are retained.
  void CollectUrlSources(std::set<GURL>* urls) const;

  // Returns the style set entry with the style most closely matching the
  // requested pattern. If the style set contains any entries, it is guaranteed
  // to not return NULL.
  const Entry* MatchStyle(const render_tree::FontStyle& pattern) const;

  bool operator==(const FontFaceStyleSet& other) const {
    return entries_ == other.entries_;
  }

 private:
  typedef std::vector<Entry> Entries;

  // Returns the index of the entry with a style most closely matching the
  // pattern (the lower the score, the more closely it matches). In the case of
  // a tie, the earliest encountered entry is given priority.
  size_t GetClosestStyleEntryIndex(const render_tree::FontStyle& pattern) const;

  // Returns the match score between two patterns. The score logic matches that
  // within SkFontStyleSet_Cobalt::match_score().
  int MatchScore(const render_tree::FontStyle& pattern,
                 const render_tree::FontStyle& candidate) const;

  Entries entries_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_FONT_FACE_H_
