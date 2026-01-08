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

#include "cobalt/testing/browser_tests/browser/test_shell.h"

#include "base/command_line.h"
#include "cobalt/shell/common/shell_switches.h"
#include "cobalt/testing/browser_tests/common/shell_test_switches.h"
#include "components/custom_handlers/protocol_handler.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/custom_handlers/simple_protocol_handler_registry_factory.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/presentation_receiver_flags.h"
#include "content/public/browser/renderer_preferences_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"

namespace content {

TestShell::TestShell(std::unique_ptr<WebContents> web_contents,
                     bool should_set_delegate)
    : Shell(std::move(web_contents), nullptr, should_set_delegate) {}

TestShell::~TestShell() {}

TestShell* TestShell::CreateShell(std::unique_ptr<WebContents> web_contents,
                                  const gfx::Size& initial_size,
                                  bool should_set_delegate) {
  WebContents* raw_web_contents = web_contents.get();
  TestShell* shell =
      new TestShell(std::move(web_contents), should_set_delegate);
  TestShell::GetPlatform()->CreatePlatformWindow(shell, initial_size);

  // Note: Do not make RenderFrameHost or RenderViewHost specific state changes
  // here, because they will be forgotten after a cross-process navigation. Use
  // RenderFrameCreated or RenderViewCreated instead.
  if (switches::IsRunWebTestsSwitchPresent()) {
    raw_web_contents->GetMutableRendererPrefs()->use_custom_colors = false;
    raw_web_contents->SyncRendererPrefs();
  }

  FinishShellInitialization(shell);
  return shell;
}

base::OnceCallback<void(TestShell*)> TestShell::shell_created_callback_;

// static
void TestShell::SetShellCreatedCallback(
    base::OnceCallback<void(TestShell*)> shell_created_callback) {
  if (!shell_created_callback) {
    shell_created_callback_.Reset();
    Shell::SetShellCreatedCallback({});
    return;
  }
  shell_created_callback_ = std::move(shell_created_callback);
  Shell::SetShellCreatedCallback(base::BindOnce([](Shell* shell) {
    if (shell_created_callback_) {
      std::move(shell_created_callback_).Run(static_cast<TestShell*>(shell));
    }
  }));
}

TestShell* TestShell::CreateNewWindow(
    BrowserContext* browser_context,
    const GURL& url,
    const scoped_refptr<SiteInstance>& site_instance,
    const gfx::Size& initial_size) {
  WebContents::CreateParams create_params(browser_context, site_instance);
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForcePresentationReceiverForTesting)) {
    create_params.starting_sandbox_flags = kPresentationReceiverSandboxFlags;
  }
  std::unique_ptr<WebContents> web_contents =
      WebContents::Create(create_params);
  TestShell* shell =
      CreateShell(std::move(web_contents), AdjustWindowSize(initial_size),
                  true /* should_set_delegate */);

  if (!url.is_empty()) {
    shell->LoadURL(url);
  }
  return shell;
}

void TestShell::RegisterProtocolHandler(RenderFrameHost* requesting_frame,
                                        const std::string& protocol,
                                        const GURL& url,
                                        bool user_gesture) {
  BrowserContext* context = requesting_frame->GetBrowserContext();
  if (context->IsOffTheRecord()) {
    return;
  }

  custom_handlers::ProtocolHandler handler =
      custom_handlers::ProtocolHandler::CreateProtocolHandler(
          protocol, url, GetProtocolHandlerSecurityLevel(requesting_frame));

  // The parameters's normalization process defined in the spec has been already
  // applied in the WebContentImpl class, so at this point it shouldn't be
  // possible to create an invalid handler.
  // https://html.spec.whatwg.org/multipage/system-state.html#normalize-protocol-handler-parameters
  DCHECK(handler.IsValid());

  custom_handlers::ProtocolHandlerRegistry* registry = custom_handlers::
      SimpleProtocolHandlerRegistryFactory::GetForBrowserContext(context, true);
  DCHECK(registry);
  if (registry->SilentlyHandleRegisterHandlerRequest(handler)) {
    return;
  }

  if (!user_gesture && !windows().empty()) {
    // TODO(jfernandez): This is not strictly needed, but we need a way to
    // inform the observers in browser tests that the request has been
    // cancelled, to avoid timeouts. Chrome just holds the handler as pending in
    // the PageContentSettingsDelegate, but we don't have such thing in the
    // Content Shell.
    registry->OnDenyRegisterProtocolHandler(handler);
    return;
  }

  // FencedFrames can not register to handle any protocols.
  if (requesting_frame->IsNestedWithinFencedFrame()) {
    registry->OnIgnoreRegisterProtocolHandler(handler);
    return;
  }

  // TODO(jfernandez): Are we interested at all on using the
  // PermissionRequestManager in the ContentShell ?
  if (registry->registration_mode() ==
      custom_handlers::RphRegistrationMode::kAutoAccept) {
    registry->OnAcceptRegisterProtocolHandler(handler);
  }
}

bool TestShell::DidAddMessageToConsole(
    WebContents* source,
    blink::mojom::ConsoleMessageLevel log_level,
    const std::u16string& message,
    int32_t line_no,
    const std::u16string& source_id) {
  return switches::IsRunWebTestsSwitchPresent();
}

PictureInPictureResult TestShell::EnterPictureInPicture(
    WebContents* web_contents) {
  // During tests, returning success to pretend the window was created and allow
  // tests to run accordingly.
  if (switches::IsRunWebTestsSwitchPresent()) {
    return PictureInPictureResult::kSuccess;
  }
  return PictureInPictureResult::kNotSupported;
}

void TestShell::SetContentsBounds(WebContents* source,
                                  const gfx::Rect& bounds) {
  DCHECK(source == web_contents());  // There's only one WebContents per Shell.

  if (switches::IsRunWebTestsSwitchPresent()) {
    // Note that chrome drops these requests on normal windows.
    // TODO(danakj): The position is dropped here but we use the size. Web tests
    // can't move the window in headless mode anyways, but maybe we should be
    // letting them pretend?
    TestShell::GetPlatform()->ResizeWebContent(this, bounds.size());
  }
}

}  // namespace content
