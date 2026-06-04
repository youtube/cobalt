// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

// clang-format off
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
// clang-format on

#include <cmath>
#include <cstdint>
#include <limits>
#include <mutex>
#include <set>

#include "base/memory/memory_pressure_listener.h"
#include "base/task/single_thread_task_runner.h"
#include "skia/ext/font_utils.h"
#include "third_party/blink/renderer/platform/fonts/font_cache_client.h"
#include "third_party/blink/renderer/platform/fonts/font_platform_data.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"

namespace blink {

// Forward declare the thread-local purge function so our global manager can
// call it.
void PurgeStarboardFallbackCache();

namespace {

// A compact key struct that uses the stable SkFontID instead of raw pointers to
// avoid dangling keys and use-after-free conditions inside the thread-local
// fallback cache.
struct FallbackCacheKey {
  uint32_t typeface_id = 0;
  int font_size_millipixels = 0;
  bool synthetic_bold = false;
  bool synthetic_italic = false;
  uint8_t text_rendering = 0;
  uint8_t orientation = 0;

  bool IsEmpty() const { return typeface_id == 0; }
  bool IsDeleted() const {
    return typeface_id == std::numeric_limits<uint32_t>::max();
  }

  bool operator==(const FallbackCacheKey& other) const {
    return typeface_id == other.typeface_id &&
           font_size_millipixels == other.font_size_millipixels &&
           synthetic_bold == other.synthetic_bold &&
           synthetic_italic == other.synthetic_italic &&
           text_rendering == other.text_rendering &&
           orientation == other.orientation;
  }
};

// Custom WTF-compliant HashTraits for FallbackCacheKey.
// WTF::HashMap uses an open-addressing table that requires explicit empty
// and deleted sentinel slots (we use 0 for empty and MaxUint32 for deleted).
struct FallbackCacheKeyHashTraits : GenericHashTraits<FallbackCacheKey> {
  STATIC_ONLY(FallbackCacheKeyHashTraits);
  static const bool kEmptyValueIsZero = true;
  static FallbackCacheKey EmptyValue() { return FallbackCacheKey(); }
  static void ConstructDeletedValue(FallbackCacheKey& slot) {
    slot.typeface_id = std::numeric_limits<uint32_t>::max();
  }
  static bool IsDeletedValue(const FallbackCacheKey& value) {
    return value.typeface_id == std::numeric_limits<uint32_t>::max();
  }

  static unsigned GetHash(const FallbackCacheKey& key) {
    unsigned hash = key.typeface_id;
    hash = CombineHash(hash, static_cast<unsigned>(key.font_size_millipixels));
    // Pack flag bits tightly: synthetic_bold (bit 0), synthetic_italic (bit 1),
    // text_rendering (bits 2-3), orientation (bits 4-5).
    unsigned flags = (static_cast<unsigned>(key.synthetic_bold) << 0) |
                     (static_cast<unsigned>(key.synthetic_italic) << 1) |
                     (static_cast<unsigned>(key.text_rendering) << 2) |
                     (static_cast<unsigned>(key.orientation) << 4);
    hash = CombineHash(hash, flags);
    return hash;
  }

  static bool Equal(const FallbackCacheKey& a, const FallbackCacheKey& b) {
    return a == b;
  }

  static constexpr bool kSafeToCompareToEmptyOrDeleted = true;

 private:
  static unsigned CombineHash(unsigned parent, unsigned child) {
    return parent ^ (child + 0x9e3779b9 + (parent << 6) + (parent >> 2));
  }
};

typedef HashMap<FallbackCacheKey,
                Persistent<FontPlatformData>,
                FallbackCacheKeyHashTraits>
    FallbackPlatformDataCache;

// A thread-safe global manager that listens for memory pressure notifications
// and dispatches a thread-local purge task to all registered threads.
class StarboardFallbackCachePurgeManager {
 public:
  static StarboardFallbackCachePurgeManager& Get() {
    static StarboardFallbackCachePurgeManager* instance =
        new StarboardFallbackCachePurgeManager();
    return *instance;
  }

  StarboardFallbackCachePurgeManager() {
    memory_pressure_listener_ = std::make_unique<base::MemoryPressureListener>(
        FROM_HERE, base::BindRepeating(
                       &StarboardFallbackCachePurgeManager::OnMemoryPressure,
                       base::Unretained(this)));
  }

  void RegisterThread(scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    std::lock_guard<std::mutex> lock(mutex_);
    threads_.insert(task_runner);
  }

  void UnregisterThread(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    std::lock_guard<std::mutex> lock(mutex_);
    threads_.erase(task_runner);
  }

 private:
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& task_runner : threads_) {
      task_runner->PostTask(FROM_HERE,
                            base::BindOnce(&PurgeStarboardFallbackCache));
    }
  }

  std::mutex mutex_;
  std::set<scoped_refptr<base::SingleThreadTaskRunner>> threads_;
  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;
};

