// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"

#ifdef OS_WIN
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#endif

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/hash_tables.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/synchronization/lock.h"

namespace base {
  bool PathProvider(int key, FilePath* result);
#if defined(OS_WIN)
  bool PathProviderWin(int key, FilePath* result);
#elif defined(OS_MACOSX)
  bool PathProviderMac(int key, FilePath* result);
#elif defined(OS_ANDROID)
  bool PathProviderAndroid(int key, FilePath* result);
#elif defined(OS_POSIX)
  bool PathProviderPosix(int key, FilePath* result);
#endif
}

namespace {

typedef base::hash_map<int, FilePath> PathMap;

// We keep a linked list of providers.  In a debug build we ensure that no two
// providers claim overlapping keys.
struct Provider {
  PathService::ProviderFunc func;
  struct Provider* next;
#ifndef NDEBUG
  int key_start;
  int key_end;
#endif
  bool is_static;
};

Provider base_provider = {
  base::PathProvider,
  NULL,
#ifndef NDEBUG
  base::PATH_START,
  base::PATH_END,
#endif
  true
};

#if defined(OS_WIN)
Provider base_provider_win = {
  base::PathProviderWin,
  &base_provider,
#ifndef NDEBUG
  base::PATH_WIN_START,
  base::PATH_WIN_END,
#endif
  true
};
#endif

#if defined(OS_MACOSX)
Provider base_provider_mac = {
  base::PathProviderMac,
  &base_provider,
#ifndef NDEBUG
  base::PATH_MAC_START,
  base::PATH_MAC_END,
#endif
  true
};
#endif

#if defined(OS_ANDROID)
Provider base_provider_android = {
  base::PathProviderAndroid,
  &base_provider,
#ifndef NDEBUG
  0,
  0,
#endif
  true
};
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
Provider base_provider_posix = {
  base::PathProviderPosix,
  &base_provider,
#ifndef NDEBUG
  0,
  0,
#endif
  true
};
#endif


struct PathData {
  base::Lock lock;
  PathMap cache;        // Cache mappings from path key to path value.
  PathMap overrides;    // Track path overrides.
  Provider* providers;  // Linked list of path service providers.

  PathData() {
#if defined(OS_WIN)
    providers = &base_provider_win;
#elif defined(OS_MACOSX)
    providers = &base_provider_mac;
#elif defined(OS_ANDROID)
    providers = &base_provider_android;
#elif defined(OS_POSIX)
    providers = &base_provider_posix;
#endif
  }

  ~PathData() {
    Provider* p = providers;
    while (p) {
      Provider* next = p->next;
      if (!p->is_static)
        delete p;
      p = next;
    }
  }
};

static base::LazyInstance<PathData> g_path_data = LAZY_INSTANCE_INITIALIZER;

static PathData* GetPathData() {
  return g_path_data.Pointer();
}

}  // namespace


// static
bool PathService::GetFromCache(int key, FilePath* result) {
  PathData* path_data = GetPathData();
  base::AutoLock scoped_lock(path_data->lock);

  // check for a cached version
  PathMap::const_iterator it = path_data->cache.find(key);
  if (it != path_data->cache.end()) {
    *result = it->second;
    return true;
  }
  return false;
}

// static
bool PathService::GetFromOverrides(int key, FilePath* result) {
  PathData* path_data = GetPathData();
  base::AutoLock scoped_lock(path_data->lock);

  // check for an overridden version.
  PathMap::const_iterator it = path_data->overrides.find(key);
  if (it != path_data->overrides.end()) {
    *result = it->second;
    return true;
  }
  return false;
}

// static
void PathService::AddToCache(int key, const FilePath& path) {
  PathData* path_data = GetPathData();
  base::AutoLock scoped_lock(path_data->lock);
  // Save the computed path in our cache.
  path_data->cache[key] = path;
}

// TODO(brettw): this function does not handle long paths (filename > MAX_PATH)
// characters). This isn't supported very well by Windows right now, so it is
// moot, but we should keep this in mind for the future.
// static
bool PathService::Get(int key, FilePath* result) {
  PathData* path_data = GetPathData();
  DCHECK(path_data);
  DCHECK(result);
  DCHECK_GE(key, base::DIR_CURRENT);

  // special case the current directory because it can never be cached
  if (key == base::DIR_CURRENT)
    return file_util::GetCurrentDirectory(result);

  if (GetFromCache(key, result))
    return true;

  if (GetFromOverrides(key, result))
    return true;

  FilePath path;

  // search providers for the requested path
  // NOTE: it should be safe to iterate here without the lock
  // since RegisterProvider always prepends.
  Provider* provider = path_data->providers;
  while (provider) {
    if (provider->func(key, &path))
      break;
    DCHECK(path.empty()) << "provider should not have modified path";
    provider = provider->next;
  }

  if (path.empty())
    return false;

  AddToCache(key, path);

  *result = path;
  return true;
}

bool PathService::Override(int key, const FilePath& path) {
  // Just call the full function with true for the value of |create|.
  return OverrideAndCreateIfNeeded(key, path, true);
}

bool PathService::OverrideAndCreateIfNeeded(int key,
                                            const FilePath& path,
                                            bool create) {
  PathData* path_data = GetPathData();
  DCHECK(path_data);
  DCHECK_GT(key, base::DIR_CURRENT) << "invalid path key";

  FilePath file_path = path;

  // For some locations this will fail if called from inside the sandbox there-
  // fore we protect this call with a flag.
  if (create) {
    // Make sure the directory exists. We need to do this before we translate
    // this to the absolute path because on POSIX, AbsolutePath fails if called
    // on a non-existent path.
    if (!file_util::PathExists(file_path) &&
        !file_util::CreateDirectory(file_path))
      return false;
  }

  // We need to have an absolute path, as extensions and plugins don't like
  // relative paths, and will gladly crash the browser in CHECK()s if they get a
  // relative path.
  if (!file_util::AbsolutePath(&file_path))
    return false;

  base::AutoLock scoped_lock(path_data->lock);

  // Clear the cache now. Some of its entries could have depended
  // on the value we are overriding, and are now out of sync with reality.
  path_data->cache.clear();

  path_data->cache[key] = file_path;
  path_data->overrides[key] = file_path;

  return true;
}

void PathService::RegisterProvider(ProviderFunc func, int key_start,
                                   int key_end) {
  PathData* path_data = GetPathData();
  DCHECK(path_data);
  DCHECK_GT(key_end, key_start);

  base::AutoLock scoped_lock(path_data->lock);

  Provider* p;

#ifndef NDEBUG
  p = path_data->providers;
  while (p) {
    DCHECK(key_start >= p->key_end || key_end <= p->key_start) <<
      "path provider collision";
    p = p->next;
  }
#endif

  p = new Provider;
  p->is_static = false;
  p->func = func;
  p->next = path_data->providers;
#ifndef NDEBUG
  p->key_start = key_start;
  p->key_end = key_end;
#endif
  path_data->providers = p;
}
