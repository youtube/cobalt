// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkFontMgr_cobalt.h"

#include "base/debug/trace_event.h"
#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkFontConfigParser_cobalt.h"
#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkTypeface_cobalt.h"
#include "SkData.h"
#include "SkStream.h"
#include "SkString.h"
#include "SkTSearch.h"

SkFontMgr_Cobalt::SkFontMgr_Cobalt(
    const char* cobalt_font_config_directory,
    const char* cobalt_font_files_directory,
    const char* system_font_config_directory,
    const char* system_font_files_directory,
    const SkTArray<SkString, true>& default_families)
    : local_typeface_stream_manager_("Font.LocalTypefaceCache",
                                     COBALT_LOCAL_TYPEFACE_CACHE_SIZE_IN_BYTES),
      default_family_(NULL) {
  TRACE_EVENT0("cobalt::renderer", "SkFontMgr_Cobalt::SkFontMgr_Cobalt()");

  PriorityStyleSetArrayMap priority_fallback_families;

  // Cobalt fonts are loaded first.
  {
    TRACE_EVENT0("cobalt::renderer", "LoadCobaltFontFamilies");
    ParseConfigAndBuildFamilies(cobalt_font_config_directory,
                                cobalt_font_files_directory,
                                &priority_fallback_families);
  }

  // Only attempt to load the system font families if the system directories
  // have been populated.
  if (system_font_config_directory != NULL &&
      *system_font_config_directory != '\0' &&
      system_font_files_directory != NULL &&
      *system_font_files_directory != '\0') {
    TRACE_EVENT0("cobalt::renderer", "LoadSystemFontFamilies");
    ParseConfigAndBuildFamilies(system_font_config_directory,
                                system_font_files_directory,
                                &priority_fallback_families);
  }

  GeneratePriorityOrderedFallbackFamilies(priority_fallback_families);
  FindDefaultFamily(default_families);
}

SkTypeface* SkFontMgr_Cobalt::MatchFaceName(const char face_name[]) {
  if (!face_name) {
    return NULL;
  }

  SkAutoAsciiToLC face_name_to_lc(face_name);
  std::string face_name_string(face_name_to_lc.lc(), face_name_to_lc.length());

  // Lock the style sets mutex prior to accessing them.
  SkAutoMutexAcquire scoped_mutex(style_sets_mutex_);

  // Prioritize looking up the postscript name first since some of our client
  // applications prefer this method to specify face names.
  for (int i = 0; i <= 1; ++i) {
    NameToStyleSetMap& name_to_style_set_map =
        i == 0 ? font_postscript_name_to_style_set_map_
               : full_font_name_to_style_set_map_;

    NameToStyleSetMap::iterator style_set_iterator =
        name_to_style_set_map.find(face_name_string);
    if (style_set_iterator != name_to_style_set_map.end()) {
      SkFontStyleSet_Cobalt* style_set = style_set_iterator->second;
      SkTypeface* typeface =
          i == 0 ? style_set->MatchFontPostScriptName(face_name_string)
                 : style_set->MatchFullFontName(face_name_string);
      if (typeface != NULL) {
        return typeface;
      } else {
        // If no typeface was successfully created then remove the entry from
        // the map. It won't provide a successful result in subsequent calls
        // either.
        name_to_style_set_map.erase(style_set_iterator);
      }
    }
  }
  return NULL;
}

int SkFontMgr_Cobalt::onCountFamilies() const { return family_names_.count(); }

void SkFontMgr_Cobalt::onGetFamilyName(int index, SkString* family_name) const {
  if (index < 0 || family_names_.count() <= index) {
    family_name->reset();
    return;
  }

  family_name->set(family_names_[index]);
}

SkFontStyleSet_Cobalt* SkFontMgr_Cobalt::onCreateStyleSet(int index) const {
  if (index < 0 || family_names_.count() <= index) {
    return NULL;
  }

  NameToStyleSetMap::const_iterator family_iterator =
      name_to_family_map_.find(family_names_[index].c_str());
  if (family_iterator != name_to_family_map_.end()) {
    return SkRef(family_iterator->second);
  }

  return NULL;
}

