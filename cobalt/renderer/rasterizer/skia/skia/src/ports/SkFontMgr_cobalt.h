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

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKFONTMGR_COBALT_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKFONTMGR_COBALT_H_

#include <map>
#include <string>
#include <vector>

#include "base/containers/small_map.h"
#include "base/hash_tables.h"
#include "base/memory/scoped_vector.h"
#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkFontStyleSet_cobalt.h"
#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkFontUtil_cobalt.h"
#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkStream_cobalt.h"
#include "SkFontMgr.h"
#include "SkTArray.h"
#include "SkTypeface.h"

// This class, which is thread-safe, is Cobalt's implementation of SkFontMgr. It
// is responsible for the creation of remote typefaces and for, given a set of
// constraints, providing Cobalt with its best matching local typeface.
//
// When a remote typeface's raw data is downloaded from the web, it is provided
// to the font manager, which returns a typeface created from that data.
//
// For local typefaces, the font manager uses Cobalt-specific and
// system-specific configuration files to generate named font families and
// fallback font families. Cobalt uses named families when it needs a typeface
// for a family with a specific name. It utilizes fallback families when none
// of the listed named families supported a given character. In that case,
// the manager finds the best match among the fallback families that can provide
// a glyph for that character.
//
// For both named families and fallback families, after the manager locates the
// best matching family, the determination of the specific typeface to use is
// left to the family. The manager provides the family with the requested style
// and the family returns the typeface that best fits that style.
class SkFontMgr_Cobalt : public SkFontMgr {
 public:
  typedef std::vector<SkFontStyleSet_Cobalt*> StyleSetArray;
  typedef std::map<int, StyleSetArray> PriorityStyleSetArrayMap;

  SkFontMgr_Cobalt(const char* cobalt_font_config_directory,
                   const char* cobalt_font_files_directory,
                   const char* system_font_config_directory,
                   const char* system_font_files_directory,
                   const SkTArray<SkString, true>& default_fonts);

  // Purges all font caching in Skia and the local stream manager.
  void PurgeCaches();

  // NOTE: This returns NULL if a match is not found.
  SkTypeface* MatchFaceName(const char face_name[]);

 protected:
  // From SkFontMgr
  int onCountFamilies() const override;

  void onGetFamilyName(int index, SkString* family_name) const override;

  // NOTE: This returns NULL if there is no accessible style set at the index.
  SkFontStyleSet_Cobalt* onCreateStyleSet(int index) const override;

  // NOTE: This returns NULL if there is no family match.
  SkFontStyleSet_Cobalt* onMatchFamily(const char family_name[]) const override;

  // NOTE: This always returns a non-NULL value. If the family name cannot be
  // found, then the best match among the default family is returned.
  SkTypeface* onMatchFamilyStyle(const char family_name[],
                                 const SkFontStyle& style) const override;

  // NOTE: This always returns a non-NULL value. If no match can be found, then
  // the best match among the default family is returned.
  SkTypeface* onMatchFamilyStyleCharacter(const char family_name[],
                                          const SkFontStyle& style,
                                          const char* bcp47[], int bcp47_count,
                                          SkUnichar character) const override;

  // NOTE: This returns NULL if a match is not found.
  SkTypeface* onMatchFaceStyle(const SkTypeface* family_member,
                               const SkFontStyle& font_style) const override;

  // NOTE: This returns NULL if the typeface cannot be created.
  SkTypeface* onCreateFromData(SkData* data, int face_index) const override;

  // NOTE: This returns NULL if the typeface cannot be created.
  SkTypeface* onCreateFromStream(SkStreamAsset* stream,
                                 int face_index) const override;

  // NOTE: This returns NULL if the typeface cannot be created.
  SkTypeface* onCreateFromFile(const char path[],
                               int face_index) const override;

  // NOTE: This always returns a non-NULL value. If no match can be found, then
  // the best match among the default family is returned.
  SkTypeface* onLegacyCreateTypeface(const char family_name[],
                                     SkFontStyle style) const override;

 private:
  typedef base::hash_map<std::string, SkFontStyleSet_Cobalt*> NameToStyleSetMap;
  typedef base::SmallMap<base::hash_map<std::string, StyleSetArray*> >
      NameToStyleSetArrayMap;

  void ParseConfigAndBuildFamilies(
      const char* font_config_directory, const char* font_files_directory,
      PriorityStyleSetArrayMap* priority_fallback_families);
  void BuildNameToFamilyMap(
      const char* font_files_directory,
      SkTDArray<FontFamilyInfo*>* config_font_families,
      PriorityStyleSetArrayMap* priority_fallback_families);
  void GeneratePriorityOrderedFallbackFamilies(
      const PriorityStyleSetArrayMap& priority_fallback_families);
  void FindDefaultFamily(const SkTArray<SkString, true>& default_families);

  // Returns the first encountered fallback family that matches the language tag
  // and supports the specified character.
  // NOTE: |style_sets_mutex_| should be locked prior to calling this function.
  SkTypeface* FindFamilyStyleCharacter(const SkFontStyle& style,
                                       const SkString& language_tag,
                                       SkUnichar character);

  // Returns every fallback family that matches the language tag. If the tag is
  // empty, then all fallback families are returned.
  // NOTE: |style_sets_mutex_| should be locked prior to calling this function.
  StyleSetArray* GetMatchingFallbackFamilies(const SkString& language_tag);

  SkFileMemoryChunkStreamManager local_typeface_stream_manager_;

  SkTArray<sk_sp<SkFontStyleSet_Cobalt>, true> families_;

  SkTArray<SkString> family_names_;
  // Map names to the back end so that all names for a given family refer to
  // the same (non-replicated) set of typefaces.
  NameToStyleSetMap name_to_family_map_;
  NameToStyleSetMap full_font_name_to_family_map_;
  NameToStyleSetMap font_postscript_name_to_family_map_;

  // Fallback families that are used during character fallback.
  // All fallback families, regardless of language.
  StyleSetArray fallback_families_;
  // Language-specific fallback families. These are lazily populated from
  // |fallback_families_| when a new language tag is requested.
  ScopedVector<StyleSetArray> language_fallback_families_array_;
  NameToStyleSetArrayMap language_fallback_families_map_;

  // The default family that is used when no specific match is found during a
  // request.
  SkFontStyleSet_Cobalt* default_family_;

  // Mutex shared by all families for accessing their modifiable data.
  mutable SkMutex family_mutex_;
};

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKFONTMGR_COBALT_H_
