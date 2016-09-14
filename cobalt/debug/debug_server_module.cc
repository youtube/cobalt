/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/debug/debug_server_module.h"

#include "cobalt/debug/render_layer.h"

namespace cobalt {
namespace debug {

DebugServerModule::DebugServerModule(
    dom::Console* console, script::GlobalEnvironment* global_environment,
    RenderOverlay* render_overlay,
    render_tree::ResourceProvider* resource_provider, dom::Window* window) {
  ConstructionData data(console, global_environment, MessageLoop::current(),
                        render_overlay, resource_provider, window);
  Build(data);
}

DebugServerModule::DebugServerModule(
    dom::Console* console, script::GlobalEnvironment* global_environment,
    RenderOverlay* render_overlay,
    render_tree::ResourceProvider* resource_provider, dom::Window* window,
    MessageLoop* message_loop) {
  ConstructionData data(console, global_environment, message_loop,
                        render_overlay, resource_provider, window);
  Build(data);
}

DebugServerModule::~DebugServerModule() { script_debugger_->Detach(); }

void DebugServerModule::Build(const ConstructionData& data) {
  DCHECK(data.message_loop);

  if (MessageLoop::current() == data.message_loop) {
    BuildInternal(data, NULL);
  } else {
    base::WaitableEvent created(true, false);
    data.message_loop->PostTask(
        FROM_HERE,
        base::Bind(&DebugServerModule::BuildInternal, base::Unretained(this),
                   data, base::Unretained(&created)));
    created.Wait();
  }

  DCHECK(debug_server_);
}

void DebugServerModule::BuildInternal(const ConstructionData& data,
                                      base::WaitableEvent* created) {
  DCHECK(MessageLoop::current() == data.message_loop);
  DCHECK(data.console);
  DCHECK(data.global_environment);
  DCHECK(data.render_overlay);
  DCHECK(data.resource_provider);
  DCHECK(data.window);

  // Create the debug server itself.
  debug_server_.reset(new debug::DebugServer(
      data.global_environment, data.window->document()->csp_delegate()));

  // Create render layers for the components that need them and chain them
  // together. Ownership will be passed to the component that uses each layer.
  // The layers will be painted in the reverse order they are listed here.
  scoped_ptr<RenderLayer> page_render_layer(new RenderLayer(base::Bind(
      &RenderOverlay::SetOverlay, base::Unretained(data.render_overlay))));

  scoped_ptr<RenderLayer> dom_render_layer(new RenderLayer(
      base::Bind(&RenderLayer::SetBackLayer, page_render_layer->AsWeakPtr())));

  // Create the script debugger. This is owned by this object, and is
  // accessible to all the debugger components.
  script_debugger_ =
      script::ScriptDebugger::CreateDebugger(data.global_environment, this);

  // Create the connector object that provides functionality for each
  // component to interact with the debug server, script debugger, etc.
  component_connector_.reset(
      new ComponentConnector(debug_server_.get(), script_debugger_.get()));

  // Create the various components that implement the functionality of the
  // debugger by handling commands and sending event notifications.
  console_component_.reset(
      new ConsoleComponent(component_connector_.get(), data.console));

  dom_component_.reset(
      new DOMComponent(component_connector_.get(), dom_render_layer.Pass()));

  page_component_.reset(new PageComponent(component_connector_.get(),
                                          data.window, page_render_layer.Pass(),
                                          data.resource_provider));

  runtime_component_.reset(new RuntimeComponent(component_connector_.get()));

  script_debugger_component_.reset(
      new JavaScriptDebuggerComponent(component_connector_.get()));

  if (created) {
    created->Signal();
  }
}

void DebugServerModule::OnScriptDebuggerDetach(const std::string& reason) {
  DCHECK(script_debugger_component_);
  script_debugger_component_->OnScriptDebuggerDetach(reason);
}

void DebugServerModule::OnScriptDebuggerPause(
    scoped_ptr<script::CallFrame> call_frame) {
  DCHECK(script_debugger_component_);
  script_debugger_component_->OnScriptDebuggerPause(call_frame.Pass());
}

void DebugServerModule::OnScriptFailedToParse(
    scoped_ptr<script::SourceProvider> source_provider) {
  DCHECK(script_debugger_component_);
  script_debugger_component_->OnScriptFailedToParse(source_provider.Pass());
}

void DebugServerModule::OnScriptParsed(
    scoped_ptr<script::SourceProvider> source_provider) {
  DCHECK(script_debugger_component_);
  script_debugger_component_->OnScriptParsed(source_provider.Pass());
}

}  // namespace debug
}  // namespace cobalt