SkFontStyleSet_Cobalt* SkFontMgr_Cobalt::onMatchFamily(
    const char family_name[]) const {
  if (!family_name) {
    return NULL;
  }

  SkAutoAsciiToLC family_name_to_lc(family_name);

  NameToStyleSetMap::const_iterator family_iterator = name_to_family_map_.find(
      std::string(family_name_to_lc.lc(), family_name_to_lc.length()));
  if (family_iterator != name_to_family_map_.end()) {
    return SkRef(family_iterator->second);
  }

  return NULL;
}

SkTypeface* SkFontMgr_Cobalt::onMatchFamilyStyle(
    const char family_name[], const SkFontStyle& style) const {
  SkTypeface* typeface = NULL;

  if (family_name) {
    SkAutoTUnref<SkFontStyleSet> style_set(matchFamily(family_name));
    typeface = style_set->matchStyle(style);
  }

  if (NULL == typeface) {
    typeface = default_family_->matchStyle(style);
  }

  return typeface;
}

SkTypeface* SkFontMgr_Cobalt::onMatchFaceStyle(const SkTypeface* family_member,
                                               const SkFontStyle& style) const {
  // Lock the style sets mutex prior to calling private |SkFontStyleSet_Cobalt|
  // functions that expect the mutex to already be locked.
  SkAutoMutexAcquire scoped_mutex(style_sets_mutex_);

  for (int i = 0; i < font_style_sets_.count(); ++i) {
    if (font_style_sets_[i]->ContainsTypeface(family_member)) {
      return font_style_sets_[i]->MatchStyleWithoutLocking(style);
    }
  }
  return NULL;
}

SkTypeface* SkFontMgr_Cobalt::onMatchFamilyStyleCharacter(
    const char family_name[], const SkFontStyle& style, const char bcp47_val[],
    SkUnichar character) const {
  const char** bcp47 = &bcp47_val;
  int bcp47_count = bcp47_val ? 1 : 0;

  // Remove const from the manager. SkFontMgr_Cobalt modifies its internals
  // within FindFamilyStyleCharacter().
  SkFontMgr_Cobalt* font_mgr = const_cast<SkFontMgr_Cobalt*>(this);

  // Lock the style sets mutex prior to calling |FindFamilyStyleCharacter|. It
  // expects the mutex to already be locked.
  SkAutoMutexAcquire scoped_mutex(style_sets_mutex_);

  // Search the fallback families for ones matching the requested language.
  // They are given priority over other fallback families in checking for
  // character support.
  for (int bcp47_index = bcp47_count; bcp47_index-- > 0;) {
    SkLanguage language(bcp47[bcp47_index]);
    while (!language.GetTag().isEmpty()) {
      SkTypeface* matching_typeface = font_mgr->FindFamilyStyleCharacter(
          style, language.GetTag(), character);
      if (matching_typeface) {
        return matching_typeface;
      }

      language = language.GetParent();
    }
  }

  // Try to find character among all fallback families with no language
  // requirement. This will select the first encountered family that contains
  // the character.
  SkTypeface* matching_typeface =
      font_mgr->FindFamilyStyleCharacter(style, SkString(), character);

  // If no family was found that supports the character, then just fall back
  // to the default family.
  return matching_typeface ? matching_typeface
                           : default_family_->MatchStyleWithoutLocking(style);
}

SkTypeface* SkFontMgr_Cobalt::onCreateFromData(SkData* data,
                                               int face_index) const {
  SkAutoTUnref<SkStreamAsset> stream(new SkMemoryStream(data));
  return createFromStream(stream, face_index);
}

SkTypeface* SkFontMgr_Cobalt::onCreateFromStream(SkStreamAsset* stream,
                                                 int face_index) const {
  TRACE_EVENT0("cobalt::renderer", "SkFontMgr_Cobalt::onCreateFromStream()");
  bool is_fixed_pitch;
  SkTypeface::Style style;
  SkString name;
  if (!SkTypeface_FreeType::ScanFont(stream, face_index, &name, &style,
                                     &is_fixed_pitch)) {
    return NULL;
  }
  return SkNEW_ARGS(SkTypeface_CobaltStream,
                    (stream, face_index, style, is_fixed_pitch, name));
}

SkTypeface* SkFontMgr_Cobalt::onCreateFromFile(const char path[],
                                               int face_index) const {
  TRACE_EVENT0("cobalt::renderer", "SkFontMgr_Cobalt::onCreateFromFile()");
  SkAutoTUnref<SkStreamAsset> stream(SkStream::NewFromFile(path));
  return stream.get() ? createFromStream(stream, face_index) : NULL;
}

