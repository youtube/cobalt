// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_pump_android.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/time.h"
#include "jni/SystemMessageHandler_jni.h"

using base::android::ScopedJavaLocalRef;

namespace {

base::LazyInstance<base::android::ScopedJavaGlobalRef<jobject> >
    g_system_message_handler_obj = LAZY_INSTANCE_INITIALIZER;

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
  }
  return more_work_is_plausible;
}

namespace base {

MessagePumpForUI::MessagePumpForUI()
    : run_loop_(NULL) {
}

MessagePumpForUI::~MessagePumpForUI() {
}

void MessagePumpForUI::Run(Delegate* delegate) {
  NOTREACHED() << "UnitTests should rely on MessagePumpForUIStub in"
      " test_stub_android.h";
}

void MessagePumpForUI::Start(Delegate* delegate) {
  run_loop_ = new base::RunLoop();
  // Since the RunLoop was just created above, BeforeRun should be guaranteed to
  // return true (it only returns false if the RunLoop has been Quit already).
  if (!run_loop_->BeforeRun())
    NOTREACHED();

  DCHECK(g_system_message_handler_obj.Get().is_null());

  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);

  g_system_message_handler_obj.Get().Reset(
      Java_SystemMessageHandler_create(env, reinterpret_cast<jint>(delegate)));
}

void MessagePumpForUI::Quit() {
  if (!g_system_message_handler_obj.Get().is_null()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    DCHECK(env);

    Java_SystemMessageHandler_removeTimer(env,
        g_system_message_handler_obj.Get().obj());
    g_system_message_handler_obj.Get().Reset();
  }

  if (run_loop_) {
    run_loop_->AfterRun();
    delete run_loop_;
    run_loop_ = NULL;
  }
}

void MessagePumpForUI::ScheduleWork() {
  DCHECK(!g_system_message_handler_obj.Get().is_null());

  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);

  Java_SystemMessageHandler_setTimer(env,
      g_system_message_handler_obj.Get().obj());
}

void MessagePumpForUI::ScheduleDelayedWork(const TimeTicks& delayed_work_time) {
  DCHECK(!g_system_message_handler_obj.Get().is_null());

  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);

  jlong millis =
      (delayed_work_time - base::TimeTicks::Now()).InMillisecondsRoundedUp();
  // Note that we're truncating to milliseconds as required by the java side,
  // even though delayed_work_time is microseconds resolution.
  Java_SystemMessageHandler_setDelayedTimer(env,
      g_system_message_handler_obj.Get().obj(), millis);
}

// static
bool MessagePumpForUI::RegisterBindings(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace base
