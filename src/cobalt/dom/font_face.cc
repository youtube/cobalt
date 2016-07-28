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

#include "cobalt/dom/font_face.h"

#include <cmath>
#include <limits>

namespace cobalt {
namespace dom {

void FontFaceStyleSet::AddEntry(const FontFaceStyleSet::Entry& entry) {
  entries_.push_back(entry);
}

void FontFaceStyleSet::CollectUrlSources(std::set<GURL>* urls) const {
  for (Entries::const_iterator entry_iterator = entries_.begin();
       entry_iterator != entries_.end(); ++entry_iterator) {
    const FontFaceSources& entry_sources = entry_iterator->sources;
    for (FontFaceSources::const_iterator source_iterator =
             entry_sources.begin();
         source_iterator != entry_sources.end(); ++source_iterator) {
      if (source_iterator->IsUrlSource()) {
        urls->insert(source_iterator->GetUrl());
      }
    }
  }
}

const FontFaceStyleSet::Entry* FontFaceStyleSet::MatchStyle(
    const render_tree::FontStyle& pattern) const {
  return entries_.empty() ? NULL
                          : &entries_[GetClosestStyleEntryIndex(pattern)];
}

size_t FontFaceStyleSet::GetClosestStyleEntryIndex(
    const render_tree::FontStyle& pattern) const {
  size_t closest_index = 0;
  int min_score = std::numeric_limits<int>::max();
  for (size_t i = 0; i < entries_.size(); ++i) {
    int score = MatchScore(pattern, entries_[i].style);
    if (score < min_score) {
      closest_index = i;
      min_score = score;
    }
  }

  return closest_index;
}

int FontFaceStyleSet::MatchScore(
    const render_tree::FontStyle& pattern,
    const render_tree::FontStyle& candidate) const {
  int score = 0;
  score += (pattern.slant == candidate.slant) ? 0 : 1000;
  score += std::abs(pattern.weight - candidate.weight);
  return score;
}

}  // namespace dom
}  // namespace cobalt
