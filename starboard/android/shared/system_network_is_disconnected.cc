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

#include "starboard/system.h"
#include "starboard/android/shared/jni_env_ext.h"

namespace starboard {
namespace android {
namespace shared {

bool IsSystemNetworkConnected() {
  JniEnvExt* env = JniEnvExt::Get();
  jboolean j_is_connected = env->CallStarboardBooleanMethodOrAbort(
      "isNetworkConnected", "()Z");
  return j_is_connected;
}

}  // namespace shared
}  // namespace android
}  // namespace starboard

bool SbSystemNetworkIsDisconnected() {
  return !starboard::android::shared::IsSystemNetworkConnected();
}
