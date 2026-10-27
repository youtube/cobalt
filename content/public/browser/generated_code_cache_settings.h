// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_GENERATED_CODE_CACHE_SETTINGS_H_
#define CONTENT_PUBLIC_BROWSER_GENERATED_CODE_CACHE_SETTINGS_H_

#include "base/check_op.h"
#include "base/files/file_path.h"
#include "build/build_config.h"

namespace content {

class GeneratedCodeCacheSettings {
 public:
  GeneratedCodeCacheSettings(bool enabled, int size, base::FilePath path)
      : enabled_(enabled),
        size_in_bytes_(size),
#if BUILDFLAG(IS_COBALT)
        wasm_size_in_bytes_(size),
#endif
        path_(path) {
    DCHECK_GE(size_in_bytes_, 0);
  }

#if BUILDFLAG(IS_COBALT)
  GeneratedCodeCacheSettings(bool enabled,
                             int size,
                             int wasm_size,
                             base::FilePath path)
      : enabled_(enabled),
        size_in_bytes_(size),
        wasm_size_in_bytes_(wasm_size),
        path_(path) {
    DCHECK_GE(size_in_bytes_, 0);
    DCHECK_GE(wasm_size_in_bytes_, 0);
  }
#endif

  // Specifies if code caching is enanled. If it is disabled, the generated
  // code will not be cached.
  bool enabled() const { return enabled_; }

  // Specifies the size of the code cache. If this value is 0, then the
  // disk_cache chooses the size of the cache based on some heuristics.
  int64_t size_in_bytes() const { return size_in_bytes_; }

#if BUILDFLAG(IS_COBALT)
  int64_t wasm_size_in_bytes() const { return wasm_size_in_bytes_; }
#endif

  // Specifies the path of the directory where the generated code be cached
  // on the disk.
  base::FilePath path() const { return path_; }

 private:
  const bool enabled_;
  const int64_t size_in_bytes_;
#if BUILDFLAG(IS_COBALT)
  const int64_t wasm_size_in_bytes_;
#endif
  const base::FilePath path_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_GENERATED_CODE_CACHE_SETTINGS_H_
