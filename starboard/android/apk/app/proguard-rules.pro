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
