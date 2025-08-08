// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_VIEWS_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_VIEWS_UTILS_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "ui/base/models/image_model.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace views {
class Combobox;
class EditableCombobox;
class EditablePasswordCombobox;
}  // namespace views

namespace views {
class Label;
class StyledLabel;
}

namespace password_manager {
struct PasswordForm;
}

// Returns a label that can be displayed as a footer for Password Manager
// bubbles on Desktop or in other UI surfaces. `text_message_id` is the message
// id of the whole text displayed in the footer which should have two place
// holders. One for `link_message_id` that's when clicked, `open_link_closure`
// will invoked. The other placeholder will carry the `email`.
// `context` helps determine how the label text will be rendered.
std::unique_ptr<views::StyledLabel> CreateGooglePasswordManagerLabel(
    int text_message_id,
    int link_message_id,
    const std::u16string& email,
    base::RepeatingClosure open_link_closure,
    int context = CONTEXT_DIALOG_BODY_TEXT_SMALL);

// Returns a label that can be displayed as a footer for Password Manager
// bubbles on Desktop or in other UI surfaces. `text_message_id` is the message
// id of the whole text displayed in the footer which should have one place
// holder for `link_message_id` that's when clicked, `open_link_closure` will
// invoked. `context` helps determine how the label text will be rendered.
std::unique_ptr<views::StyledLabel> CreateGooglePasswordManagerLabel(
    int text_message_id,
    int link_message_id,
    base::RepeatingClosure open_link_closure,
    int context = CONTEXT_DIALOG_BODY_TEXT_SMALL);

// Returns label diaplying the username and password. It handles edge cases like
// empty username and federated credentials.
std::unique_ptr<views::Label> CreateUsernameLabel(
    const password_manager::PasswordForm& form);
std::unique_ptr<views::Label> CreatePasswordLabel(
    const password_manager::PasswordForm& form);

// Returns the size of the icons displayed within combobox.
int ComboboxIconSize();

// Builds a credential rows, adds the given elements to the |parent_view|.
// |destination_field| is nullptr if the destination field shouldn't be shown.
// Generated UI will look like this:
//
//  | destination combobox |
//  Username  | username combobox |
//  Password  | password combobox |
//
void BuildCredentialRows(views::View* parent_view,
                         std::unique_ptr<views::View> destination_field,
                         std::unique_ptr<views::View> username_field,
                         std::unique_ptr<views::View> password_field);

// Creates an EditableCombobox from |PasswordForm.all_alternative_usernames|
// or even just |PasswordForm.username_value|.
std::unique_ptr<views::EditableCombobox> CreateUsernameEditableCombobox(
    const password_manager::PasswordForm& form);

// Creates an EditablePasswordCombobox from
// `PasswordForm.all_alternative_passwords` or even just
// `PasswordForm.password_value`.
std::unique_ptr<views::EditablePasswordCombobox> CreateEditablePasswordCombobox(
    const password_manager::PasswordForm& form,
    views::Button::PressedCallback reveal_password_callback);

// Creates a Combobox with account / device destination.
std::unique_ptr<views::Combobox> CreateDestinationCombobox(
    std::u16string primary_account_email,
    ui::ImageModel primary_account_avatar,
    bool is_using_account_store);

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_VIEWS_UTILS_H_
