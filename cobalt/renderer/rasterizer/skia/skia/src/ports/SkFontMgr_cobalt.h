/*
 * Copyright 2015 Google Inc.
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

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKFONTMGR_COBALT_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKFONTMGR_COBALT_H_

#include <bitset>
#include <limits>
#include <string>
#include <utility>

#include "base/hash_tables.h"
#include "cobalt/base/c_val.h"
#include "cobalt/base/poller.h"
#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkFontUtil_cobalt.h"
#include "SkFontHost.h"
#include "SkFontHost_FreeType_common.h"
#include "SkFontDescriptor.h"
#include "SkFontMgr.h"
#include "SkThread.h"
#include "SkTypefaceCache.h"

class SkFontMgr_Cobalt;
class SkTypeface_CobaltSystem;

//  This class is used by SkFontMgr_Cobalt to hold SkTypeface_Cobalt families.
//
//  To avoid the memory hit from keeping all fonts around, both the full
//  character map for fonts and the font faces for fonts are lazily loaded
//  when needed. After this, they are retained in memory.
//
//  The style sets contain an array of page ranges, providing information on
//  characters that are potentially contained. To determine whether or not a
//  character is actually contained, the full character map is generated.
class SkFontStyleSet_Cobalt : public SkFontStyleSet {
 public:
  struct SkFontStyleSetEntry_Cobalt : public SkRefCnt {
    SkFontStyleSetEntry_Cobalt(const SkString& file_path, const int index,
                               const SkFontStyle& style)
        : font_file_path(file_path),
          ttc_index(index),
          font_style(style),
          typeface(NULL) {}

    const SkString font_file_path;
    const int ttc_index;
    const SkFontStyle font_style;
    SkAutoTUnref<SkTypeface> typeface;
  };

  SkFontStyleSet_Cobalt(const SkFontMgr_Cobalt* const manager,
                        const FontFamily& family, const char* base_path,
                        SkMutex* const manager_owned_mutex);

  // From SkFontStyleSet
  virtual int count() SK_OVERRIDE;
  // NOTE: SkFontStyleSet_Cobalt does not support |getStyle|, as publicly
  // accessing styles by index is unsafe.
  virtual void getStyle(int index, SkFontStyle* style,
                        SkString* name) SK_OVERRIDE;
  // NOTE: SkFontStyleSet_Cobalt does not support |createTypeface|, as
  // publicly accessing styles by index is unsafe.
  virtual SkTypeface* createTypeface(int index) SK_OVERRIDE;

  virtual SkTypeface* matchStyle(const SkFontStyle& pattern) SK_OVERRIDE;

  const SkString& get_family_name() const { return family_name_; }

 private:
  // NOTE: It is the responsibility of the caller to lock the mutex before
  // calling any of the non-const private functions.

  SkTypeface* MatchStyleWithoutLocking(const SkFontStyle& pattern);
  bool ContainsTypeface(const SkTypeface* typeface);

  bool ContainsCharacter(const SkFontStyle& style, SkUnichar character);
  bool CharacterMapContainsCharacter(SkUnichar character);
  void GenerateCharacterMapFromData(SkData* font_data);

  int GetClosestStyleIndex(const SkFontStyle& pattern);
  void CreateSystemTypeface(SkFontStyleSetEntry_Cobalt* style);
  void CreateSystemTypefaceFromData(SkFontStyleSetEntry_Cobalt* style,
                                    SkData* data);

  // NOTE: The following variables can safely be accessed outside of a lock.
  const SkFontMgr_Cobalt* const font_manager_;
  SkMutex* const manager_owned_mutex_;

  SkString family_name_;
  bool is_fallback_font_;
  SkLanguage language_;
  font_character_map::PageRanges page_ranges_;

  // NOTE: The following characters require locking when being accessed.
  bool is_character_map_generated_;
  font_character_map::CharacterMap character_map_;

  SkTArray<SkAutoTUnref<SkFontStyleSetEntry_Cobalt>, true> styles_;

  friend class SkFontMgr_Cobalt;
};

//  This class is essentially a collection of SkFontStyleSet_Cobalt, one
//  SkFontStyleSet_Cobalt for each family. This class may be modified to load
//  fonts from any source by changing the initialization.
//
//  In addition to containing a collection of SkFontStyleSet_Cobalt, the class
//  also contains a mapping of names to families, allowing for multiple aliases
//  to map to the same back-end family.
//
//  It also contains a list of fallback fonts, which are used when attempting
//  to match a character, rather than a family name. If a language is provided
//  then fallback fonts with that language are given priority. Otherwise, the
//  fallback fonts are checked in order.
class SkFontMgr_Cobalt : public SkFontMgr {
 public:
  SkFontMgr_Cobalt(const char* directory,
                   const SkTArray<SkString, true>& default_fonts);

 protected:
  // From SkFontMgr
  virtual int onCountFamilies() const SK_OVERRIDE;

  virtual void onGetFamilyName(int index,
                               SkString* family_name) const SK_OVERRIDE;

  virtual SkFontStyleSet_Cobalt* onCreateStyleSet(int index) const SK_OVERRIDE;

  virtual SkFontStyleSet_Cobalt* onMatchFamily(const char family_name[]) const
      SK_OVERRIDE;

  virtual SkTypeface* onMatchFamilyStyle(
      const char family_name[], const SkFontStyle& style) const SK_OVERRIDE;

#ifdef SK_FM_NEW_MATCH_FAMILY_STYLE_CHARACTER
  virtual SkTypeface* onMatchFamilyStyleCharacter(
      const char family_name[], const SkFontStyle& style, const char* bcp47[],
      int bcp47_count, SkUnichar character) const SK_OVERRIDE;
#else
  virtual SkTypeface* onMatchFamilyStyleCharacter(
      const char family_name[], const SkFontStyle& style, const char bcp47[],
      SkUnichar character) const SK_OVERRIDE;
#endif

  virtual SkTypeface* onMatchFaceStyle(const SkTypeface* family_member,
                                       const SkFontStyle& font_style) const
      SK_OVERRIDE;

  virtual SkTypeface* onCreateFromData(SkData* data,
                                       int ttc_index) const SK_OVERRIDE;

  virtual SkTypeface* onCreateFromStream(SkStream* stream,
                                         int ttc_index) const SK_OVERRIDE;

  virtual SkTypeface* onCreateFromFile(const char path[],
                                       int ttc_index) const SK_OVERRIDE;

  virtual SkTypeface* onLegacyCreateTypeface(
      const char family_name[], unsigned style_bits) const SK_OVERRIDE;

 private:
  struct TimedSystemTypeface {
    TimedSystemTypeface(const SkTypeface_CobaltSystem* system_typeface,
                        const base::TimeTicks& event_time)
        : typeface(system_typeface), time(event_time) {}

    const SkTypeface_CobaltSystem* typeface;
    base::TimeTicks time;
  };

  //  Map names to the back end so that all names for a given family refer to
  //  the same (non-replicated) set of typefaces.
  typedef base::hash_map<std::string, SkFontStyleSet_Cobalt*> NameToFamilyMap;

  void BuildNameToFamilyMap(const char* base_path,
                            SkTDArray<FontFamily*>* families);
  void FindDefaultFont(const SkTArray<SkString, true>& default_fonts);

  // NOTE: |style_sets_mutex_| should be locked prior to calling this function.
  SkTypeface* FindFamilyStyleCharacter(const SkFontStyle& style,
                                       const SkString& lang_tag,
                                       SkUnichar character) const;

  void HandlePeriodicProcessing();

  // System typeface stream related

  // Called by SkTypeface_CobaltSystem when it opens a new stream, this adds
  // the system typeface to the active open stream list and adds the stream's
  // bytes into the open stream cache size.
  // NOTE1: This assumes that the typeface is not already in the list.
  // NOTE2: This requires the caller to lock |system_typeface_stream_mutex_|
  void AddSystemTypefaceWithActiveOpenStream(
      const SkTypeface_CobaltSystem* system_typeface) const;
  // Called by SkTypeface_CobaltSystem when an already open stream shows
  // activity, this moves the system typeface from the inactive open stream list
  // to the active open stream list.
  // NOTE: This requires the caller to lock |system_typeface_stream_mutex_|
  void NotifySystemTypefaceOfOpenStreamActivity(
      const SkTypeface_CobaltSystem* system_typeface) const;
  // Called by |system_typeface_open_stream_timer_|, this handles periodic
  // processing on the open streams, including moving typefaces from the
  // active to the inactive list and releasing inactive open streams.
  // NOTE: Handles locking |system_typeface_stream_mutex_| internally
  void ProcessSystemTypefacesWithOpenStreams(
      const base::TimeTicks& current_time) const;
  // Mutex shared by all system typefaces for accessing/modifying stream data.
  SkMutex& GetSystemTypefaceStreamMutex() const;

  SkTArray<SkAutoTUnref<SkFontStyleSet_Cobalt>, true> font_style_sets_;

  SkTArray<SkString> family_names_;
  NameToFamilyMap name_to_family_map_;

  SkTArray<SkFontStyleSet_Cobalt*> fallback_families_;

  SkFontStyleSet_Cobalt* default_family_;
  SkAutoTUnref<SkTypeface> default_typeface_;

  // System typeface stream related
  // NOTE: mutable is required on all variables potentially modified via calls
  // from SkFontStyleSet_Cobalt::onOpenStream(), which is const.

  // Tracking of active and inactive open streams among the system typefaces.
  // The streams are initially active and are moved to the inactive list when
  // the stream's underlying SkData no longer has any external references.
  // Inactive open streams are stored and released in temporal order.
  mutable SkTArray<const SkTypeface_CobaltSystem*>
      system_typefaces_with_active_open_streams_;
  mutable SkTArray<TimedSystemTypeface>
      system_typefaces_with_inactive_open_streams_;

  // This mutex is used when accessing/modifying both the system typeface stream
  // data itself and the variables used with tracking that data.
  mutable SkMutex system_typeface_stream_mutex_;

  // Mutex shared by all style sets for accessing their modifiable data.
  mutable SkMutex style_sets_mutex_;

  // The last time that a partial purge of Skia's font cache was forced. This is
  // done periodically to ensure that unused fonts are not indefinitely
  // referenced by Skia.
  base::TimeTicks last_font_cache_purge_time_;

  // SkTypeface_CobaltSystem is a friend so that it can make system typeface
  // open stream calls into SkFontMgr_Cobalt from
  // SkTypeface_CobaltSystem::onOpenStream(). Those calls should not be
  // accessible publicly.
  friend class SkTypeface_CobaltSystem;
};

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKFONTMGR_COBALT_H_
