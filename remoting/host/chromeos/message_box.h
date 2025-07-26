// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMEOS_MESSAGE_BOX_H_
#define REMOTING_HOST_CHROMEOS_MESSAGE_BOX_H_

#include <optional>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace views {
class DialogDelegate;
}  // namespace views

namespace remoting {

// Overview:
// Shows a system modal message box with OK and cancel buttons. This class
// is not thread-safe, it must be called on the UI thread of the browser
// process. Destroy the instance to hide the message box.
class MessageBox {
 public:
  enum Result { OK, CANCEL };

  // ResultCallback will be invoked with Result::Cancel if the user closes the
  // MessageBox without clicking on any buttons.
  typedef base::OnceCallback<void(Result)> ResultCallback;

  MessageBox(const std::u16string& title_label,
             const std::u16string& message_label,
             const std::u16string& ok_label,
             const std::u16string& cancel_label,
             const std::optional<ui::ImageModel> icon,
             ResultCallback result_callback);

  MessageBox(const MessageBox&) = delete;
  MessageBox& operator=(const MessageBox&) = delete;

  ~MessageBox();

  // Shows the message box widget in `ash::kShellWindowId_SystemModalContainer`
  // container. The message box is only visible when an active user session is
  // present.
  void Show();

  // Shows the message box in a suggested parent view. The message box's
  // visibility is tied to the parent's visibility. A suitable parent can be
  // chosen from the containers in `ash::ShellWindowIds`.
  void ShowInParentContainer(gfx::NativeView parent);

  void ChangeParentContainer(gfx::NativeView parent);

  views::DialogDelegate& GetDialogDelegate();

 private:
  class Core;
  raw_ptr<Core> core_;
  base::ThreadChecker thread_checker_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMEOS_MESSAGE_BOX_H_
