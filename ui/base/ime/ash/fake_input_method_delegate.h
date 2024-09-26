// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_ASH_FAKE_INPUT_METHOD_DELEGATE_H_
#define UI_BASE_IME_ASH_FAKE_INPUT_METHOD_DELEGATE_H_

#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "ui/base/ime/ash/input_method_delegate.h"

namespace ash {
namespace input_method {

class COMPONENT_EXPORT(UI_BASE_IME_ASH) FakeInputMethodDelegate
    : public InputMethodDelegate {
 public:
  using LanguageNameLocalizationCallback =
      base::RepeatingCallback<std::u16string(const std::string&)>;
  using GetLocalizedStringCallback =
      base::RepeatingCallback<std::u16string(int)>;

  FakeInputMethodDelegate();

  FakeInputMethodDelegate(const FakeInputMethodDelegate&) = delete;
  FakeInputMethodDelegate& operator=(const FakeInputMethodDelegate&) = delete;

  ~FakeInputMethodDelegate() override;

  // InputMethodDelegate implementation:
  std::string GetHardwareKeyboardLayouts() const override;
  std::u16string GetLocalizedString(int resource_id) const override;
  void SetHardwareKeyboardLayoutForTesting(const std::string& layout) override;
  std::u16string GetDisplayLanguageName(
      const std::string& language_code) const override;

  void set_hardware_keyboard_layout(const std::string& value) {
    hardware_keyboard_layout_ = value;
  }

  void set_active_locale(const std::string& value) {
    active_locale_ = value;
  }

  void set_get_display_language_name_callback(
      const LanguageNameLocalizationCallback& callback) {
    get_display_language_name_callback_ = callback;
  }

  void set_get_localized_string_callback(
      const GetLocalizedStringCallback& callback) {
    get_localized_string_callback_ = callback;
  }

 private:
  std::string hardware_keyboard_layout_;
  std::string active_locale_;
  LanguageNameLocalizationCallback get_display_language_name_callback_;
  GetLocalizedStringCallback get_localized_string_callback_;
};

}  // namespace input_method
}  // namespace ash

#endif  // UI_BASE_IME_ASH_FAKE_INPUT_METHOD_DELEGATE_H_
