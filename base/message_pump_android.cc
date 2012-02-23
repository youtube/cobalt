// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_pump_android.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/logging.h"
#include "jni/system_message_handler_jni.h"

using base::android::ScopedJavaReference;

namespace {

const char* kClassPathName = "com/android/chromeview/base/SystemMessageHandler";

jobject g_system_message_handler_obj = NULL;

}  // namespace

// ----------------------------------------------------------------------------
// Native JNI methods called by Java.
// ----------------------------------------------------------------------------
// This method can not move to anonymous namespace as it has been declared as
// 'static' in system_message_handler_jni.h.
static jboolean DoRunLoopOnce(JNIEnv* env, jobject obj, jint native_delegate) {
  base::MessagePump::Delegate* delegate =
      reinterpret_cast<base::MessagePump::Delegate*>(native_delegate);
  DCHECK(delegate);
  // This is based on MessagePumpForUI::DoRunLoop() from desktop.
  // Note however that our system queue is handled in the java side.
  // In desktop we inspect and process a single system message and then
  // we call DoWork() / DoDelayedWork().
  // On Android, the java message queue may contain messages for other handlers
  // that will be processed before calling here again.
  bool more_work_is_plausible = delegate->DoWork();

  // This is the time when we need to do delayed work.
  base::TimeTicks delayed_work_time;
  more_work_is_plausible |= delegate->DoDelayedWork(&delayed_work_time);

  // This is a major difference between android and other platforms: since we
  // can't inspect it and process just one single message, instead we'll yeld
  // the callstack, and post a message to call us back soon.
  if (more_work_is_plausible)
    return true;

  more_work_is_plausible = delegate->DoIdleWork();
  if (!more_work_is_plausible && !delayed_work_time.is_null()) {
    // We only set the timer here as returning true would post a message.
    jlong millis =
        (delayed_work_time - base::TimeTicks::Now()).InMillisecondsRoundedUp();
    Java_SystemMessageHandler_setDelayedTimer(env, obj, millis);
    base::android::CheckException(env);
  }
  return more_work_is_plausible;
}

namespace base {

MessagePumpForUI::MessagePumpForUI()
    : state_(NULL) {
}

MessagePumpForUI::~MessagePumpForUI() {
}

void MessagePumpForUI::Run(Delegate* delegate) {
  NOTREACHED() << "UnitTests should rely on MessagePumpForUIStub in"
      " test_stub_android.h";
}

void MessagePumpForUI::Start(Delegate* delegate) {
  state_ = new MessageLoop::AutoRunState(MessageLoop::current());

  DCHECK(!g_system_message_handler_obj);

  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);

  jclass clazz = env->FindClass(kClassPathName);
  DCHECK(clazz);

  jmethodID constructor = base::android::GetMethodID(env, clazz, "<init>",
                                                     "(I)V");
  ScopedJavaReference<jobject> client(env, env->NewObject(clazz, constructor,
                                                          delegate));
  DCHECK(client.obj());

  g_system_message_handler_obj = env->NewGlobalRef(client.obj());

  base::android::CheckException(env);
}

void MessagePumpForUI::Quit() {
  if (g_system_message_handler_obj) {
    JNIEnv* env = base::android::AttachCurrentThread();
    DCHECK(env);

    Java_SystemMessageHandler_removeTimer(env, g_system_message_handler_obj);
    env->DeleteGlobalRef(g_system_message_handler_obj);
    base::android::CheckException(env);
    g_system_message_handler_obj = NULL;
  }

  if (state_) {
    delete state_;
    state_ = NULL;
  }
}

void MessagePumpForUI::ScheduleWork() {
  if (!g_system_message_handler_obj)
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);

  Java_SystemMessageHandler_setTimer(env, g_system_message_handler_obj);
  base::android::CheckException(env);

}

void MessagePumpForUI::ScheduleDelayedWork(const TimeTicks& delayed_work_time) {
  if (!g_system_message_handler_obj)
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);

  jlong millis =
      (delayed_work_time - base::TimeTicks::Now()).InMillisecondsRoundedUp();
  // Note that we're truncating to milliseconds as required by the java side,
  // even though delayed_work_time is microseconds resolution.
  Java_SystemMessageHandler_setDelayedTimer(env, g_system_message_handler_obj,
                                            millis);
  base::android::CheckException(env);
}

// Register native methods
bool RegisterSystemMessageHandler(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace base
