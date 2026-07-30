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

#include "cobalt/browser/android/mojo/cobalt_interface_registrar_android.h"

#include <jni.h>

#include "base/android/jni_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "cobalt/android/jni_headers/CobaltInterfaceRegistrar_jni.h"

namespace cobalt {

void RegisterCobaltJavaMojoInterfaces() {
  Java_CobaltInterfaceRegistrar_registerMojoInterfaces(
      base::android::AttachCurrentThread());
}

}  // namespace cobalt
