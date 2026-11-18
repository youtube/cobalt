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

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "cobalt/browser/lifecycle/cobalt_lifecycle_manager.h"
#include "cobalt/shell/browser/shell.h"
#include "cobalt/testing/browser_tests/browser/test_shell.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom-shared.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-external.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-microtask-queue.h"
#include "v8/include/v8-primitive.h"

namespace cobalt {
namespace {

std::unique_ptr<net::test_server::HttpResponse> HandleInlinePageRequest(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != "/page1.html" &&
      request.relative_url != "/page2.html") {
    return nullptr;
  }
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content_type("text/html");
  response->set_content(
      "<!doctype html><html><head><title>Lifecycle</title></head>"
      "<body></body></html>");
  return response;
}

// Renderer-side helper that installs a pagehide hook to toggle WebView
// visibility during navigation (queuing TaskType::kDOMManipulation tasks on the
// outgoing CobaltLifecycleController) and posts a high-priority frame task on
// DidCommitProvisionalLoad to collect the detached controller before those
// queued tasks execute.
//
// Lifetime and ownership: Heap-allocated and self-owned via
// content::RenderFrameObserver; deletes itself in OnDestruct().
// Threading: Constructed, used, and destroyed exclusively on the in-process
// renderer main thread.
class LifecycleDuringCommitObserver : public content::RenderFrameObserver {
 public:
  explicit LifecycleDuringCommitObserver(content::RenderFrame* render_frame)
      : content::RenderFrameObserver(render_frame) {}
  ~LifecycleDuringCommitObserver() override = default;

  void InstallPagehideVisibilityTrigger() {
    blink::WebLocalFrame* web_frame = render_frame()->GetWebFrame();
    if (!web_frame) {
      return;
    }
    v8::Isolate* isolate = web_frame->GetAgentGroupScheduler()->Isolate();
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Context> context = web_frame->MainWorldScriptContext();
    if (context.IsEmpty()) {
      return;
    }
    v8::Context::Scope context_scope(context);
    v8::MicrotasksScope microtasks_scope(
        context, v8::MicrotasksScope::kDoNotRunMicrotasks);
    v8::Local<v8::Function> fn =
        v8::Function::New(context, &OnPagehideCallback,
                          v8::External::New(isolate, this))
            .ToLocalChecked();
    context->Global()
        ->Set(context,
              v8::String::NewFromUtf8Literal(isolate,
                                             "__queueVisibilityDuringPagehide"),
              fn)
        .Check();
  }

  // Posts a high-priority frame task so Oilpan sweeps the detached outgoing
  // CobaltLifecycleController at the top of the Blink event loop before
  // lower-priority TaskType::kDOMManipulation tasks execute.
  void DidCommitProvisionalLoad(ui::PageTransition transition) override {
    if (!queued_during_pagehide_) {
      return;
    }
    queued_during_pagehide_ = false;
    render_frame()
        ->GetTaskRunner(blink::TaskType::kInternalHighPriorityLocalFrame)
        ->PostTask(
            FROM_HERE,
            base::BindOnce(&blink::WebHeap::CollectAllGarbageForTesting));
  }

  void OnDestruct() override { delete this; }

 private:
  static void OnPagehideCallback(
      const v8::FunctionCallbackInfo<v8::Value>& info) {
    auto* self = static_cast<LifecycleDuringCommitObserver*>(
        info.Data().As<v8::External>()->Value());
    if (!self || !self->render_frame()) {
      return;
    }
    blink::WebLocalFrame* web_frame = self->render_frame()->GetWebFrame();
    if (!web_frame || !web_frame->View()) {
      return;
    }
    self->queued_during_pagehide_ = true;
    blink::WebView* view = web_frame->View();
    view->SetVisibilityState(blink::mojom::PageVisibilityState::kHidden,
                             /*is_initial_state=*/false);
    view->SetVisibilityState(blink::mojom::PageVisibilityState::kVisible,
                             /*is_initial_state=*/false);
  }

  bool queued_during_pagehide_ = false;
};

// Scoped browser-side observer that blocks a RunLoop until
// CobaltLifecycleManager reports OnAllFramesConcealed for the target
// WebContents.
//
// Lifetime and ownership: Stack-allocated per test check; registers and
// unregisters with CobaltLifecycleManager in constructor and destructor.
// Threading: Used exclusively on the browser UI main thread.
class VisibilityAckWaiter : public CobaltLifecycleManagerObserver {
 public:
  explicit VisibilityAckWaiter(content::WebContents* web_contents)
      : web_contents_(web_contents) {
    CobaltLifecycleManager::GetInstance()->AddObserver(this);
  }
  ~VisibilityAckWaiter() override {
    CobaltLifecycleManager::GetInstance()->RemoveObserver(this);
  }

