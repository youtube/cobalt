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

#include "cobalt/browser/on_screen_keyboard/on_screen_keyboard_impl_stub.h"

#include "cobalt/browser/on_screen_keyboard/on_screen_keyboard_impl_base.h"
#include "content/public/browser/render_frame_host.h"

namespace on_screen_keyboard {

OnScreenKeyboardImplStub::OnScreenKeyboardImplStub(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<mojom::OnScreenKeyboard> receiver)
    : OnScreenKeyboardImpl(render_frame_host, std::move(receiver)) {}

void OnScreenKeyboardImplStub::Show(const std::string& text,
                                    mojom::KeyboardOptionsPtr options,
                                    ShowCallback callback) {
  std::move(callback).Run();
}

void OnScreenKeyboardImplStub::Hide(HideCallback callback) {
  std::move(callback).Run();
}

void OnScreenKeyboardImplStub::Focus(FocusCallback callback) {
  std::move(callback).Run();
}

void OnScreenKeyboardImplStub::Blur(BlurCallback callback) {
  std::move(callback).Run();
}

void OnScreenKeyboardImplStub::UpdateSuggestions(
    const std::vector<std::string>& suggestions,
    UpdateSuggestionsCallback callback) {
  std::move(callback).Run();
}

void OnScreenKeyboardImplStub::KeepFocus(bool keep_focus) {}

void OnScreenKeyboardImplStub::SupportsSuggestions(
    SupportsSuggestionsCallback callback) {
  std::move(callback).Run(false);
}

void OnScreenKeyboardImplStub::IsShown(IsShownCallback callback) {
  std::move(callback).Run(false);
}

void OnScreenKeyboardImplStub::BoundingRect(BoundingRectCallback callback) {
  std::move(callback).Run(gfx::RectF(0, 0, 0, 0));
}

}  // namespace on_screen_keyboard
