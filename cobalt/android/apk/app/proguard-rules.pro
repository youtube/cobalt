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

# Annotations are implemented as attributes, so we have to explicitly keep them.
-keepattributes *Annotation*

# TODO(cobalt, b/393465183): Remove the cobalt prefix if we can avoid symbol relocation.
-keepclasseswithmembers,allowaccessmodification class ** {
  @cobalt.org.jni_zero.AccessedByNative <fields>;
}

-keepclasseswithmembers,includedescriptorclasses,allowaccessmodification,allowoptimization class ** {
  @cobalt.org.jni_zero.CalledByNative <methods>;
}

-keepclasseswithmembernames,includedescriptorclasses,allowaccessmodification class ** {
  native <methods>;
}

-keepclasseswithmembers,includedescriptorclasses,allowaccessmodification,allowoptimization class ** {
  @dev.cobalt.coat.javabridge.CobaltJavaScriptInterface <methods>;
}

# Keep classes from the following packages from being obfuscated.
# without this, crash stack will be "at dev.cobalt.coat.CobaltActivity.onCreate(Unknown Source)"
# classes with package name starts with dev.cobalt
-keep,allowshrinking,allowoptimization,allowaccessmodification class dev.cobalt.** { *; }
# classes with package name is org.chromium.components.embedder_support.view
-keep,allowshrinking,allowoptimization,allowaccessmodification class org.chromium.components.embedder_support.view.* { *; }
# classes with package name starts with org.chromium.content
-keep,allowshrinking,allowoptimization,allowaccessmodification class org.chromium.content.** { *; }

# Keeps debugging information for stack traces for the ENTIRE app.
# Without this dev.cobalt.coat.CobaltActivity.onStart() will be renamed to a.b.c.a()
-keepattributes SourceFile,LineNumberTable
