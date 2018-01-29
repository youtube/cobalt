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

#include "SkData.h"
#include "SkGraphics.h"
#include "SkStream.h"
#include "SkString.h"
#include "SkTSearch.h"
#include "base/debug/trace_event.h"
#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkFontConfigParser_cobalt.h"
#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkFreeType_cobalt.h"
#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkTypeface_cobalt.h"

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

void SkFontMgr_Cobalt::PurgeCaches() {
  SkGraphics::PurgeFontCache();
  local_typeface_stream_manager_.PurgeUnusedMemoryChunks();

  // Lock the family mutex prior to purging each family's unreferenced
  // typefaces.
  SkAutoMutexAcquire scoped_mutex(family_mutex_);
  for (int i = 0; i < families_.count(); ++i) {
    families_[i]->PurgeUnreferencedTypefaces();
  }
}

SkTypeface* SkFontMgr_Cobalt::MatchFaceName(const char face_name[]) {
  if (face_name == NULL) {
    return NULL;
  }

  SkAutoAsciiToLC face_name_to_lc(face_name);
  std::string face_name_string(face_name_to_lc.lc(), face_name_to_lc.length());

  // Lock the family mutex prior to accessing them.
  SkAutoMutexAcquire scoped_mutex(family_mutex_);

  // Prioritize looking up the postscript name first since some of our client
  // applications prefer this method to specify face names.
  for (int i = 0; i <= 1; ++i) {
    NameToStyleSetMap& name_to_family_map =
        i == 0 ? font_postscript_name_to_family_map_
               : full_font_name_to_family_map_;

    NameToStyleSetMap::iterator family_map_iterator =
        name_to_family_map.find(face_name_string);
    if (family_map_iterator != name_to_family_map.end()) {
      SkFontStyleSet_Cobalt* family = family_map_iterator->second;
      SkTypeface* typeface =
          i == 0 ? family->MatchFontPostScriptName(face_name_string)
                 : family->MatchFullFontName(face_name_string);
      if (typeface != NULL) {
        return typeface;
      } else {
        // If no typeface was successfully created then remove the entry from
        // the map. It won't provide a successful result in subsequent calls
        // either.
        name_to_family_map.erase(family_map_iterator);
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
  if (family_name == NULL) {
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

  if (family_name != NULL) {
    sk_sp<SkFontStyleSet> family(matchFamily(family_name));
    typeface = family->matchStyle(style);
  }

  if (typeface == NULL) {
    typeface = default_family_->matchStyle(style);
  }

  return typeface;
}

SkTypeface* SkFontMgr_Cobalt::onMatchFaceStyle(const SkTypeface* family_member,
                                               const SkFontStyle& style) const {
  // Lock the family mutex prior to calling private SkFontStyleSet_Cobalt
  // functions that expect the mutex to already be locked.
  SkAutoMutexAcquire scoped_mutex(family_mutex_);

  for (int i = 0; i < families_.count(); ++i) {
    if (families_[i]->ContainsTypeface(family_member)) {
      return families_[i]->MatchStyleWithoutLocking(style);
    }
  }
  return NULL;
}

SkTypeface* SkFontMgr_Cobalt::onMatchFamilyStyleCharacter(
    const char family_name[], const SkFontStyle& style, const char* bcp47[],
    int bcp47_count, SkUnichar character) const {
  // Remove const from the manager. SkFontMgr_Cobalt modifies its internals
  // within FindFamilyStyleCharacter().
  SkFontMgr_Cobalt* font_manager = const_cast<SkFontMgr_Cobalt*>(this);

  // Lock the family mutex prior to calling FindFamilyStyleCharacter(). It
  // expects the mutex to already be locked.
  SkAutoMutexAcquire scoped_mutex(family_mutex_);

  // Search the fallback families for ones matching the requested language.
  // They are given priority over other fallback families in checking for
  // character support.
  for (int bcp47_index = bcp47_count; bcp47_index-- > 0;) {
    SkLanguage language(bcp47[bcp47_index]);
    while (!language.GetTag().isEmpty()) {
      SkTypeface* matching_typeface = font_manager->FindFamilyStyleCharacter(
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
      font_manager->FindFamilyStyleCharacter(style, SkString(), character);

  // If no family was found that supports the character, then just fall back
  // to the default family.
  return matching_typeface ? matching_typeface
                           : default_family_->MatchStyleWithoutLocking(style);
}

SkTypeface* SkFontMgr_Cobalt::onCreateFromData(SkData* data,
                                               int face_index) const {
  scoped_ptr<SkStreamAsset> stream(
      new SkMemoryStream(data->data(), data->size()));
  return createFromStream(stream.get(), face_index);
}

SkTypeface* SkFontMgr_Cobalt::onCreateFromStream(SkStreamAsset* stream,
                                                 int face_index) const {
  TRACE_EVENT0("cobalt::renderer", "SkFontMgr_Cobalt::onCreateFromStream()");
  bool is_fixed_pitch;
  SkTypeface::Style style;
  SkString name;
  if (!sk_freetype_cobalt::ScanFont(stream, face_index, &name, &style,
                                    &is_fixed_pitch)) {
    return NULL;
  }
  return new SkTypeface_CobaltStream(stream, face_index, style, is_fixed_pitch,
                                     name);
}

SkTypeface* SkFontMgr_Cobalt::onCreateFromFile(const char path[],
                                               int face_index) const {
  TRACE_EVENT0("cobalt::renderer", "SkFontMgr_Cobalt::onCreateFromFile()");
  std::unique_ptr<SkStreamAsset> stream = SkStream::MakeFromFile(path);
  return stream.get() ? createFromStream(stream.release(), face_index) : NULL;
}

SkTypeface* SkFontMgr_Cobalt::onLegacyCreateTypeface(const char family_name[],
                                                     SkFontStyle style) const {
  return matchFamilyStyle(family_name, style);
}

void SkFontMgr_Cobalt::ParseConfigAndBuildFamilies(
    const char* font_config_directory, const char* font_files_directory,
    PriorityStyleSetArrayMap* priority_fallback_families) {
  SkTDArray<FontFamilyInfo*> config_font_families;
  {
    TRACE_EVENT0("cobalt::renderer", "SkFontConfigParser::GetFontFamilies()");
    SkFontConfigParser::GetFontFamilies(font_config_directory,
                                        &config_font_families);
  }
  BuildNameToFamilyMap(font_files_directory, &config_font_families,
                       priority_fallback_families);
  config_font_families.deleteAll();
}

void SkFontMgr_Cobalt::BuildNameToFamilyMap(
    const char* font_files_directory,
    SkTDArray<FontFamilyInfo*>* config_font_families,
    PriorityStyleSetArrayMap* priority_fallback_families) {
  TRACE_EVENT0("cobalt::renderer", "SkFontMgr_Cobalt::BuildNameToFamilyMap()");
  for (int i = 0; i < config_font_families->count(); i++) {
    FontFamilyInfo& family_info = *(*config_font_families)[i];
    bool is_named_family = family_info.names.count() > 0;

    if (!is_named_family) {
      // Unnamed families should always be fallback families.
      DCHECK(family_info.is_fallback_family);
      SkString& fallback_name = family_info.names.push_back();
      fallback_name.printf("%.2x##fallback", families_.count());
    }

    sk_sp<SkFontStyleSet_Cobalt> new_family(new SkFontStyleSet_Cobalt(
        family_info, font_files_directory, &local_typeface_stream_manager_,
        &family_mutex_));

    // Do not add the family if none of its fonts were available. This allows
    // the configuration files to specify a superset of all fonts, and ones that
    // are not included in the final package are stripped out.
    if (new_family->styles_.count() == 0) {
      continue;
    }

    families_.push_back().reset(SkRef(new_family.get()));

    if (is_named_family) {
      for (int j = 0; j < family_info.names.count(); j++) {
        // Verify that the name was not previously added.
        if (name_to_family_map_.find(family_info.names[j].c_str()) ==
            name_to_family_map_.end()) {
          family_names_.push_back(family_info.names[j]);
          name_to_family_map_.insert(
              std::make_pair(family_info.names[j].c_str(), new_family.get()));
        } else {
          NOTREACHED() << "Duplicate Font name: \""
                       << family_info.names[j].c_str() << "\"";
        }
      }
    }

    // If this is a fallback family, add it to the fallback family array
    // that corresponds to its priority. This will be used to generate a
    // priority-ordered fallback families list once the family map is fully
    // built.
    if (family_info.is_fallback_family) {
      (*priority_fallback_families)[family_info.fallback_priority].push_back(
          new_family.get());
    }

    for (sk_sp<SkFontStyleSet_Cobalt::SkFontStyleSetEntry_Cobalt>*
             family_style_entry = new_family->styles_.begin();
         family_style_entry != new_family->styles_.end();
         ++family_style_entry) {
      // On the first pass through, process the full font name.
      // On the second pass through, process the font postscript name.
      for (int i = 0; i <= 1; ++i) {
        const std::string& font_face_name =
            i == 0 ? (*family_style_entry)->full_font_name
                   : (*family_style_entry)->font_postscript_name;
        // If there is no font face name for this style entry, then there's
        // nothing to add. Simply skip past it.
        if (font_face_name.empty()) {
          continue;
        }

        NameToStyleSetMap& font_face_name_to_family_map =
            i == 0 ? full_font_name_to_family_map_
                   : font_postscript_name_to_family_map_;

        // Verify that the font face name was not already added.
        if (font_face_name_to_family_map.find(font_face_name) ==
            font_face_name_to_family_map.end()) {
          font_face_name_to_family_map[font_face_name] = new_family.get();
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
  CHECK(!families_.empty());

  for (size_t i = 0; i < default_families.count(); ++i) {
    sk_sp<SkFontStyleSet_Cobalt> check_family(
        onMatchFamily(default_families[i].c_str()));
    if (check_family.get() == NULL) {
      continue;
    }

    sk_sp<SkTypeface> check_typeface(
        check_family->MatchStyleWithoutLocking(SkFontStyle()));
    if (check_typeface.get() != NULL) {
      default_family_ = check_family.get();
      break;
    }
  }

  if (default_family_ == NULL) {
    sk_sp<SkTypeface> check_typeface(
        families_[0]->MatchStyleWithoutLocking(SkFontStyle()));
    if (check_typeface.get() != NULL) {
      default_family_ = families_[0].get();
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
