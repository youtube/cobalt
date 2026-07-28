#include <memory>
#include <optional>

#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/renderer/core/inspector/worker_inspector_controller.h"
#include "third_party/blink/renderer/core/inspector/worker_devtools_params.h"
#include "third_party/blink/renderer/core/inspector/devtools_agent.h"
#include "third_party/blink/renderer/core/inspector/devtools_session.h"
#include "third_party/blink/renderer/core/inspector/inspector_animation_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_css_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_debugger_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_dom_snapshot_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_emulation_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_event_breakpoints_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_layer_tree_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_log_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_media_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_media_context_impl.h"
#include "third_party/blink/renderer/core/inspector/inspector_network_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_overlay_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_page_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_performance_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_performance_timeline_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_preload_agent.h"
#include "third_party/blink/renderer/core/inspector/main_thread_debugger.h"
#include "third_party/blink/renderer/core/inspector/worker_thread_debugger.h"
#include "third_party/blink/renderer/core/inspector/protocol/network.h"
#include "third_party/blink/renderer/core/exported/web_dev_tools_agent_impl.h"

namespace blink {

MainThreadDebugger::MainThreadDebugger(v8::Isolate* isolate)
    : ThreadDebuggerCommonImpl(isolate) {}
MainThreadDebugger::~MainThreadDebugger() = default;

MainThreadDebugger* MainThreadDebugger::Instance(v8::Isolate*) { return nullptr; }
void MainThreadDebugger::ContextWillBeDestroyed(ScriptState*) {}
void MainThreadDebugger::ContextCreated(ScriptState*, LocalFrame*, const SecurityOrigin*) {}
void MainThreadDebugger::runMessageLoopOnPause(int) {}
void MainThreadDebugger::runMessageLoopOnInstrumentationPause(int) {}
void MainThreadDebugger::quitMessageLoopOnPause() {}
void MainThreadDebugger::runIfWaitingForDebugger(int) {}
void MainThreadDebugger::muteMetrics(int) {}
void MainThreadDebugger::unmuteMetrics(int) {}
v8::Local<v8::Context> MainThreadDebugger::ensureDefaultContextInGroup(int) { return {}; }
void MainThreadDebugger::beginEnsureAllContextsInGroup(int) {}
void MainThreadDebugger::endEnsureAllContextsInGroup(int) {}
void MainThreadDebugger::installAdditionalCommandLineAPI(v8::Local<v8::Context>, v8::Local<v8::Object>) {}
void MainThreadDebugger::consoleAPIMessage(int, v8::Isolate::MessageErrorLevel, const v8_inspector::StringView&, const v8_inspector::StringView&, unsigned, unsigned, v8_inspector::V8StackTrace*) {}
v8::MaybeLocal<v8::Value> MainThreadDebugger::memoryInfo(v8::Isolate*, v8::Local<v8::Context>) { return {}; }
void MainThreadDebugger::consoleClear(int) {}
bool MainThreadDebugger::canExecuteScripts(int) { return true; }
int MainThreadDebugger::ContextGroupId(ExecutionContext*) { return 0; }
int MainThreadDebugger::ContextGroupId(LocalFrame*) { return 0; }
void MainThreadDebugger::ReportConsoleMessage(ExecutionContext*, mojom::ConsoleMessageSource, mojom::ConsoleMessageLevel, const WTF::String&, SourceLocation*) {}
void MainThreadDebugger::DidClearContextsForFrame(LocalFrame*) {}
void MainThreadDebugger::ExceptionThrown(ExecutionContext*, ErrorEvent*) {}

} // namespace blink
