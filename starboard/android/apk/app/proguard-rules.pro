# Copyright 2016 Google Inc. All Rights Reserved.
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
-keep @interface foo.cobalt.util.UsedByNative
-keep @foo.cobalt.util.UsedByNative class *
-keepclasseswithmembers class * {
  @foo.cobalt.util.UsedByNative <methods>;
}
-keepclasseswithmembers class * {
  @foo.cobalt.util.UsedByNative <fields>;
}
