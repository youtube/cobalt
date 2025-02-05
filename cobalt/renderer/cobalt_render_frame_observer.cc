// Copyright 2025 The Cobalt Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/renderer/cobalt_render_frame_observer.h"

#include "cobalt/configuration/configuration.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/renderer/chrome_object_extensions_utils.h"
#include "content/public/renderer/render_frame.h"
#include "gin/converter.h"
#include "starboard/system.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "v8/include/v8.h"

namespace cobalt {

namespace {

void PerformUserOnExitStrategy() {
  auto exit_strategy =
      configuration::Configuration::GetInstance()->CobaltUserOnExitStrategy();
  // By checking the exit strategy here, the client can call |window.close()|
  // without checking what the exit strategy is. The client does differentiate
  // between |kUserOnExitStrategyClose| and the other exit strategies. For
  // |kUserOnExitStrategyClose|, the player is stopped. For this reason, we may
  // still need to expose |window.h5vcc.system.userOnExitStrategy| to support
  // this.
  //
  // Our |window.close()| deviates from the spec's original intent, but it is an
  // existing function on |window|. If a window is opened with |window.open()|,
  // the return is a new window (like a tab in Chrome) and this new window can
  // be closed by calling |close()|. To check if the window was closed, the
  // |closed| property can be checked.
  //
  // In our case, |window.close()| is being used to conceal or stop the
  // application.
  //
  // |window.close()| can continue to be used. Arguments passed to
  // |window.close()| in the original implementation are ignored. In our custom
  // implementation, we can check for closures to run before no-exit, conceal,
  // or exit. They could be passed in an object like
  //
  // window.close({beforeExit: doBeforeExitOps, beforeNoExit, beforeConceal});
  //
  // This would allow us to remove |window.h5vcc.system.userOnExitStrategy|.
  // And this seems reasonable considering the |window.close()| is a custom
  // implementation that does not align with spec's original intent.
  if (exit_strategy ==
      configuration::Configuration::UserOnExitStrategy::kMinimize) {
    SbSystemRequestConceal();
    return;
  }

  if (exit_strategy ==
      configuration::Configuration::UserOnExitStrategy::kClose) {
    SbSystemRequestStop(0);
    return;
  }
}

}  // namespace

CobaltRenderFrameObserver::CobaltRenderFrameObserver(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      user_on_exit_strategy_callback_(
          base::BindRepeating(&PerformUserOnExitStrategy)) {}

void CobaltRenderFrameObserver::DidCreateScriptContext(
    v8::Local<v8::Context> /* v8_context */,
    int32_t world_id) {
  // Only install on the main world context of the main frame.
  if (!render_frame() || !render_frame()->IsMainFrame() ||
      world_id != content::ISOLATED_WORLD_ID_GLOBAL) {
    return;
  }

  InstallCloseAndMinimize();
}

// static
void CobaltRenderFrameObserver::DoWindowCloseOrMinimize(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Value> data = info.Data();
  CobaltRenderFrameObserver* self = static_cast<CobaltRenderFrameObserver*>(
      v8::External::Cast(*data)->Value());
  self->user_on_exit_strategy_callback_.Run();
}

void CobaltRenderFrameObserver::InstallCloseAndMinimize() {
  if (!render_frame()) {
    return;
  }
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      render_frame()->GetWebFrame()->MainWorldScriptContext();
  if (context.IsEmpty()) {
    return;
  }

  v8::Context::Scope context_scope(context);
  v8::Local<v8::Object> window = context->Global();
  v8::Local<v8::External> v8_this = v8::External::New(isolate, this);
  window
      ->Set(context, gin::StringToSymbol(isolate, "minimize"),
            v8::Function::New(
                context, &CobaltRenderFrameObserver::DoWindowCloseOrMinimize,
                v8_this)
                .ToLocalChecked())
      .Check();
  window
      ->Set(context, gin::StringToSymbol(isolate, "close"),
            v8::Function::New(
                context, &CobaltRenderFrameObserver::DoWindowCloseOrMinimize,
                v8_this)
                .ToLocalChecked())
      .Check();
  v8::Local<v8::Object> h5vcc =
      content::GetOrCreateObject(isolate, context, "h5vcc");
  v8::Local<v8::Object> h5vcc_system =
      content::GetOrCreateObject(isolate, context, h5vcc, "system");
  auto exit_strategy =
      configuration::Configuration::GetInstance()->CobaltUserOnExitStrategy();
  h5vcc_system->Set(
      context, gin::StringToSymbol(isolate, "userOnExitStrategy"),
      v8::Integer::New(isolate, static_cast<int32_t>(exit_strategy)));
}

void CobaltRenderFrameObserver::OnDestruct() {
  delete this;
}

}  // namespace cobalt
