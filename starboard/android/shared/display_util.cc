#include "starboard/android/shared/display_util.h"

#include "base/android/jni_android.h"
#include "starboard/android/shared/media_capabilities_cache.h"
#include "starboard/shared/starboard/media/mime_supportability_cache.h"

#include "cobalt/android/jni_headers/DisplayUtil_jni.h"

namespace starboard {
using base::android::ScopedJavaLocalRef;

DisplayUtil::Dpi DisplayUtil::GetDisplayDpi() {
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedJavaLocalRef<jobject> display_dpi_obj =
      Java_DisplayUtil_getDisplayDpi(env);

  return {Java_DisplayDpi_getX(env, display_dpi_obj),
          Java_DisplayDpi_getY(env, display_dpi_obj)};
}

void JNI_DisplayUtil_OnDisplayChanged(JNIEnv* env) {
  // Display device change could change hdr capabilities.
  MediaCapabilitiesCache::GetInstance()->ClearCache();
  MimeSupportabilityCache::GetInstance()->ClearCachedMimeSupportabilities();
}

}  // namespace starboard
