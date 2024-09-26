// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/native_web_keyboard_event.h"

#include "base/android/jni_android.h"
#include "content/browser/renderer_host/input/web_input_event_builders_android.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/native_widget_types.h"

namespace content {

NativeWebKeyboardEvent::NativeWebKeyboardEvent(blink::WebInputEvent::Type type,
                                               int modifiers,
                                               base::TimeTicks timestamp)
    : WebKeyboardEvent(type, modifiers, timestamp),
      os_event(nullptr),
      skip_in_browser(false) {}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(
    const blink::WebKeyboardEvent& web_event,
    gfx::NativeView native_view)
    : WebKeyboardEvent(web_event), os_event(nullptr), skip_in_browser(false) {}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& android_key_event,
    blink::WebInputEvent::Type type,
    int modifiers,
    base::TimeTicks timestamp,
    int keycode,
    int scancode,
    int unicode_character,
    bool is_system_key)
    : WebKeyboardEvent(WebKeyboardEventBuilder::Build(env,
                                                      android_key_event,
                                                      type,
                                                      modifiers,
                                                      timestamp,
                                                      keycode,
                                                      scancode,
                                                      unicode_character,
                                                      is_system_key)),
      os_event(nullptr),
      skip_in_browser(false) {
  if (!android_key_event.is_null())
    os_event.Reset(android_key_event);
}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(
    const NativeWebKeyboardEvent& other)
    : WebKeyboardEvent(other),
      os_event(other.os_event),
      skip_in_browser(other.skip_in_browser) {}

NativeWebKeyboardEvent& NativeWebKeyboardEvent::operator=(
    const NativeWebKeyboardEvent& other) {
  WebKeyboardEvent::operator=(other);

  os_event = other.os_event;
  skip_in_browser = other.skip_in_browser;

  return *this;
}

NativeWebKeyboardEvent::~NativeWebKeyboardEvent() {}

}  // namespace content
