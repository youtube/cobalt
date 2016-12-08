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

#include "starboard/decode_target.h"
#include "starboard/log.h"
#include "starboard/mutex.h"

#if SB_VERSION(3) && SB_HAS(GRAPHICS)

namespace {
struct Registry {
  SbMutex mutex;
  SbDecodeTargetProvider* provider;
  void* context;
};

Registry g_registry = {SB_MUTEX_INITIALIZER};
}  // namespace

bool SbDecodeTargetRegisterProvider(SbDecodeTargetProvider* provider,
                                    void* context) {
  SbMutexAcquire(&(g_registry.mutex));
  if (g_registry.provider || g_registry.context) {
    SB_DLOG(WARNING) << __FUNCTION__ << ": "
                     << "Registering (P" << provider << ", C" << context
                     << ") while "
                     << "(P" << g_registry.provider << ", C"
                     << g_registry.context << ") is already registered.";
  }
  g_registry.provider = provider;
  g_registry.context = context;
  SbMutexRelease(&(g_registry.mutex));
  return true;
}

void SbDecodeTargetUnregisterProvider(SbDecodeTargetProvider* provider,
                                      void* context) {
  SbMutexAcquire(&(g_registry.mutex));
  if (g_registry.provider == provider && g_registry.context == context) {
    g_registry.provider = NULL;
    g_registry.context = NULL;
  } else {
    SB_DLOG(WARNING) << __FUNCTION__ << ": "
                     << "Unregistering (P" << provider << ", C" << context
                     << ") while "
                     << "(P" << g_registry.provider << ", C"
                     << g_registry.context << ") is what is registered.";
  }
  SbMutexRelease(&(g_registry.mutex));
}

SbDecodeTarget SbDecodeTargetAcquireFromProvider(SbDecodeTargetFormat format,
                                                 int width,
                                                 int height) {
  SbDecodeTargetProvider* provider = NULL;
  void* context = NULL;
  SbMutexAcquire(&(g_registry.mutex));
  provider = g_registry.provider;
  context = g_registry.context;
  SbMutexRelease(&(g_registry.mutex));
  if (!provider) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": "
                   << "No registered provider.";
    return kSbDecodeTargetInvalid;
  }

  return provider->acquire(context, format, width, height);
}

void SbDecodeTargetReleaseToProvider(SbDecodeTarget decode_target) {
  SbDecodeTargetProvider* provider = NULL;
  void* context = NULL;
  SbMutexAcquire(&(g_registry.mutex));
  provider = g_registry.provider;
  context = g_registry.context;
  SbMutexRelease(&(g_registry.mutex));
  if (!provider) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": "
                   << "No registered provider.";
    return;
  }

  provider->release(context, decode_target);
}

#endif  // SB_VERSION(3) && SB_HAS(GRAPHICS)
