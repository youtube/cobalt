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

#include <sys/stat.h>
#include <cmath>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "base/memory/singleton.h"
#include "cobalt/base/c_val.h"
#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkFontConfigParser_cobalt.h"
#include "SkData.h"
#include "SkGraphics.h"
#include "SkOSFile.h"
#include "SkStream.h"
#include "SkString.h"
#include "SkTArray.h"
#include "SkTSearch.h"
#include "third_party/skia/src/ports/SkFontHost_FreeType_common.h"

///////////////////////////////////////////////////////////////////////////////

namespace {

#ifndef SK_TYPEFACE_COBALT_SYSTEM_OPEN_STREAM_CACHE_LIMIT
#define SK_TYPEFACE_COBALT_SYSTEM_OPEN_STREAM_CACHE_LIMIT (10 * 1024 * 1024)
#endif

const int32_t kPeriodicProcessingTimerDelayMs = 5000;
const int32_t kReleaseInactiveSystemTypefaceOpenStreamsDelayMs = 300000;
const int32_t kPeriodicFontCachePurgeDelayMs = 900000;

// base::CVal tracking specific to fonts.
// NOTE: These are put in their own Singleton, because Skia's lazy instance
// implementation does not guarantee that only one SkFontMgr_Cobalt will be
// created at a time (see skia/src/core/SkLazyPtr.h), and we cannot have
// multiple copies of the same CVal.
class SkFontMgrCVals {
 public:
  static SkFontMgrCVals* GetInstance() {
    return Singleton<SkFontMgrCVals,
                     DefaultSingletonTraits<SkFontMgrCVals> >::get();
  }

  const base::CVal<base::cval::SizeInBytes, base::CValPublic>&
  system_typeface_open_stream_cache_limit_in_bytes() {
    return system_typeface_open_stream_cache_limit_in_bytes_;
  }

  base::CVal<base::cval::SizeInBytes, base::CValPublic>&
  system_typeface_open_stream_cache_size_in_bytes() {
    return system_typeface_open_stream_cache_size_in_bytes_;
  }

  void IncrementFontFilesLoadedCount() { ++font_files_loaded_count_; }

 private:
  SkFontMgrCVals()
      : system_typeface_open_stream_cache_limit_in_bytes_(
            "Memory.Font.SystemTypeface.Capacity",
            SK_TYPEFACE_COBALT_SYSTEM_OPEN_STREAM_CACHE_LIMIT,
            "The capacity, in bytes, of the system fonts. Exceeding this "
            "results in *inactive* fonts being released."),
        system_typeface_open_stream_cache_size_in_bytes_(
            "Memory.Font.SystemTypeface.Size", 0,
            "Total number of bytes currently used by the cache."),
        font_files_loaded_count_(
            "Count.Font.FilesLoaded", 0,
            "Total number of font file loads that have occurred.") {}

  const base::CVal<base::cval::SizeInBytes, base::CValPublic>
      system_typeface_open_stream_cache_limit_in_bytes_;
  mutable base::CVal<base::cval::SizeInBytes, base::CValPublic>
      system_typeface_open_stream_cache_size_in_bytes_;

  base::CVal<int> font_files_loaded_count_;

