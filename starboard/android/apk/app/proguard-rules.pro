# Add project specific ProGuard rules here.
# By default, the flags in this file are appended to flags specified
# in <ANDROID_SDK>/tools/proguard/proguard-android.txt
# You can edit the include path and order by changing the proguardFiles
# directive in build.gradle.
#
# For more details, see
#   http://developer.android.com/guide/developing/tools/proguard.html

# JNI is an entry point that's hard to keep track of, so the @UsedByNative
# annotation marks fields and methods used by native code.

# Keep the annotations that proguard needs to process.
-keep class foo.cobalt.UsedByNative

# Just because native code accesses members of a class, does not mean that the
# class itself needs to be annotated - only classes that are explicitly
# referenced in native code should be annotated.
-keep @foo.cobalt.UsedByNative class *

# Methods and fields that are referenced in native code should be annotated.
-keepclassmembers class * {
    @foo.cobalt.UsedByNative *;
}