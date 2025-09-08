// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_BROWSER_H5VCC_ACCESSIBILITY_H5VCC_ACCESSIBILITY_IMPL_H_
#define COBALT_BROWSER_H5VCC_ACCESSIBILITY_H5VCC_ACCESSIBILITY_IMPL_H_

#include <string>

#include "base/threading/thread_checker.h"
#include "cobalt/browser/h5vcc_accessibility/public/mojom/h5vcc_accessibility.mojom.h"
#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

#if BUILDFLAG(IS_ANDROIDTV)
#include "starboard/android/shared/text_to_speech_helper.h"
#endif

namespace content {
class RenderFrameHost;
}  // namespace content

namespace h5vcc_accessibility {

// Implements the H5vccAccessibilityBrowser Mojo interface and extends
// DocumentService so that an object's lifetime is scoped to the corresponding
// document / RenderFrameHost (see DocumentService for details).
class H5vccAccessibilityImpl
    : public content::DocumentService<mojom::H5vccAccessibilityBrowser>
#if BUILDFLAG(IS_ANDROIDTV)
    ,  // TODO(b/407584170): Understand how many H5vccAccessibilityImpl
       // instances are generated in the browser.
       // TODO(b/407584348): Compare with a different design approach that
       // implements a separate EventListener to replace TextToSpeechObserver.
       // With this alternative, H5vccAccessibilityImpl can have a single
       // responsibility and delegate client/listener management to another
       // class.
      public starboard::android::shared::TextToSpeechObserver
#endif
{
 public:
  // Creates a H5vccAccessibilityImpl. The H5vccAccessibilityImpl is bound to
  // the receiver and its lifetime is scoped to the render_frame_host.
  // This function will be invoked when Blink process requests
  // BrowserInterfaceBroker.
  static void Create(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<mojom::H5vccAccessibilityBrowser> receiver);

  H5vccAccessibilityImpl(const H5vccAccessibilityImpl&) = delete;
  H5vccAccessibilityImpl& operator=(const H5vccAccessibilityImpl&) = delete;

  // mojom::H5vccAccessibilityBrowser APIs:
  void IsTextToSpeechEnabledSync(
      IsTextToSpeechEnabledSyncCallback callback) override;
  void RegisterClient(
      mojo::PendingRemote<mojom::H5vccAccessibilityClient> client) override;

#if BUILDFLAG(IS_ANDROIDTV)
  // starboard::android::shared::TextToSpeechObserver APIs:
  void ObserveTextToSpeechChange() override;
#endif
 private:
  H5vccAccessibilityImpl(
      content::RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<mojom::H5vccAccessibilityBrowser> receiver);
  ~H5vccAccessibilityImpl();

  mojo::RemoteSet<mojom::H5vccAccessibilityClient> remote_clients_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace h5vcc_accessibility

#endif  // COBALT_BROWSER_H5VCC_ACCESSIBILITY_H5VCC_ACCESSIBILITY_IMPL_H_