  void WaitForConcealed() { concealed_loop_.Run(); }

  void OnAllFramesConcealed(content::WebContents* web_contents) override {
    if (web_contents == web_contents_) {
      concealed_loop_.Quit();
    }
  }

  void OnAllFramesVisible(content::WebContents* web_contents) override {}

 private:
  content::WebContents* const web_contents_;
  base::RunLoop concealed_loop_;
};

}  // namespace

// Browser test fixture exercising renderer CobaltLifecycleController Oilpan
// garbage collection across cross-document navigations in single-process mode.
//
// Lifetime and ownership: Instantiated and owned by GTest per test case.
// Threading: Fixture setup and test body run on the browser main thread.
class CobaltLifecycleBrowserTest : public content::ContentBrowserTest {
 public:
  CobaltLifecycleBrowserTest() = default;
  ~CobaltLifecycleBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::ContentBrowserTest::SetUpCommandLine(command_line);
    if (!command_line->HasSwitch(switches::kSingleProcess)) {
      command_line->AppendSwitch(switches::kSingleProcess);
    }
    const std::string existing_js_flags =
        command_line->GetSwitchValueASCII(blink::switches::kJavaScriptFlags);
    if (existing_js_flags.empty()) {
      command_line->AppendSwitchASCII(blink::switches::kJavaScriptFlags,
                                      "--expose-gc");
    } else if (existing_js_flags.find("--expose-gc") == std::string::npos) {
      command_line->RemoveSwitch(blink::switches::kJavaScriptFlags);
      command_line->AppendSwitchASCII(blink::switches::kJavaScriptFlags,
                                      existing_js_flags + ",--expose-gc");
    }
  }

  void TearDownOnMainThread() override {
    CobaltLifecycleManager::GetInstance()->ResetForTesting();
    content::ContentBrowserTest::TearDownOnMainThread();
  }
};

// Covers the user journey where page visibility changes during a same-origin
// cross-document navigation right before Blink garbage collection sweeps the
// outgoing window, verifying that stale callbacks do not crash the renderer
// and that lifecycle ACKs continue working on the newly committed document.
IN_PROC_BROWSER_TEST_F(
    CobaltLifecycleBrowserTest,
    VisibilityTransitionDuringNavigationAndGarbageCollection) {
  embedded_test_server()->RegisterRequestHandler(
      base::BindRepeating(&HandleInlinePageRequest));
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url1 = embedded_test_server()->GetURL("/page1.html");
  const GURL url2 = embedded_test_server()->GetURL("/page2.html");

  CobaltLifecycleManager::GetInstance()->InitializeTracker(
      shell()->web_contents());
  ASSERT_TRUE(content::NavigateToURL(shell()->web_contents(), url1));
  EXPECT_EQ("visible",
            content::EvalJs(shell()->web_contents(), "document.visibilityState")
                .ExtractString());

  const blink::LocalFrameToken frame_token =
      shell()->web_contents()->GetPrimaryMainFrame()->GetFrameToken();
  scoped_refptr<base::SingleThreadTaskRunner> renderer_task_runner = content::
      RenderProcessHostImpl::GetInProcessRendererThreadTaskRunnerForTesting();
  ASSERT_NE(renderer_task_runner, nullptr);

  base::RunLoop attach_loop;
  renderer_task_runner->PostTaskAndReply(
      FROM_HERE, base::BindLambdaForTesting([frame_token]() {
        blink::WebLocalFrame* web_frame =
            blink::WebLocalFrame::FromFrameToken(frame_token);
        ASSERT_NE(web_frame, nullptr);
        content::RenderFrame* rf =
            content::RenderFrame::FromWebFrame(web_frame);
        ASSERT_NE(rf, nullptr);
        auto* observer = new LifecycleDuringCommitObserver(rf);
        observer->InstallPagehideVisibilityTrigger();
      }),
      attach_loop.QuitClosure());
  attach_loop.Run();

  ASSERT_TRUE(content::ExecJs(shell()->web_contents(),
                              "window.addEventListener('pagehide', () => "
                              "window.__queueVisibilityDuringPagehide());"));

  ASSERT_TRUE(content::NavigateToURL(shell()->web_contents(), url2));
  EXPECT_EQ("visible",
            content::EvalJs(shell()->web_contents(), "document.visibilityState")
                .ExtractString());

  VisibilityAckWaiter waiter(shell()->web_contents());
  CobaltLifecycleManager::GetInstance()->StartWaitingForAck(
      shell()->web_contents(), PendingAck::kConceal);
  shell()->web_contents()->WasHidden();
  waiter.WaitForConcealed();
}

}  // namespace cobalt
