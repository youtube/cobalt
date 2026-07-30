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

#include "base/feature_list.h"
#include "include/core/SkString.h"

namespace sk_woff2_cache_cobalt {

// When enabled, each local WOFF2 font is decompressed once to a raw SFNT
// (TTF/TTC) cache file under <cache dir>/font_cache/ and subsequently opened
// via an mmap-backed stream. Without this, FreeType (via brotli) decompresses
// the WOFF2 file onto the heap on every face open and the reconstruction
// buffer backs the FT_Face for the session (NotoSansCJK-Regular.woff2 alone
// retains ~16.4MB of dirty heap). The mmap'd cache bytes are file-backed,
// clean and evictable by the kernel under memory pressure.
BASE_DECLARE_FEATURE(kCobaltMmapFontCache);

// Returns true if the CobaltMmapFontCache feature is enabled. Safely returns
// false when the FeatureList has not been initialized yet.
bool IsMmapFontCacheEnabled();

// If |font_file_path| refers to a WOFF2 file, returns the path of a cache
// file containing the decompressed SFNT bytes, decompressing the font and
// writing the cache file (atomic tmp + rename) on first use. The cache file
// name is keyed on the source file's basename, size and mtime, so a changed
// source font naturally produces a new cache entry. Returns an empty string
// for non-WOFF2 files or on any failure; callers must then fall back to the
// regular (in-heap decompression) load path.
SkString GetOrCreateCachedSfntPath(const SkString& font_file_path);

}  // namespace sk_woff2_cache_cobalt

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_PORTS_SKWOFF2FONTCACHE_COBALT_H_
