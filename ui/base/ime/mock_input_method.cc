// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/mock_input_method.h"

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "ui/base/ime/ime_key_event_dispatcher.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/event.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace ui {

MockInputMethod::MockInputMethod(
    ImeKeyEventDispatcher* ime_key_event_dispatcher)
    : text_input_client_(nullptr),
      ime_key_event_dispatcher_(ime_key_event_dispatcher) {}

MockInputMethod::~MockInputMethod() {
  for (InputMethodObserver& observer : observer_list_)
    observer.OnInputMethodDestroyed(this);
}

void MockInputMethod::SetImeKeyEventDispatcher(
    ImeKeyEventDispatcher* ime_key_event_dispatcher) {
  ime_key_event_dispatcher_ = ime_key_event_dispatcher;
}

void MockInputMethod::SetFocusedTextInputClient(TextInputClient* client) {
  if (text_input_client_ == client)
    return;
  text_input_client_ = client;
  if (client)
    OnTextInputTypeChanged(client);
}

void MockInputMethod::DetachTextInputClient(TextInputClient* client) {
  if (text_input_client_ == client) {
    text_input_client_ = nullptr;
  }
}

TextInputClient* MockInputMethod::GetTextInputClient() const {
  return text_input_client_;
}

ui::EventDispatchDetails MockInputMethod::DispatchKeyEvent(
    ui::KeyEvent* event) {
  if (text_input_client_) {
    text_input_client_->OnDispatchingKeyEventPostIME(event);
    if (event->handled())
      return EventDispatchDetails();
  }
  return ime_key_event_dispatcher_->DispatchKeyEventPostIME(event);
}

void MockInputMethod::OnFocus() {
  for (InputMethodObserver& observer : observer_list_)
    observer.OnFocus();
}

void MockInputMethod::OnTouch(ui::EventPointerType pointerType) {}

void MockInputMethod::OnBlur() {
  for (InputMethodObserver& observer : observer_list_)
    observer.OnBlur();
}

#if BUILDFLAG(IS_WIN)
bool MockInputMethod::OnUntranslatedIMEMessage(const CHROME_MSG event,
                                               NativeEventResult* result) {
  if (result)
    *result = NativeEventResult();
  return false;
}

void MockInputMethod::OnInputLocaleChanged() {}

bool MockInputMethod::IsInputLocaleCJK() const {
  return false;
}
#endif

void MockInputMethod::OnTextInputTypeChanged(TextInputClient* client) {
  for (InputMethodObserver& observer : observer_list_)
    observer.OnTextInputStateChanged(client);
}

void MockInputMethod::OnCaretBoundsChanged(const TextInputClient* client) {
  for (InputMethodObserver& observer : observer_list_)
    observer.OnCaretBoundsChanged(client);
}

void MockInputMethod::CancelComposition(const TextInputClient* client) {
}

TextInputType MockInputMethod::GetTextInputType() const {
  return TEXT_INPUT_TYPE_NONE;
}

bool MockInputMethod::IsCandidatePopupOpen() const {
  return false;
}

void MockInputMethod::SetVirtualKeyboardVisibilityIfEnabled(bool should_show) {
  for (InputMethodObserver& observer : observer_list_)
    observer.OnVirtualKeyboardVisibilityChangedIfEnabled(should_show);
}

void MockInputMethod::AddObserver(InputMethodObserver* observer) {
  observer_list_.AddObserver(observer);
}

void MockInputMethod::RemoveObserver(InputMethodObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

VirtualKeyboardController* MockInputMethod::GetVirtualKeyboardController() {
  return &keyboard_controller_;
}

}  // namespace ui
