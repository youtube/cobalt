// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "third_party/androidx/local_modifications/ink/ink_jni_headers/InkJniLoader_jni.h"
#include "third_party/androidx/third_party/ink/ink_jni_registration.h"

// Ink cannot rely on implicit JNI registration because libchrome.so is
// loaded by the base split, but Ink's java code lives in the on_demand
// split.
static void JNI_InkJniLoader_Init(JNIEnv* env) {
  Ink_RegisterNatives(env);
}

DEFINE_JNI_FOR_InkJniLoader()

// Paper over a misalignment between Java & C++. This method was removed
// from C++, but not from Java.
// Can likely be removed upon next roll.
extern "C" JNIEXPORT jint JNICALL
Java_androidx_ink_strokes_InProgressStrokeNative_getTriangleIndexStride(
    JNIEnv* env,
    jobject thiz,
    jlong native_pointer,
    jint coat_index,
    jint mesh_index) {
  return 2;
}