SkTypeface* SkFontMgr_Cobalt::onLegacyCreateTypeface(
    const char family_name[], unsigned style_bits) const {
  SkTypeface::Style old_style = (SkTypeface::Style)style_bits;
  SkFontStyle style = SkFontStyle(
      old_style & SkTypeface::kBold ? SkFontStyle::kBold_Weight
                                    : SkFontStyle::kNormal_Weight,
      SkFontStyle::kNormal_Width,
      old_style & SkTypeface::kItalic ? SkFontStyle::kItalic_Slant
                                      : SkFontStyle::kUpright_Slant);
  return matchFamilyStyle(family_name, style);
}

void SkFontMgr_Cobalt::ParseConfigAndBuildFamilies(
    const char* font_config_directory, const char* font_files_directory,
    PriorityStyleSetArrayMap* priority_fallback_families) {
  SkTDArray<FontFamily*> font_families;
  {
    TRACE_EVENT0("cobalt::renderer", "SkFontConfigParser::GetFontFamilies()");
    SkFontConfigParser::GetFontFamilies(font_config_directory, &font_families);
  }
  BuildNameToFamilyMap(font_files_directory, &font_families,
                       priority_fallback_families);
  font_families.deleteAll();
}

void SkFontMgr_Cobalt::BuildNameToFamilyMap(
    const char* font_files_directory, SkTDArray<FontFamily*>* families,
    PriorityStyleSetArrayMap* priority_fallback_families) {
  TRACE_EVENT0("cobalt::renderer", "SkFontMgr_Cobalt::BuildNameToFamilyMap()");
  for (int i = 0; i < families->count(); i++) {
    FontFamily& family = *(*families)[i];
    bool named_family = family.names.count() > 0;

    if (!named_family) {
      // Unnamed families should always be fallback families.
      DCHECK(family.is_fallback_family);
      SkString& fallback_name = family.names.push_back();
      fallback_name.printf("%.2x##fallback", font_style_sets_.count());
    }

    SkAutoTUnref<SkFontStyleSet_Cobalt> new_set(
        SkNEW_ARGS(SkFontStyleSet_Cobalt,
                   (family, font_files_directory,
                    &local_typeface_stream_manager_, &style_sets_mutex_)));

    // Do not add the set if none of its fonts were available. This allows the
    // config file to specify a superset of all fonts, and ones that are not
    // included in the final package are stripped out.
    if (new_set->styles_.count() == 0) {
      continue;
    }

    font_style_sets_.push_back().reset(SkRef(new_set.get()));

    if (named_family) {
      for (int j = 0; j < family.names.count(); j++) {
        // Verify that the name was not previously added.
        if (name_to_family_map_.find(family.names[j].c_str()) ==
            name_to_family_map_.end()) {
          family_names_.push_back(family.names[j]);
          name_to_family_map_.insert(
              std::make_pair(family.names[j].c_str(), new_set.get()));
        } else {
          NOTREACHED() << "Duplicate Font name: \"" << family.names[j].c_str()
                       << "\"";
        }
      }
    }

    // If this is a fallback family, add it to the fallback family array
    // that corresponds to its priority. This will be used to generate a
    // priority-ordered fallback families list once the family map is fully
    // built.
    if (family.is_fallback_family) {
      (*priority_fallback_families)[family.fallback_priority].push_back(
          new_set.get());
    }

    for (SkAutoTUnref<SkFontStyleSet_Cobalt::SkFontStyleSetEntry_Cobalt>*
             font_style_set_entry = new_set->styles_.begin();
         font_style_set_entry != new_set->styles_.end();
         ++font_style_set_entry) {
      // On the first pass through, process the full font name.
      // On the second pass through, process the font postscript name.
      for (int i = 0; i <= 1; ++i) {
        const std::string& font_face_name =
            i == 0 ? (*font_style_set_entry)->full_font_name
                   : (*font_style_set_entry)->font_postscript_name;
        // If there is no font face name for this style entry, then there's
        // nothing to add. Simply skip past it.
        if (font_face_name.empty()) {
          continue;
        }

        NameToStyleSetMap& font_face_name_style_set_map =
            i == 0 ? full_font_name_to_style_set_map_
                   : font_postscript_name_to_style_set_map_;

        // Verify that the font face name was not already added.
        if (font_face_name_style_set_map.find(font_face_name) ==
            font_face_name_style_set_map.end()) {
          font_face_name_style_set_map[font_face_name] = new_set.get();
        } else {
          const std::string font_face_name_type =
              i == 0 ? "Full Font" : "Postscript";
          NOTREACHED() << "Duplicate " << font_face_name_type << " name: \""
                       << font_face_name << "\"";
        }
      }
    }
  }
}

