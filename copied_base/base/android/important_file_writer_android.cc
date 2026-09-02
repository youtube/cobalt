// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/files/important_file_writer.h"
#include "base/threading/thread_restrictions.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/base_jni/ImportantFileWriterAndroid_jni.h"

namespace base {
namespace android {

class ScopedAllowBlockingForImportantFileWriter
    : public base::ScopedAllowBlocking {};

static jboolean JNI_ImportantFileWriterAndroid_WriteFileAtomically(
    JNIEnv* env,
    const jni_zero::JavaRef<jstring>& file_name,
    const jni_zero::JavaRef<jbyteArray>& data) {
  // This is called on the UI thread during shutdown to save tab data, so
  // needs to enable IO.
  ScopedAllowBlockingForImportantFileWriter allow_blocking;
  base::FilePath path(ConvertJavaStringToUTF8(env, file_name));
  std::string native_data_string;
  JavaByteArrayToString(env, data, &native_data_string);
  return base::ImportantFileWriter::WriteFileAtomically(
      path, native_data_string);
}

}  // namespace android
}  // namespace base