// A garbage-collected client that listens to Blink's global FontCache
// invalidation/purge events and clears our system fallback cache for the
// current thread context safely.
class StarboardFallbackFontCacheClient final : public FontCacheClient {
 public:
  StarboardFallbackFontCacheClient(FallbackPlatformDataCache* cache)
      : cache_(cache) {
    FontCache::Get().AddClient(this);
  }
  void FontCacheInvalidated() override {
    if (cache_) {
      cache_->clear();
    }
  }
  void Trace(Visitor* visitor) const override {
    FontCacheClient::Trace(visitor);
  }

 private:
  FallbackPlatformDataCache* cache_;
};

// A container that groups our thread-local cache map, its invalidation client,
// and a task runner. Destructor hooks on thread exit ensure we safely
// unregister the task runner to prevent memory leaks.
struct ThreadLocalFallbackCacheContainer {
  FallbackPlatformDataCache cache;
  Persistent<StarboardFallbackFontCacheClient> client;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner;

  ThreadLocalFallbackCacheContainer() {
    client = MakeGarbageCollected<StarboardFallbackFontCacheClient>(&cache);
    task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    StarboardFallbackCachePurgeManager::Get().RegisterThread(task_runner);
  }

  ~ThreadLocalFallbackCacheContainer() {
    StarboardFallbackCachePurgeManager::Get().UnregisterThread(task_runner);
  }
};

static ThreadLocalFallbackCacheContainer& GetThreadLocalContainer() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      WTF::ThreadSpecific<ThreadLocalFallbackCacheContainer>, container, ());
  return *container;
}

}  // namespace

void PurgeStarboardFallbackCache() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      WTF::ThreadSpecific<ThreadLocalFallbackCacheContainer>, container, ());
  if (container.IsSet()) {
    container->cache.clear();
  }
}

static AtomicString& MutableSystemFontFamily() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(AtomicString, system_font_family, ());
  return system_font_family;
}

// static
const AtomicString& FontCache::SystemFontFamily() {
  return MutableSystemFontFamily();
}

// static
void FontCache::SetSystemFontFamily(const AtomicString& family_name) {
  DCHECK(!family_name.empty());
  MutableSystemFontFamily() = family_name;
}

const SimpleFontData* FontCache::PlatformFallbackFontForCharacter(
    const FontDescription& font_description,
    UChar32 character,
    const SimpleFontData* font_data_to_substitute,
    FontFallbackPriority fallback_priority) {
  sk_sp<SkFontMgr> font_mgr(skia::DefaultFontMgr());
  std::string family_name = font_description.Family().FamilyName().Utf8();
  Bcp47Vector locales =
      GetBcp47LocaleForRequest(font_description, fallback_priority);
  sk_sp<SkTypeface> typeface(font_mgr->matchFamilyStyleCharacter(
      family_name.c_str(), font_description.SkiaFontStyle(), locales.data(),
      locales.size(), character));
  if (!typeface) {
    return nullptr;
  }

  bool synthetic_bold = font_description.IsSyntheticBold() &&
                        !typeface->isBold() &&
                        font_description.SyntheticBoldAllowed();
  bool synthetic_italic = font_description.IsSyntheticItalic() &&
                          !typeface->isItalic() &&
                          font_description.SyntheticItalicAllowed();

  // Normalize NaN and negative font sizes to 0.0f, and defensively clamp
  // excessive values to 100,000.0f to prevent signed integer overflow during
  // millipixels fixed-point conversion.
  float font_size = font_description.EffectiveFontSize();
  if (std::isnan(font_size) || font_size < 0.0f) {
    font_size = 0.0f;
  } else if (font_size > 100000.0f) {
    font_size = 100000.0f;
  }
  FallbackCacheKey key{typeface->uniqueID(),
                       static_cast<int>(font_size * 1000.0f),
                       synthetic_bold,
                       synthetic_italic,
                       static_cast<uint8_t>(font_description.TextRendering()),
                       static_cast<uint8_t>(font_description.Orientation())};

  // Query the thread-local member cache map to resolve the cached platform
  // data. Thread-locality guarantees thread safety and removes the need for
  // active mutex locks.
  FontPlatformData* platform_data = nullptr;
  ThreadLocalFallbackCacheContainer& container = GetThreadLocalContainer();
  auto it = container.cache.find(key);
  if (it != container.cache.end()) {
    platform_data = it->value.Get();
  } else {
    // Construct the platform data using the safe, normalized float size
    // to prevent malformed values from propagating downstream.
    auto* new_data = MakeGarbageCollected<FontPlatformData>(
        std::move(typeface), std::string(), font_size, synthetic_bold,
        synthetic_italic, font_description.TextRendering(),
        ResolvedFontFeatures(), font_description.Orientation());
    platform_data = new_data;
    container.cache.insert(key, new_data);
  }

  return FontDataFromFontPlatformData(platform_data);
}

}  // namespace blink