void SkFontMgr_Cobalt::GeneratePriorityOrderedFallbackFamilies(
    const PriorityStyleSetArrayMap& priority_fallback_families) {
  // Reserve the combined size of the priority fallback families.
  size_t reserve_size = 0;
  for (PriorityStyleSetArrayMap::const_iterator iter =
           priority_fallback_families.begin();
       iter != priority_fallback_families.end(); ++iter) {
    reserve_size += iter->second.size();
  }
  fallback_families_.reserve(reserve_size);

  // Add each of the priority fallback families to |fallback_families_| in
  // reverse order, so that higher priorities are added first. This results in
  // |fallback_families_| being ordered by priority.
  for (PriorityStyleSetArrayMap::const_reverse_iterator iter =
           priority_fallback_families.rbegin();
       iter != priority_fallback_families.rend(); ++iter) {
    fallback_families_.insert(fallback_families_.end(), iter->second.begin(),
                              iter->second.end());
  }
}

void SkFontMgr_Cobalt::FindDefaultFamily(
    const SkTArray<SkString, true>& default_families) {
  CHECK(!font_style_sets_.empty());

  for (size_t i = 0; i < default_families.count(); ++i) {
    SkAutoTUnref<SkFontStyleSet_Cobalt> check_style_set(
        onMatchFamily(default_families[i].c_str()));
    if (NULL == check_style_set) {
      continue;
    }

    SkAutoTUnref<SkTypeface> check_typeface(
        check_style_set->MatchStyleWithoutLocking(SkFontStyle()));
    if (NULL != check_typeface) {
      default_family_ = check_style_set.get();
      break;
    }
  }

  if (NULL == default_family_) {
    SkAutoTUnref<SkTypeface> check_typeface(
        font_style_sets_[0]->MatchStyleWithoutLocking(SkFontStyle()));
    if (NULL != check_typeface) {
      default_family_ = font_style_sets_[0].get();
    }
  }

  CHECK(default_family_);
}

SkTypeface* SkFontMgr_Cobalt::FindFamilyStyleCharacter(
    const SkFontStyle& style, const SkString& language_tag,
    SkUnichar character) {
  if (!font_character_map::IsCharacterValid(character)) {
    return NULL;
  }

  StyleSetArray* fallback_families = GetMatchingFallbackFamilies(language_tag);
  for (int i = 0; i < fallback_families->size(); ++i) {
    SkFontStyleSet_Cobalt* family = (*fallback_families)[i];
    if (family->ContainsCharacter(style, character)) {
      SkTypeface* matching_typeface = family->MatchStyleWithoutLocking(style);
      if (matching_typeface) {
        return matching_typeface;
      }
    }
  }

  return NULL;
}

SkFontMgr_Cobalt::StyleSetArray* SkFontMgr_Cobalt::GetMatchingFallbackFamilies(
    const SkString& language_tag) {
  if (language_tag.isEmpty()) {
    return &fallback_families_;
  }

  StyleSetArray*& language_fallback_families =
      language_fallback_families_map_[language_tag.c_str()];

  // The fallback families for a specific language tag are lazily populated. If
  // this is the first time that this tag has been encountered, then create and
  // populate the fallback families now.
  if (language_fallback_families == NULL) {
    language_fallback_families_array_.push_back(new StyleSetArray);
    language_fallback_families = *language_fallback_families_array_.rbegin();

    for (StyleSetArray::iterator iter = fallback_families_.begin();
         iter != fallback_families_.end(); ++iter) {
      SkFontStyleSet_Cobalt* fallback_family = *iter;
      if (fallback_family->language_.GetTag().startsWith(
              language_tag.c_str())) {
        language_fallback_families->push_back(fallback_family);
      }
    }
  }

  return language_fallback_families;
}
