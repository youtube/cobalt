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

#ifndef COBALT_BROWSER_ON_SCREEN_KEYBOARD_ON_SCREEN_KEYBOARD_IMPL_TVOS_H_
#define COBALT_BROWSER_ON_SCREEN_KEYBOARD_ON_SCREEN_KEYBOARD_IMPL_TVOS_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "cobalt/browser/on_screen_keyboard/on_screen_keyboard_impl_base.h"

namespace on_screen_keyboard {

class OnScreenKeyboardImplTvos final : public OnScreenKeyboardImpl {
 private:
  friend class OnScreenKeyboardImpl;

  OnScreenKeyboardImplTvos(
      content::RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<mojom::OnScreenKeyboard> receiver);

  ~OnScreenKeyboardImplTvos() override;

  void Show(const std::string& text,
            mojom::KeyboardOptionsPtr options,
            ShowCallback) override;

  void Hide(HideCallback) override;

  void Focus(FocusCallback) override;

  void Blur(BlurCallback) override;

  void UpdateSuggestions(const std::vector<std::string>& suggestions,
                         UpdateSuggestionsCallback) override;

  void KeepFocus(bool) override;

  void SupportsSuggestions(SupportsSuggestionsCallback) override;

  void IsShown(IsShownCallback) override;

  void BoundingRect(BoundingRectCallback) override;

  struct ObjCStorage;
  std::unique_ptr<ObjCStorage> objc_storage_;

  base::WeakPtrFactory<OnScreenKeyboardImplTvos> weak_ptr_factory_{this};
};

}  // namespace on_screen_keyboard

#endif  // COBALT_BROWSER_ON_SCREEN_KEYBOARD_ON_SCREEN_KEYBOARD_IMPL_TVOS_H_
