// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/devices/input_device_observer_android.h"

#include "base/memory/singleton.h"
#include "ui/events/devices/ui_events_devices_jni_headers/InputDeviceObserver_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;

namespace ui {

InputDeviceObserverAndroid::InputDeviceObserverAndroid() {}

InputDeviceObserverAndroid::~InputDeviceObserverAndroid() {}

InputDeviceObserverAndroid* InputDeviceObserverAndroid::GetInstance() {
  return base::Singleton<
      InputDeviceObserverAndroid,
      base::LeakySingletonTraits<InputDeviceObserverAndroid>>::get();
}

void InputDeviceObserverAndroid::AddObserver(
    ui::InputDeviceEventObserver* observer) {
  observers_.AddObserver(observer);
  JNIEnv* env = AttachCurrentThread();
  Java_InputDeviceObserver_addObserver(env);
}

void InputDeviceObserverAndroid::RemoveObserver(
    ui::InputDeviceEventObserver* observer) {
  observers_.RemoveObserver(observer);
  JNIEnv* env = AttachCurrentThread();
  Java_InputDeviceObserver_removeObserver(env);
}

static void JNI_InputDeviceObserver_InputConfigurationChanged(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  InputDeviceObserverAndroid::GetInstance()
      ->NotifyObserversDeviceConfigurationChanged();
}

void InputDeviceObserverAndroid::NotifyObserversDeviceConfigurationChanged() {
  for (ui::InputDeviceEventObserver& observer : observers_)
    observer.OnInputDeviceConfigurationChanged(
        InputDeviceEventObserver::kMouse | InputDeviceEventObserver::kKeyboard |
        InputDeviceEventObserver::kTouchpad);
}

}  // namespace ui
