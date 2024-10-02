// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/at_exit.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_support_android.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/java/system/jni_headers/MojoTestRule_jni.h"

using base::android::JavaParamRef;

namespace {

struct TestEnvironment {
  TestEnvironment() {}

  base::ShadowingAtExitManager at_exit;
  base::SingleThreadTaskExecutor main_task_executor;
};

}  // namespace

namespace mojo {
namespace android {

static void JNI_MojoTestRule_InitCore(JNIEnv* env) {
  mojo::core::Init();
}

static void JNI_MojoTestRule_Init(JNIEnv* env) {
  base::InitAndroidTestMessageLoop();
}

static jlong JNI_MojoTestRule_SetupTestEnvironment(JNIEnv* env) {
  return reinterpret_cast<intptr_t>(new TestEnvironment());
}

static void JNI_MojoTestRule_TearDownTestEnvironment(JNIEnv* env,
                                                     jlong test_environment) {
  delete reinterpret_cast<TestEnvironment*>(test_environment);
}

static void JNI_MojoTestRule_RunLoop(JNIEnv* env, jlong timeout_ms) {
  base::RunLoop run_loop;
  if (timeout_ms) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitWhenIdleClosure(),
        base::Milliseconds(timeout_ms));
    run_loop.Run();
  } else {
    run_loop.RunUntilIdle();
  }
}

}  // namespace android
}  // namespace mojo
