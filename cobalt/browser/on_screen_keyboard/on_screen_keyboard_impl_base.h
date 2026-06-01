// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_BROWSER_ON_SCREEN_KEYBOARD_ON_SCREEN_KEYBOARD_IMPL_BASE_H_
#define COBALT_BROWSER_ON_SCREEN_KEYBOARD_ON_SCREEN_KEYBOARD_IMPL_BASE_H_

#include <string>

#include "cobalt/browser/on_screen_keyboard/public/mojom/on_screen_keyboard.mojom.h"
#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace on_screen_keyboard {

// Implements the OnScreenKeyboard Mojo interface and extends
// DocumentService so that an object's lifetime is scoped to the corresponding
// document / RenderFrameHost (see DocumentService for details).
class OnScreenKeyboardImpl
    : public content::DocumentService<mojom::OnScreenKeyboard> {
 public:
  // Creates a OnScreenKeyboardImpl. The OnScreenKeyboardImpl is bound to the
  // receiver and its lifetime is scoped to the render_frame_host.
  static void Create(content::RenderFrameHost* render_frame_host,
                     mojo::PendingReceiver<mojom::OnScreenKeyboard> receiver);

  OnScreenKeyboardImpl(const OnScreenKeyboardImpl&) = delete;
  OnScreenKeyboardImpl& operator=(const OnScreenKeyboardImpl&) = delete;

  virtual ~OnScreenKeyboardImpl();

 protected:
  OnScreenKeyboardImpl(content::RenderFrameHost& render_frame_host,
                       mojo::PendingReceiver<mojom::OnScreenKeyboard> receiver);

  // The following methods can be used by implementations to notify a connected
  // mojom::OnScreenKeyboardClient of changes.
  void KeyboardBlurred();
  void KeyboardFocused();
  void KeyboardTextChanged(const std::string& text);

 private:
  void RegisterClient(
      mojo::PendingRemote<mojom::OnScreenKeyboardClient> client) override;

  mojo::Remote<mojom::OnScreenKeyboardClient> on_screen_keyboard_client_;
};

}  // namespace on_screen_keyboard

#endif  // COBALT_BROWSER_ON_SCREEN_KEYBOARD_ON_SCREEN_KEYBOARD_IMPL_BASE_H_
