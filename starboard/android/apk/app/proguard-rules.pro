# Copyright 2016 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Proguard flags needed for the Android Starboard Java implementation to work
# properly with its JNI native library.
#
# For more details, see
#   http://developer.android.com/guide/developing/tools/proguard.html

# JNI is an entry point that's hard to keep track of, so the @UsedByNative
# annotation marks fields and methods used by native code.

# Annotations are implemented as attributes, so we have to explicitly keep them.
-keepattributes *Annotation*

# Keep classes, methods, and fields that are accessed with JNI.
-keep @interface dev.cobalt.util.UsedByNative
-keep @dev.cobalt.util.UsedByNative class *
-keepclasseswithmembers class * {
  @dev.cobalt.util.UsedByNative <methods>;
}
-keepclasseswithmembers class * {
  @dev.cobalt.util.UsedByNative <fields>;
}

# Keep GameActivity APIs used by JNI (b/254102295).
-keepclassmembers class com.google.androidgamesdk.GameActivity {
   void setWindowFlags(int, int);
   public androidx.core.graphics.Insets getWindowInsets(int);
   public androidx.core.graphics.Insets getWaterfallInsets();
   public void setImeEditorInfo(android.view.inputmethod.EditorInfo);
   public void setImeEditorInfoFields(int, int, int);
}
-keep class androidx.core.graphics.Insets** { *; }
-keep class androidx.core.view.WindowInsetsCompat** { *; }
-keep class com.google.androidgamesdk.gametextinput.** { *; }
