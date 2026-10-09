// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKWOFF2FONTCACHE_COBALT_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKWOFF2FONTCACHE_COBALT_H_

#include <string>
#include <vector>

#include "base/feature_list.h"
#include "include/core/SkData.h"
#include "include/core/SkRefCnt.h"
#include "include/core/SkString.h"

namespace sk_woff2_cache_cobalt {

// When enabled, each local WOFF2 font that gets used is stored decompressed,
// as a raw SFNT (TTF/TTC) cache file under <cache dir>/font_cache/, and from
// the next app launch on the font is opened via an mmap-backed stream of that
// file. Without this, FreeType (via brotli) decompresses the WOFF2 file onto
// the heap on every face open and the reconstruction buffer backs the FT_Face
// for the session (NotoSansCJK-Regular.woff2 alone retains ~16.4MB of dirty
// heap). The mmap'd cache bytes are file-backed, clean and evictable by the
// kernel under memory pressure. On a font's first use, the font is
// decompressed once, into memory (see DecompressWoff2()); those bytes back the
// typeface for the session and are written to the cache file in the
// background (see ScheduleCacheFileWrite()). Cache files that no longer belong
// to a current font are deleted in the background at startup; see
// ScheduleCacheCleanup().
//
// Disabled by default. While it is disabled, the font cache is never used:
// SkFontMgr_Cobalt and SkFontStyleSet_Cobalt only call the functions below
// (other than IsMmapFontCacheEnabled()) after checking that it is enabled,
// and otherwise load fonts exactly as they do without this feature.
BASE_DECLARE_FEATURE(kCobaltMmapFontCache);

// Returns true if the CobaltMmapFontCache feature is enabled. Safely returns
// false when the FeatureList has not been initialized yet.
bool IsMmapFontCacheEnabled();

// If |font_file_path| refers to a WOFF2 file whose decompressed SFNT cache
// file already exists, returns the path of that cache file. Returns an empty
// string otherwise (non-WOFF2 file, cache miss or any failure); callers must
// then fall back to DecompressWoff2().
//
// This never writes and never trips base's blocking-call assertions, so it is
// safe to call on any thread, including threads where blocking is disallowed
// (e.g. the browser UI thread, which creates the default typeface).
SkString GetCachedSfntPath(const SkString& font_file_path);

// If |font_file_path| refers to a WOFF2 file, decompresses it into memory and
// returns the SFNT bytes. Returns null for non-WOFF2 files or on any failure;
// callers must then fall back to the regular load path.
//
// The WOFF2 file is mmap'd, not read onto the heap. Like GetCachedSfntPath(),
// this never writes and never trips base's blocking-call assertions, so it is
// safe to call on any thread.
sk_sp<SkData> DecompressWoff2(const SkString& font_file_path);

// Posts a best-effort background task that writes |sfnt|, the decompressed
// bytes of the WOFF2 font at |font_file_path| (see DecompressWoff2()), to the
// cache file of that font, replacing any existing one, so that
// GetCachedSfntPath() finds it from the next app launch on. The task only
// writes, and keeps a reference to |sfnt| until then. The cache file appears
// complete or not at all. Does nothing for non-WOFF2 files or when there is
// no ThreadPool. Like GetCachedSfntPath(), this is safe to call on any thread.
void ScheduleCacheFileWrite(const SkString& font_file_path, sk_sp<SkData> sfnt);

// Posts a best-effort background task that deletes every file in the font
// cache directory other than the cache files of the current version (size and
// mtime) of the WOFF2 fonts in |font_file_paths|; other paths are ignored.
// Called with all of the local fonts at startup, this deletes the cache files
// of fonts that were updated or removed (e.g. by a firmware update) and the
// temporary files of interrupted cache file writes, so the cache holds at
// most one copy of the current fonts. Cache files of fonts that are installed
// but unused are kept. The task runs in order with, and never concurrently
// with, the ones posted by ScheduleCacheFileWrite(), so it never deletes a
// cache file that is being written in this process. Does nothing when there
// is no ThreadPool. Like ScheduleCacheFileWrite(), this is safe to call on any
// thread.
void ScheduleCacheCleanup(std::vector<std::string> font_file_paths);

}  // namespace sk_woff2_cache_cobalt

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKWOFF2FONTCACHE_COBALT_H_
