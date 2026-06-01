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

#include "cobalt/browser/on_screen_keyboard/on_screen_keyboard_impl_base.h"

#include "build/build_config.h"
#include "content/public/browser/render_frame_host.h"

#if BUILDFLAG(IS_IOS_TVOS)
#include "cobalt/browser/on_screen_keyboard/on_screen_keyboard_impl_tvos.h"
#else
#include "cobalt/browser/on_screen_keyboard/on_screen_keyboard_impl_stub.h"
#endif

namespace on_screen_keyboard {

OnScreenKeyboardImpl::OnScreenKeyboardImpl(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<mojom::OnScreenKeyboard> receiver)
    : content::DocumentService<mojom::OnScreenKeyboard>(render_frame_host,
                                                        std::move(receiver)) {}

OnScreenKeyboardImpl::~OnScreenKeyboardImpl() = default;

void OnScreenKeyboardImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::OnScreenKeyboard> receiver) {
#if BUILDFLAG(IS_IOS_TVOS)
  new OnScreenKeyboardImplTvos(*render_frame_host, std::move(receiver));
#else
  new OnScreenKeyboardImplStub(*render_frame_host, std::move(receiver));
#endif
}

void OnScreenKeyboardImpl::RegisterClient(
    mojo::PendingRemote<mojom::OnScreenKeyboardClient> client) {
  on_screen_keyboard_client_.Bind(std::move(client));
}

void OnScreenKeyboardImpl::KeyboardBlurred() {
  if (!on_screen_keyboard_client_.is_bound()) {
    return;
  }
  on_screen_keyboard_client_->KeyboardBlurred();
}

void OnScreenKeyboardImpl::KeyboardFocused() {
  if (!on_screen_keyboard_client_.is_bound()) {
    return;
  }
  on_screen_keyboard_client_->KeyboardFocused();
}

void OnScreenKeyboardImpl::KeyboardTextChanged(const std::string& text) {
  if (!on_screen_keyboard_client_.is_bound()) {
    return;
  }
  on_screen_keyboard_client_->KeyboardTextChanged(text);
}

}  // namespace on_screen_keyboard
