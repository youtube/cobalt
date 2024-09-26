// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_EXTENSION_FRAME_HELPER_H_
#define EXTENSIONS_RENDERER_EXTENSION_FRAME_HELPER_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "extensions/common/mojom/frame.mojom.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "v8/include/v8-forward.h"

struct ExtensionMsg_OnConnectData;

namespace extensions {

class Dispatcher;
struct Message;
struct PortId;
class ScriptContext;

// RenderFrame-level plumbing for extension features.
class ExtensionFrameHelper
    : public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<ExtensionFrameHelper>,
      public mojom::LocalFrame {
 public:
  ExtensionFrameHelper(content::RenderFrame* render_frame,
                       Dispatcher* extension_dispatcher);

  ExtensionFrameHelper(const ExtensionFrameHelper&) = delete;
  ExtensionFrameHelper& operator=(const ExtensionFrameHelper&) = delete;

  ~ExtensionFrameHelper() override;

  // Returns a list of extension RenderFrames that match the given filter
  // criteria. A |browser_window_id| of extension_misc::kUnknownWindowId
  // specifies "all", as does a |view_type| of mojom::ViewType::kInvalid.
  static std::vector<content::RenderFrame*> GetExtensionFrames(
      const std::string& extension_id,
      int browser_window_id,
      int tab_id,
      mojom::ViewType view_type);
  // Same as above, but returns a v8::Array of the v8 global objects for those
  // frames, and only includes outermost main frames. Note: This only returns
  // contexts that are accessible by |context|, and |context| must be the
  // current context.
  // Returns an empty v8::Array if no frames are found.
  static v8::Local<v8::Array> GetV8MainFrames(v8::Local<v8::Context> context,
                                              const std::string& extension_id,
                                              int browser_window_id,
                                              int tab_id,
                                              mojom::ViewType view_type);

  // Returns the main frame of the extension's background page, or null if there
  // isn't one in this process.
  static content::RenderFrame* GetBackgroundPageFrame(
      const std::string& extension_id);
  // Same as above, but returns the background page's main frame, or
  // v8::Undefined if there is none. Note: This will assert that the
  // isolate's current context can access the returned object; callers should
  // ensure that the current context is correct.
  static v8::Local<v8::Value> GetV8BackgroundPageMainFrame(
      v8::Isolate* isolate,
      const std::string& extension_id);

  // Finds a neighboring extension frame with the same extension as the one
  // owning |relative_to_frame| (if |relative_to_frame| is not an extension
  // frame, returns nullptr). Pierces the browsing instance boundary because
  // certain extensions rely on this behavior.
  // TODO(devlin, lukasza): https://crbug.com/786411: Remove this behavior, and
  // make extensions follow the web standard for finding frames or use an
  // explicit API.
  static content::RenderFrame* FindFrame(
      content::RenderFrame* relative_to_frame,
      const std::string& name);

  // Returns true if the given |context| is for any frame in the extension's
  // event page.
  // TODO(devlin): This isn't really used properly, and should probably be
  // deleted.
  static bool IsContextForEventPage(const ScriptContext* context);

  mojom::ViewType view_type() const { return view_type_; }
  int tab_id() const { return tab_id_; }
  int browser_window_id() const { return browser_window_id_; }
  bool did_create_current_document_element() const {
    return did_create_current_document_element_;
  }

  // mojom::LocalFrame:
  void SetFrameName(const std::string& name) override;
  void SetSpatialNavigationEnabled(bool enabled) override;
  void SetTabId(int32_t id) override;
  void AppWindowClosed(bool send_onclosed) override;
  void NotifyRenderViewType(mojom::ViewType view_type) override;
  void MessageInvoke(const std::string& extension_id,
                     const std::string& module_name,
                     const std::string& function_name,
                     base::Value::List args) override;

  void ExecuteCode(mojom::ExecuteCodeParamsPtr param,
                   ExecuteCodeCallback callback) override;

  void ExecuteDeclarativeScript(int32_t tab_id,
                                const std::string& extension_id,
                                const std::string& script_id,
                                const GURL& url) override;

  void UpdateBrowserWindowId(int32_t window_id) override;

  void NotifyDidCreateScriptContext(int32_t world_id);
  bool did_create_script_context() const { return did_create_script_context_; }

  // Called when the document element has been inserted in this frame. This
  // method may invoke untrusted JavaScript code that invalidate the frame and
  // this ExtensionFrameHelper.
  void RunScriptsAtDocumentStart();

  // Called after the DOMContentLoaded event has fired.
  void RunScriptsAtDocumentEnd();

  // Called before the window.onload event is fired.
  void RunScriptsAtDocumentIdle();

  // Schedule a callback, to be run at the next RunScriptsAtDocumentStart
  // notification. Only call this when you are certain that there will be such a
  // notification, e.g. from RenderFrameObserver::DidCreateDocumentElement.
  // Otherwise the callback is never invoked, or invoked for a document that you
  // were not expecting.
  void ScheduleAtDocumentStart(base::OnceClosure callback);

  // Schedule a callback, to be run at the next RunScriptsAtDocumentEnd call.
  void ScheduleAtDocumentEnd(base::OnceClosure callback);

  // Schedule a callback, to be run at the next RunScriptsAtDocumentIdle call.
  void ScheduleAtDocumentIdle(base::OnceClosure callback);

  mojom::LocalFrameHost* GetLocalFrameHost();

 private:
  void BindLocalFrame(
      mojo::PendingAssociatedReceiver<mojom::LocalFrame> receiver);

  // RenderFrameObserver implementation.
  void DidCreateDocumentElement() override;
  void DidCreateNewDocument() override;
  void ReadyToCommitNavigation(
      blink::WebDocumentLoader* document_loader) override;
  void DidCommitProvisionalLoad(ui::PageTransition transition) override;
  void DidCreateScriptContext(v8::Local<v8::Context>,
                              int32_t world_id) override;
  void WillReleaseScriptContext(v8::Local<v8::Context>,
                                int32_t world_id) override;
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnDestruct() override;
  void DraggableRegionsChanged() override;
  void DidClearWindowObject() override;

  // IPC handlers.
  void OnExtensionValidateMessagePort(int worker_thread_id, const PortId& id);
  void OnExtensionDispatchOnConnect(
      int worker_thread_id,
      const ExtensionMsg_OnConnectData& connect_data);
  void OnExtensionDeliverMessage(int worker_thread_id,
                                 const PortId& target_port_id,
                                 const Message& message);
  void OnExtensionDispatchOnDisconnect(int worker_thread_id,
                                       const PortId& id,
                                       const std::string& error_message);

  // Type of view associated with the RenderFrame.
  mojom::ViewType view_type_ = mojom::ViewType::kInvalid;

  // The id of the tab the render frame is attached to.
  int tab_id_ = -1;

  // The id of the browser window the render frame is attached to.
  int browser_window_id_ = -1;

  Dispatcher* extension_dispatcher_;

  // Whether or not the current document element has been created. This starts
  // true as the initial empty document is already created when this class is
  // instantiated.
  // TODO(danakj): Does this still need to be tracked? We now have consisitent
  // notifications for initial empty documents on all frames.
  bool did_create_current_document_element_ = true;

  // Callbacks to be run at the next RunScriptsAtDocumentStart notification.
  std::vector<base::OnceClosure> document_element_created_callbacks_;

  // Callbacks to be run at the next RunScriptsAtDocumentEnd notification.
  std::vector<base::OnceClosure> document_load_finished_callbacks_;

  // Callbacks to be run at the next RunScriptsAtDocumentIdle notification.
  std::vector<base::OnceClosure> document_idle_callbacks_;

  bool delayed_main_world_script_initialization_ = false;

  // Whether or not a DocumentLoader has been created at least once for this
  // RenderFrame.
  // Note: Chrome Apps intentionally do not support new navigations. When a
  // navigation happens, it is either the initial one or a reload.
  bool has_started_first_navigation_ = false;

  bool did_create_script_context_ = false;
  // Whether we are currently initializing the main world script context.
  bool is_initializing_main_world_script_context_ = false;

  mojo::AssociatedRemote<mojom::LocalFrameHost> local_frame_host_remote_;

  mojo::AssociatedReceiver<mojom::LocalFrame> local_frame_receiver_{this};

  base::WeakPtrFactory<ExtensionFrameHelper> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_EXTENSION_FRAME_HELPER_H_
