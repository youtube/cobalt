// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

// A singleton that can be created and deleted which provides global access to
// the one instance of the class.

#ifndef COBALT_WATCHDOG_SINGLETON_H_
#define COBALT_WATCHDOG_SINGLETON_H_

#include "cobalt/persistent_storage/persistent_settings.h"

namespace cobalt {
namespace watchdog {

// Singleton helper class.
template <typename Type>
class Singleton {
 protected:
  Singleton() {}
  static Type* s_singleton;

 public:
  // This helper class assumes that Type has a function
  // "bool Initialize();" that returns false when
  // initialization fails and a function "void Uninitialize();".

  static Type* CreateInstance(
      persistent_storage::PersistentSettings* persistent_settings) {
    if (!s_singleton) {
      Type* singleton = new Type();
      if (singleton && singleton->Type::Initialize(persistent_settings))
        s_singleton = singleton;
    }
    return s_singleton;
  }

  static Type* CreateStubInstance() {
    if (!s_singleton) {
      Type* singleton = new Type();
      if (singleton && singleton->Type::InitializeStub())
        s_singleton = singleton;
    }
    return s_singleton;
  }

  static Type* GetInstance() { return s_singleton; }

  static void DeleteInstance() {
    if (s_singleton) {
      s_singleton->Type::Uninitialize();
      delete s_singleton;
      s_singleton = nullptr;
    }
  }

  // Prevent copying and moving.
  Singleton(Singleton const&) = delete;
  Singleton& operator=(const Singleton&) = delete;
};

template <typename Type>
Type* Singleton<Type>::s_singleton = nullptr;

}  // namespace watchdog
}  // namespace cobalt

#endif  // COBALT_WATCHDOG_SINGLETON_H_
