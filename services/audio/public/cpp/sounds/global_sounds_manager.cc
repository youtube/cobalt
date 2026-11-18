// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/public/cpp/sounds/global_sounds_manager.h"

#include "base/check_deref.h"
#include "services/audio/public/cpp/sounds/sounds_manager.h"

namespace audio {
namespace {

SoundsManager* g_instance = nullptr;

}  // namespace

// static
void GlobalSoundsManager::Create(
    SoundsManager::StreamFactoryBinder stream_factory_binder) {
  CHECK(!g_instance) << "`GlobalSoundsManager::Create` is called twice";
  g_instance =
      SoundsManager::Create(std::move(stream_factory_binder)).release();
}

// static
void GlobalSoundsManager::Shutdown() {
  CHECK(g_instance) << "`GlobalSoundsManager::Shutdown` is called "
                       "without previous call to `GlobalSoundsManager::Create`";
  delete g_instance;
  g_instance = nullptr;
}

// static
SoundsManager& GlobalSoundsManager::Get() {
  return CHECK_DEREF(g_instance);
}

}  // namespace audio