  friend struct DefaultSingletonTraits<SkFontMgrCVals>;
  DISALLOW_COPY_AND_ASSIGN(SkFontMgrCVals);
};

// The timer used to trigger periodic processing of open streams, which
// handles moving open streams from the active to the inactive list and
// releasing inactive open streams. Create it as a lazy instance to ensure it
// gets cleaned up correctly at exit, even though the SkFontMgr_Cobalt may be
// created multiple times and the surviving instance isn't deleted until after
// the thread has been stopped.
base::LazyInstance<scoped_ptr<base::PollerWithThread> >
    process_system_typefaces_with_open_streams_poller =
        LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<base::Lock> poller_lock = LAZY_INSTANCE_INITIALIZER;

// NOTE: It is the responsibility of the caller to call Unref() on the SkData.
SkData* NewDataFromFile(const SkString& file_path) {
  TRACE_EVENT1("cobalt::renderer", "SkFontMgr_cobalt::NewDataFromFile()",
               "file_path", TRACE_STR_COPY(file_path.c_str()));
  LOG(INFO) << "Loading font file: " << file_path.c_str();
  SkFontMgrCVals::GetInstance()->IncrementFontFilesLoadedCount();

  SkAutoTUnref<SkStreamAsset> file_stream(
      SkStream::NewFromFile(file_path.c_str()));
  if (file_stream == NULL) {
    LOG(ERROR) << "Failed to open font file: " << file_path.c_str();
    return NULL;
  }

  SkAutoDataUnref data(
      SkData::NewFromStream(file_stream, file_stream->getLength()));
  if (data == NULL) {
    LOG(ERROR) << "Failed to read font file: " << file_path.c_str();
    return NULL;
  }

  return data.detach();
}

int match_score(const SkFontStyle& pattern, const SkFontStyle& candidate) {
  int score = 0;
  score += std::abs((pattern.width() - candidate.width()) * 100);
  score += (pattern.isItalic() == candidate.isItalic()) ? 0 : 1000;
  score += std::abs(pattern.weight() - candidate.weight());
  return score;
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////

class SkTypeface_Cobalt : public SkTypeface_FreeType {
 public:
  SkTypeface_Cobalt(int index, Style style, bool is_fixed_pitch,
                    const SkString family_name)
      : INHERITED(style, SkTypefaceCache::NewFontID(), is_fixed_pitch),
        index_(index),
        family_name_(family_name) {}

 protected:
  virtual void onGetFamilyName(SkString* family_name) const SK_OVERRIDE {
    *family_name = family_name_;
  }

  int index_;
  SkString family_name_;

 private:
  typedef SkTypeface_FreeType INHERITED;
};

class SkTypeface_CobaltStream : public SkTypeface_Cobalt {
 public:
  SkTypeface_CobaltStream(SkStreamAsset* stream, int index, Style style,
                          bool is_fixed_pitch, const SkString family_name)
      : INHERITED(index, style, is_fixed_pitch, family_name),
        stream_(SkRef(stream)) {
    LOG(INFO) << "Created SkTypeface_CobaltStream: " << family_name.c_str()
              << "(" << style << "); Size: " << stream_->getLength()
              << " bytes";
  }

  virtual void onGetFontDescriptor(SkFontDescriptor* descriptor,
                                   bool* serialize) const SK_OVERRIDE {
    SkASSERT(descriptor);
    SkASSERT(serialize);
    descriptor->setFamilyName(family_name_.c_str());
    descriptor->setFontFileName(NULL);
    descriptor->setFontIndex(index_);
    *serialize = true;
  }

  virtual SkStreamAsset* onOpenStream(int* ttc_index) const SK_OVERRIDE {
    *ttc_index = index_;
    return stream_->duplicate();
  }

 private:
  typedef SkTypeface_Cobalt INHERITED;

  SkAutoTUnref<SkStreamAsset> stream_;
};

class SkTypeface_CobaltSystem : public SkTypeface_Cobalt {
 public:
  SkTypeface_CobaltSystem(const SkFontMgr_Cobalt* manager, SkString path_name,
                          SkMemoryStream* stream, int index, Style style,
                          bool is_fixed_pitch, const SkString family_name)
      : INHERITED(index, style, is_fixed_pitch, family_name),
        font_manager_(manager),
        path_name_(path_name),
        stream_(SkRef(stream)) {
    LOG(INFO) << "Created SkTypeface_CobaltSystem: " << family_name.c_str()
              << "(" << style << "); File: " << path_name.c_str()
              << "; Size: " << stream_->getLength() << " bytes";

    SkAutoMutexAcquire scoped_mutex(
        font_manager_->GetSystemTypefaceStreamMutex());
    font_manager_->AddSystemTypefaceWithActiveOpenStream(this);
  }

  virtual void onGetFontDescriptor(SkFontDescriptor* descriptor,
                                   bool* serialize) const SK_OVERRIDE {
    SkASSERT(descriptor);
    SkASSERT(serialize);
    descriptor->setFamilyName(family_name_.c_str());
    descriptor->setFontFileName(path_name_.c_str());
    descriptor->setFontIndex(index_);
    *serialize = false;
  }

  virtual SkStreamAsset* onOpenStream(int* ttc_index) const SK_OVERRIDE {
    TRACE_EVENT0("cobalt::renderer", "SkTypeface_CobaltSystem::onOpenStream()");
    *ttc_index = index_;

    // Scope the initial mutex lock.
    {
      // Check for the stream still being open. In this case, the typeface
      // stream does not need to be re-loaded. Simply notify the manager of the
      // stream activity, as it potentially needs to move the stream from the
      // inactive list to the active list.
      SkAutoMutexAcquire scoped_mutex(
          font_manager_->GetSystemTypefaceStreamMutex());
      if (stream_ != NULL) {
        font_manager_->NotifySystemTypefaceOfOpenStreamActivity(this);
        return stream_->duplicate();
      }
    }

    TRACE_EVENT0("cobalt::renderer", "Load data from file");
    // This is where the bulk of the time will be spent, so load the font data
    // outside of a mutex lock.
    SkAutoTUnref<SkData> data(NewDataFromFile(path_name_));

    // Re-lock the mutex now that the data has been loaded.
    SkAutoMutexAcquire scoped_mutex(
        font_manager_->GetSystemTypefaceStreamMutex());

    // Verify that the data was not loaded on another thread while it was being
    // loaded on this one. If that's the case, then simply use that data.
    // Otherwise, update the stream and notify the font manager that the stream
    // is open.
    if (stream_ == NULL) {
      stream_.reset(new SkMemoryStream(data));
      LOG(INFO) << "Setting font stream: " << path_name_.c_str()
                << "; Size: " << stream_->getLength() << " bytes";
      font_manager_->AddSystemTypefaceWithActiveOpenStream(this);
    }

    return stream_->duplicate();
  }

  size_t GetOpenStreamSizeInBytes() const { return stream_->getLength(); }

  bool IsOpenStreamInactive() const {
    SkAutoTUnref<SkData> data(stream_->copyToData());
    // A refcount of 2 indicates that there are no external references to the
    // data. The first reference comes from SkMemoryStream and the second comes
    // from the call to copyToData().
    return data->getRefCnt() == 2;
  }

  void ReleaseStream() const {
    LOG(INFO) << "Releasing font stream: " << path_name_.c_str()
              << "; Size: " << stream_->getLength() << " bytes";
    stream_.reset(NULL);
  }

 private:
  typedef SkTypeface_Cobalt INHERITED;

  const SkFontMgr_Cobalt* font_manager_;
  const SkString path_name_;

  // |stream_| is mutable so that it can be modified within onOpenStream().
  mutable SkAutoTUnref<SkMemoryStream> stream_;
};

///////////////////////////////////////////////////////////////////////////////
//  SkFontStyleSet_Cobalt
//
//  This class is used by SkFontMgr_Cobalt to hold SkTypeface_CobaltSystem
//  families.
SkFontStyleSet_Cobalt::SkFontStyleSet_Cobalt(
    const SkFontMgr_Cobalt* const manager, const FontFamily& family,
    const char* base_path, SkMutex* const manager_owned_mutex)
    : font_manager_(manager),
      manager_owned_mutex_(manager_owned_mutex),
      is_fallback_family_(family.is_fallback_family),
      language_(family.language),
      page_ranges_(family.page_ranges),
      is_character_map_generated_(false) {
  TRACE_EVENT0("cobalt::renderer",
               "SkFontStyleSet_Cobalt::SkFontStyleSet_Cobalt()");
  DCHECK(manager_owned_mutex_);

  if (family.names.count() == 0) {
    return;
  }

  family_name_ = family.names[0];

  for (int i = 0; i < family.fonts.count(); ++i) {
    const FontFileInfo& font_file = family.fonts[i];

    SkString path_name(SkOSPath::Join(base_path, font_file.file_name.c_str()));

    // Sanity check that something exists at this location.
    if (!sk_exists(path_name.c_str(), kRead_SkFILE_Flag)) {
      LOG(ERROR) << "Failed to find font file: " << path_name.c_str();
      continue;
    }

    SkFontStyle style(font_file.weight, SkFontStyle::kNormal_Width,
                      font_file.style == FontFileInfo::kItalic_FontStyle
                          ? SkFontStyle::kItalic_Slant
                          : SkFontStyle::kUpright_Slant);

    std::string full_font_name;
    if (!font_file.full_font_name.isEmpty()) {
      full_font_name = std::string(font_file.full_font_name.c_str(),
                                   font_file.full_font_name.size());
    }
    std::string postscript_name;
    if (!font_file.postscript_name.isEmpty()) {
      postscript_name = std::string(font_file.postscript_name.c_str(),
                                    font_file.postscript_name.size());
    }

    styles_.push_back().reset(SkNEW_ARGS(
        SkFontStyleSetEntry_Cobalt,
        (path_name, font_file.index, style, full_font_name, postscript_name)));
  }
}

int SkFontStyleSet_Cobalt::count() {
  SkAutoMutexAcquire scoped_mutex(*manager_owned_mutex_);
  return styles_.count();
}

void SkFontStyleSet_Cobalt::getStyle(int index, SkFontStyle* style,
                                     SkString* name) {
  // SkFontStyleSet_Cobalt does not support publicly interacting with entries
  // via index, as entries can potentially be removed, thereby invalidating the
  // indices.
  NOTREACHED();
}

SkTypeface* SkFontStyleSet_Cobalt::createTypeface(int index) {
  // SkFontStyleSet_Cobalt does not support publicly interacting with entries
  // via index, as entries can potentially be removed, thereby invalidating the
  // indices.
  NOTREACHED();
  return NULL;
}

SkTypeface* SkFontStyleSet_Cobalt::matchStyle(const SkFontStyle& pattern) {
  SkAutoMutexAcquire scoped_mutex(*manager_owned_mutex_);
  return MatchStyleWithoutLocking(pattern);
}

SkTypeface* SkFontStyleSet_Cobalt::MatchStyleWithoutLocking(
    const SkFontStyle& pattern) {
  SkTypeface* typeface = NULL;
  while (typeface == NULL && styles_.count() > 0) {
    typeface = TryRetrieveTypefaceAndRemoveStyleOnFailure(
        GetClosestStyleIndex(pattern));
  }
  return typeface;
}

SkTypeface* SkFontStyleSet_Cobalt::MatchFullFontName(const std::string& name) {
  for (int i = 0; i < styles_.count(); ++i) {
    if (styles_[i]->full_font_name == name) {
      return TryRetrieveTypefaceAndRemoveStyleOnFailure(i);
    }
  }
  return NULL;
}

SkTypeface* SkFontStyleSet_Cobalt::MatchFontPostScriptName(
    const std::string& name) {
  for (int i = 0; i < styles_.count(); ++i) {
    if (styles_[i]->font_postscript_name == name) {
      return TryRetrieveTypefaceAndRemoveStyleOnFailure(i);
    }
  }
  return NULL;
}

SkTypeface* SkFontStyleSet_Cobalt::TryRetrieveTypefaceAndRemoveStyleOnFailure(
    int style_index) {
  DCHECK(style_index >= 0 && style_index < styles_.count());
  SkFontStyleSetEntry_Cobalt* style = styles_[style_index];
  // If the typeface doesn't already exist, then attempt to create it.
  if (style->typeface == NULL) {
    CreateSystemTypeface(style);
    // If the creation attempt failed and the typeface is still NULL, then
    // remove the entry from the set's styles.
    if (style->typeface == NULL) {
      styles_[style_index].swap(&styles_.back());
      styles_.pop_back();
      return NULL;
    }
  }
  return SkRef(style->typeface.get());
}

bool SkFontStyleSet_Cobalt::ContainsTypeface(const SkTypeface* typeface) {
  for (int i = 0; i < styles_.count(); ++i) {
    if (styles_[i]->typeface == typeface) {
      return true;
    }
  }
  return false;
}

bool SkFontStyleSet_Cobalt::ContainsCharacter(const SkFontStyle& style,
                                              SkUnichar character) {
  // If page ranges exist for this style set, then verify that this character
  // falls within the ranges. Otherwise, the style set is treated as having a
  // page range containing all characters.
  size_t num_ranges = page_ranges_.count();
  if (num_ranges > 0) {
    int16 page = font_character_map::GetPage(character);

    // Verify that the last page range is not less than the page containing the
    // character. If it's less, then this character can't be contained
    // within the pages. Otherwise, this last range acts as a sentinel for the
    // search.
    if (page_ranges_[num_ranges - 1].second < page) {
      return false;
    }

    int range_index = 0;
    while (page_ranges_[range_index].second < page) {
      ++range_index;
    }

    if (page_ranges_[range_index].first > page) {
      return false;
    }
  }

  // If this point is reached, then the character is contained within one of
  // the font style set's page ranges. Now, verify that the specific character
  // is supported by the font style set.

  // The character map is lazily generated. Generate it now if it isn't already
  // generated.
  if (!is_character_map_generated_) {
    TRACE_EVENT0("cobalt::renderer",
                 "SkFontStyleSet_Cobalt::ContainsCharacter() and "
                 "!is_character_map_generated_");
    // Attempt to load the closest font style from the set. If it fails to load,
    // it will be removed from the set and, as long as font styles remain in the
    // set, the logic will be attempted again.
    while (styles_.count() > 0) {
      int style_index = GetClosestStyleIndex(style);
      SkFontStyleSetEntry_Cobalt* closest_style = styles_[style_index];

      // Load the font data from the file and use it to generate the character
      // map.
      SkAutoTUnref<SkData> font_data(
          NewDataFromFile(closest_style->font_file_path));
      // The font failed to load. Remove it from the styles and start the loop
      // over.
      if (font_data == NULL) {
        styles_[style_index].swap(&styles_.back());
        styles_.pop_back();
        continue;
      }

      GenerateCharacterMapFromData(font_data, closest_style->ttc_index);

      // If the character is contained within the font style set, then go ahead
      // and create the typeface with the loaded font data. Otherwise, creating
      // the typeface would require an additional load of the file.
      bool contains_character = CharacterMapContainsCharacter(character);
      if (contains_character) {
        CreateSystemTypefaceFromData(closest_style, font_data);
      }
      return contains_character;
    }

    // If this point is reached, none of the fonts were successfully loaded.
    is_character_map_generated_ = true;
    return false;
  } else {
    return CharacterMapContainsCharacter(character);
  }
}

bool SkFontStyleSet_Cobalt::CharacterMapContainsCharacter(SkUnichar character) {
  font_character_map::CharacterMap::iterator page_iterator =
      character_map_.find(font_character_map::GetPage(character));
  return page_iterator != character_map_.end() &&
         page_iterator->second.test(
             font_character_map::GetPageCharacterIndex(character));
}

void SkFontStyleSet_Cobalt::GenerateCharacterMapFromData(SkData* font_data,
                                                         int ttc_index) {
  if (is_character_map_generated_) {
    return;
  }
  TRACE_EVENT0("cobalt::renderer", "GenerateCharacterMapFromData()");

  FT_Library freetype_lib;
  if (FT_Init_FreeType(&freetype_lib) != 0) {
    return;
  }

  FT_Face face;
  FT_Error err = FT_New_Memory_Face(freetype_lib, font_data->bytes(),
                                    font_data->size(), ttc_index, &face);
  if (!err) {
    // map out this font's characters.
    FT_UInt glyph_index;

    int last_page = -1;
    font_character_map::PageCharacters* page_characters = NULL;

    SkUnichar code_point = FT_Get_First_Char(face, &glyph_index);
    while (glyph_index) {
      int page = font_character_map::GetPage(code_point);
      if (page != last_page) {
        page_characters = &character_map_[page];
        last_page = page;
      }
      page_characters->set(
          font_character_map::GetPageCharacterIndex(code_point));

      code_point = FT_Get_Next_Char(face, code_point, &glyph_index);
    }

    // release this font.
    FT_Done_Face(face);
  }

  // shut down FreeType.
  FT_Done_FreeType(freetype_lib);

  is_character_map_generated_ = true;
}

int SkFontStyleSet_Cobalt::GetClosestStyleIndex(const SkFontStyle& pattern) {
  int closest_index = 0;
  int min_score = std::numeric_limits<int>::max();
  for (int i = 0; i < styles_.count(); ++i) {
    int score = match_score(pattern, styles_[i]->font_style);
    if (score < min_score) {
      closest_index = i;
      min_score = score;
    }
  }
  return closest_index;
}

void SkFontStyleSet_Cobalt::CreateSystemTypeface(
    SkFontStyleSetEntry_Cobalt* style_entry) {
  TRACE_EVENT0("cobalt::renderer",
               "SkFontStyleSet_Cobalt::CreateSystemTypeface()");
  SkAutoTUnref<SkData> font_data(NewDataFromFile(style_entry->font_file_path));
  if (font_data != NULL) {
    CreateSystemTypefaceFromData(style_entry, font_data);
  }
}

void SkFontStyleSet_Cobalt::CreateSystemTypefaceFromData(
    SkFontStyleSetEntry_Cobalt* style_entry, SkData* data) {
  TRACE_EVENT0("cobalt::renderer",
               "SkFontStyleSet_Cobalt::CreateSystemTypefaceFromData()");
  DCHECK(!style_entry->typeface);

  // Since the font data is available, generate the character map if this is a
  // fallback font (which means that it'll need the character map for character
  // lookups during fallback).
  if (is_fallback_family_) {
    GenerateCharacterMapFromData(data, style_entry->ttc_index);
  }

  // Pass the SkData into the SkMemoryStream.
  SkAutoTUnref<SkMemoryStream> stream(new SkMemoryStream(data));

  bool is_fixed_pitch;
  SkTypeface::Style style;
  SkString name;
  if (SkTypeface_FreeType::ScanFont(stream, style_entry->ttc_index, &name,
                                    &style, &is_fixed_pitch)) {
    LOG(INFO) << "Scanned font: " << name.c_str() << "(" << style << ")";
    style_entry->typeface.reset(SkNEW_ARGS(
        SkTypeface_CobaltSystem,
        (font_manager_, style_entry->font_file_path, stream,
         style_entry->ttc_index, style, is_fixed_pitch, family_name_)));
  } else {
    LOG(ERROR) << "Failed to scan font: "
               << style_entry->font_file_path.c_str();
  }
}

///////////////////////////////////////////////////////////////////////////////
//
//  SkFontMgr_Cobalt
//

SkFontMgr_Cobalt::SkFontMgr_Cobalt(
    const char* directory, const SkTArray<SkString, true>& default_fonts)
    : default_family_(NULL),
      last_font_cache_purge_time_(base::TimeTicks::Now()) {
  TRACE_EVENT0("cobalt::renderer", "SkFontMgr_Cobalt::SkFontMgr_Cobalt()");
  // Ensure that both the CValManager and SkFontMgrCVals are initialized. The
  // CValManager is created first as it must outlast the SkFontMgrCVals.
  base::CValManager::GetInstance();
  SkFontMgrCVals::GetInstance();

  SkTDArray<FontFamily*> font_families;
  {
    TRACE_EVENT0("cobalt::renderer", "SkFontConfigParser::GetFontFamilies()");
    SkFontConfigParser::GetFontFamilies(directory, &font_families);
  }
  BuildNameToFamilyMap(directory, &font_families);
  font_families.deleteAll();

  FindDefaultFont(default_fonts);

  {
    // Create the poller. The lazy instance ensures we only get one copy, and
    // that it will be cleaned up at appl exit, but we still need to lock
    // so we only construct it once.
    base::AutoLock lock(poller_lock.Get());
    if (!process_system_typefaces_with_open_streams_poller.Get()) {
      process_system_typefaces_with_open_streams_poller.Get().reset(
          new base::PollerWithThread(
              base::Bind(&SkFontMgr_Cobalt::HandlePeriodicProcessing,
                         base::Unretained(this)),
              base::TimeDelta::FromMilliseconds(
                  kPeriodicProcessingTimerDelayMs)));
    }
  }
}

SkTypeface* SkFontMgr_Cobalt::matchFaceName(const std::string& font_face_name) {
  if (font_face_name.empty()) {
    return NULL;
  }

  // Lock the style sets mutex prior to accessing them.
  SkAutoMutexAcquire scoped_mutex(style_sets_mutex_);

  // Prioritize looking up the postscript name first since some of our client
  // applications prefer this method to specify face names.
  for (int i = 0; i <= 1; ++i) {
    NameToStyleSetMap& name_to_style_set_map =
        i == 0 ? font_postscript_name_to_style_set_map_
               : full_font_name_to_style_set_map_;

    NameToStyleSetMap::iterator style_set_iterator =
        name_to_style_set_map.find(font_face_name);
    if (style_set_iterator != name_to_style_set_map.end()) {
      SkFontStyleSet_Cobalt* style_set = style_set_iterator->second;
      SkTypeface* typeface =
          i == 0 ? style_set->MatchFontPostScriptName(font_face_name)
                 : style_set->MatchFullFontName(font_face_name);
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

  SkAutoAsciiToLC tolc(family_name);

  NameToStyleSetMap::const_iterator family_iterator =
      name_to_family_map_.find(tolc.lc());
  if (family_iterator != name_to_family_map_.end()) {
    return SkRef(family_iterator->second);
  }

  return NULL;
}

SkTypeface* SkFontMgr_Cobalt::onMatchFamilyStyle(
    const char family_name[], const SkFontStyle& style) const {
  SkTypeface* tf = NULL;

  if (family_name) {
    SkAutoTUnref<SkFontStyleSet> sset(matchFamily(family_name));
    tf = sset->matchStyle(style);
  }

  if (NULL == tf) {
    tf = default_family_->matchStyle(style);
  }

  return tf;
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

#ifdef SK_FM_NEW_MATCH_FAMILY_STYLE_CHARACTER
SkTypeface* SkFontMgr_Cobalt::onMatchFamilyStyleCharacter(
    const char family_name[], const SkFontStyle& style, const char* bcp47[],
    int bcp47_count, SkUnichar character) const {
#else
SkTypeface* SkFontMgr_Cobalt::onMatchFamilyStyleCharacter(
    const char family_name[], const SkFontStyle& style, const char bcp47_val[],
    SkUnichar character) const {
  const char** bcp47 = &bcp47_val;
  int bcp47_count = bcp47_val ? 1 : 0;
#endif

  // Lock the style sets mutex prior to calling |FindFamilyStyleCharacter|. It
  // expects the mutex to already be locked.
  SkAutoMutexAcquire scoped_mutex(style_sets_mutex_);

  // Search the fallback families for ones matching the requested language.
  // They are given priority over other fallback families in checking for
  // character support.
  for (int bcp47_index = bcp47_count; bcp47_index-- > 0;) {
    SkLanguage language(bcp47[bcp47_index]);
    while (!language.GetTag().isEmpty()) {
      SkTypeface* matching_typeface =
          FindFamilyStyleCharacter(style, language.GetTag(), character);
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
      FindFamilyStyleCharacter(style, SkString(), character);

  // If no family was found that supports the character, then just fall back
  // to the default family.
  return matching_typeface ? matching_typeface
                           : default_family_->MatchStyleWithoutLocking(style);
}

SkTypeface* SkFontMgr_Cobalt::onCreateFromData(SkData* data,
                                               int ttc_index) const {
  SkAutoTUnref<SkStreamAsset> stream(new SkMemoryStream(data));
  return createFromStream(stream, ttc_index);
}

SkTypeface* SkFontMgr_Cobalt::onCreateFromStream(SkStreamAsset* stream,
                                                 int ttc_index) const {
  TRACE_EVENT0("cobalt::renderer", "SkFontMgr_Cobalt::onCreateFromStream()");
  bool is_fixed_pitch;
  SkTypeface::Style style;
  SkString name;
  if (!SkTypeface_FreeType::ScanFont(stream, ttc_index, &name, &style,
                                     &is_fixed_pitch)) {
    return NULL;
  }
  return SkNEW_ARGS(SkTypeface_CobaltStream,
                    (stream, ttc_index, style, is_fixed_pitch, name));
}

SkTypeface* SkFontMgr_Cobalt::onCreateFromFile(const char path[],
                                               int ttc_index) const {
  TRACE_EVENT0("cobalt::renderer", "SkFontMgr_Cobalt::onCreateFromFile()");
  SkAutoTUnref<SkStreamAsset> stream(SkStream::NewFromFile(path));
  return stream.get() ? createFromStream(stream, ttc_index) : NULL;
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

void SkFontMgr_Cobalt::BuildNameToFamilyMap(const char* base_path,
                                            SkTDArray<FontFamily*>* families) {
  TRACE_EVENT0("cobalt::renderer", "SkFontMgr_Cobalt::BuildNameToFamilyMap()");
  for (int i = 0; i < families->count(); i++) {
    FontFamily& family = *(*families)[i];
    bool named_family = family.names.count() > 0;

    if (family.is_fallback_family) {
      if (!named_family) {
        SkString& fallback_name = family.names.push_back();
        fallback_name.printf("%.2x##fallback", i);
      }
    }

    SkAutoTUnref<SkFontStyleSet_Cobalt> new_set(SkNEW_ARGS(
        SkFontStyleSet_Cobalt, (this, family, base_path, &style_sets_mutex_)));

    // Verify that the set successfully added entries.
    if (new_set->styles_.count() == 0) {
      continue;
    }

    font_style_sets_.push_back().reset(SkRef(new_set.get()));

    if (named_family) {
      for (int j = 0; j < family.names.count(); j++) {
        family_names_.push_back(family.names[j]);
        name_to_family_map_.insert(
            std::make_pair(family.names[j].c_str(), new_set.get()));
      }
    }

    if (family.is_fallback_family) {
      fallback_families_.push_back(new_set.get());
    }

    // Handle adding the font face names for the style set entries. These are
    // optional, so many may not have them.
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

        if (font_face_name_style_set_map.find(font_face_name) ==
            font_face_name_style_set_map.end()) {
          font_face_name_style_set_map[font_face_name] = new_set.get();
        } else {
          // Purposely, not overwriting the entry gives priority to the
          // earlier entry.  This is consistent with how fonts.xml gives
          // priority to fonts that are specified earlier in the file.
          const std::string font_face_name_type =
              i == 0 ? "Full Font" : "Postscript";
          NOTREACHED() << font_face_name_type << " name [" << font_face_name
                       << "] already registered in BuildNameToFamilyMap.";
        }
      }
    }
  }
}

void SkFontMgr_Cobalt::FindDefaultFont(
    const SkTArray<SkString, true>& default_fonts) {
  SkASSERT(!font_style_sets_.empty());

  for (size_t i = 0; i < default_fonts.count(); ++i) {
    SkAutoTUnref<SkFontStyleSet_Cobalt> set(
        onMatchFamily(default_fonts[i].c_str()));
    if (NULL == set) {
      continue;
    }
    SkAutoTUnref<SkTypeface> tf(set->MatchStyleWithoutLocking(SkFontStyle()));
    if (NULL == tf) {
      continue;
    }
    default_family_ = set.get();
    default_typeface_.swap(&tf);
    break;
  }

  if (NULL == default_typeface_) {
    default_family_ = font_style_sets_[0].get();
    default_typeface_.reset(
        default_family_->MatchStyleWithoutLocking(SkFontStyle()));
  }

  SkASSERT(default_family_);
  SkASSERT(default_typeface_);

  // Remove the default typeface from the tracked system typefaces with open
  // streams. We never want to release it.
  for (int i = 0; i < system_typefaces_with_active_open_streams_.count(); ++i) {
    if (system_typefaces_with_active_open_streams_[i]->uniqueID() ==
        default_typeface_->uniqueID()) {
      system_typefaces_with_active_open_streams_.removeShuffle(i);
      break;
    }
  }
}

SkTypeface* SkFontMgr_Cobalt::FindFamilyStyleCharacter(
    const SkFontStyle& style, const SkString& language_tag,
    SkUnichar character) const {
  if (!font_character_map::IsCharacterValid(character)) {
    return NULL;
  }

  for (int i = 0; i < fallback_families_.count(); ++i) {
    SkFontStyleSet_Cobalt* family = fallback_families_[i];

    // If a language tag was provided and this family doesn't meet the language
    // requirement, then just skip over the family.
    if (!language_tag.isEmpty() &&
        !family->language_.GetTag().startsWith(language_tag.c_str())) {
      continue;
    }

    if (family->ContainsCharacter(style, character)) {
      SkTypeface* matching_typeface = family->MatchStyleWithoutLocking(style);
      if (matching_typeface) {
        return matching_typeface;
      }
    }
  }

  return NULL;
}

void SkFontMgr_Cobalt::HandlePeriodicProcessing() {
  base::TimeTicks current_time = base::TimeTicks::Now();

  ProcessSystemTypefacesWithOpenStreams(current_time);

  // If the required delay has elapsed since the last font cache purge, then
  // it's time to force another. This is accomplished by setting the limit to
  // 1 byte smaller than it's current size, which initiates a partial purge (it
  // always purges at least 25% of its contents). After this is done, the cache
  // is set back to its previous size.
  if ((current_time - last_font_cache_purge_time_).InMilliseconds() >=
      kPeriodicFontCachePurgeDelayMs) {
    last_font_cache_purge_time_ = current_time;

    size_t font_cache_used = SkGraphics::GetFontCacheUsed();
    if (font_cache_used > 0) {
      size_t font_cache_limit = SkGraphics::GetFontCacheLimit();
      SkGraphics::SetFontCacheLimit(font_cache_used - 1);
      SkGraphics::SetFontCacheLimit(font_cache_limit);
    }
  }
}

// NOTE: It is the responsibility of the caller to lock
// |system_typeface_stream_mutex_|
void SkFontMgr_Cobalt::AddSystemTypefaceWithActiveOpenStream(
    const SkTypeface_CobaltSystem* system_typeface) const {
  SkFontMgrCVals::GetInstance()
      ->system_typeface_open_stream_cache_size_in_bytes() +=
      system_typeface->GetOpenStreamSizeInBytes();
  system_typefaces_with_active_open_streams_.push_back(system_typeface);

  LOG(INFO) << "Total system typeface memory: "
            << SkFontMgrCVals::GetInstance()
                   ->system_typeface_open_stream_cache_size_in_bytes()
            << " bytes";
}

// NOTE: It is the responsibility of the caller to lock
// |system_typeface_stream_mutex_|
void SkFontMgr_Cobalt::NotifySystemTypefaceOfOpenStreamActivity(
    const SkTypeface_CobaltSystem* system_typeface) const {
  // Walk through the system_typefaces with inactive open streams, searching for
  // the typeface with activity. It potentially is still in the active list and
  // won't be found here.
  for (int i = 0; i < system_typefaces_with_inactive_open_streams_.count();
       ++i) {
    // We found the typeface in the inactive list. Move it back to the active
    // list and break out. The typeface will never be in the list more than
    // once, so we don't need to continue searching.
    if (system_typefaces_with_inactive_open_streams_[i].typeface ==
        system_typeface) {
      system_typefaces_with_active_open_streams_.push_back(system_typeface);
      system_typefaces_with_inactive_open_streams_.removeShuffle(i);
      break;
    }
  }
}

void SkFontMgr_Cobalt::ProcessSystemTypefacesWithOpenStreams(
    const base::TimeTicks& current_time) const {
  SkAutoMutexAcquire scoped_mutex(system_typeface_stream_mutex_);

  // First, release the inactive open streams that meet the following
  // conditions:
  //   1. The cache is over its memory limit.
  //   2. The stream has been inactive for the required period of time.
  // Given that the streams are in temporal order, only the first stream in the
  // list ever needs to be checked. If it cannot be released, then all
  // subsequent streams are also guaranteed to not be releasable.
  while (!system_typefaces_with_inactive_open_streams_.empty()) {
    // The cache size is not over the memory limit. No open streams are released
    // while the cache is under its limit. Simply break out.
    if (SkFontMgrCVals::GetInstance()
            ->system_typeface_open_stream_cache_size_in_bytes()
            .value() < SkFontMgrCVals::GetInstance()
                           ->system_typeface_open_stream_cache_limit_in_bytes()
                           .value()) {
      break;
    }

    const TimedSystemTypeface& timed_system_typeface =
        system_typefaces_with_inactive_open_streams_[0];

    // The current stream has not been inactive long enough to be releasable. No
    // subsequent streams can meet the threshold either. Simply break out.
    if ((current_time - timed_system_typeface.time).InMilliseconds() <
        kReleaseInactiveSystemTypefaceOpenStreamsDelayMs) {
      break;
    }

    const SkTypeface_CobaltSystem* system_typeface =
        timed_system_typeface.typeface;
    DCHECK(system_typeface->IsOpenStreamInactive());

    SkFontMgrCVals::GetInstance()
        ->system_typeface_open_stream_cache_size_in_bytes() -=
        system_typeface->GetOpenStreamSizeInBytes();
    system_typeface->ReleaseStream();
    system_typefaces_with_inactive_open_streams_.removeShuffle(0);

    LOG(INFO) << "Total system typeface memory: "
              << SkFontMgrCVals::GetInstance()
                     ->system_typeface_open_stream_cache_size_in_bytes()
              << " bytes";
  }

  // Now, walk the active open streams. Any that are found to be inactive are
  // moved from the active list to the inactive list, with the current time
  // included for tracking when it is time to release the inactive stream.
  // NOTE: The active streams are processed after the inactive streams so that
  // regardless of |kReleaseInactiveSystemTypefaceOpenStreamsDelayMs|, it is
  // guaranteed that active streams cannot immediately be released upon being
  // moved to the inactive list. They must be retained in the inactive list for
  // at least |kProcessSystemTypefacesWithOpenStreamsTimerDelayMs|. If this
  // desire changes, then the active open stream processing should be moved
  // above the inactive open stream processing.
  for (int i = 0; i < system_typefaces_with_active_open_streams_.count(); ++i) {
    const SkTypeface_CobaltSystem* system_typeface =
        system_typefaces_with_active_open_streams_[i];
    if (system_typeface->IsOpenStreamInactive()) {
      system_typefaces_with_active_open_streams_.removeShuffle(i--);
      system_typefaces_with_inactive_open_streams_.push_back(
          TimedSystemTypeface(system_typeface, current_time));
    }
  }
}

SkMutex& SkFontMgr_Cobalt::GetSystemTypefaceStreamMutex() const {
  return system_typeface_stream_mutex_;
}
