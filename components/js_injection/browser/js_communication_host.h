// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_JS_INJECTION_BROWSER_JS_COMMUNICATION_HOST_H_
#define COMPONENTS_JS_INJECTION_BROWSER_JS_COMMUNICATION_HOST_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/js_injection/common/interfaces.mojom.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace js_injection {

class OriginMatcher;
struct DocumentStartJavaScript;
struct JsObject;
class JsToBrowserMessaging;
class WebMessageHostFactory;

// This class is 1:1 with WebContents, when AddWebMessageListener() is called,
// it stores the information in this class and send them to renderer side
// JsCommunication if there is any. When RenderFrameCreated() gets called, it
// needs to configure that new RenderFrame with the information stores in this
// class.
class JsCommunicationHost : public content::WebContentsObserver {
 public:
  explicit JsCommunicationHost(content::WebContents* web_contents);

  JsCommunicationHost(const JsCommunicationHost&) = delete;
  JsCommunicationHost& operator=(const JsCommunicationHost&) = delete;

  ~JsCommunicationHost() override;

  // Captures the result of adding script. There are two possibilities when
  // adding script: there was an error, in which case |error_message| is set,
  // otherwise the add was successful and |script_id| is set.
  struct AddScriptResult {
    AddScriptResult();
    AddScriptResult(const AddScriptResult&);
    AddScriptResult& operator=(const AddScriptResult&);
    ~AddScriptResult();

    absl::optional<std::string> error_message;
    absl::optional<int> script_id;
  };

  // Native side AddDocumentStartJavaScript, returns an error message if the
  // parameters didn't pass necessary checks.
  AddScriptResult AddDocumentStartJavaScript(
      const std::u16string& script,
      const std::vector<std::string>& allowed_origin_rules);

  bool RemoveDocumentStartJavaScript(int script_id);

  // Adds a new WebMessageHostFactory. For any urls that match
  // |allowed_origin_rules|, |js_object_name| is registered as a JS object that
  // can be used by script on the page to send and receive messages. Returns
  // an empty string on success. On failure, the return string gives the error
  // message.
  std::u16string AddWebMessageHostFactory(
      std::unique_ptr<WebMessageHostFactory> factory,
      const std::u16string& js_object_name,
      const std::vector<std::string>& allowed_origin_rules);

  // Returns the factory previously registered under the specified name.
  void RemoveWebMessageHostFactory(const std::u16string& js_object_name);

  struct RegisteredFactory {
    std::u16string js_name;
    OriginMatcher allowed_origin_rules;
    raw_ptr<WebMessageHostFactory> factory = nullptr;
  };

  // Returns the registered factories.
  std::vector<RegisteredFactory> GetWebMessageHostFactories();

  // content::WebContentsObserver implementations
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameHostStateChanged(
      content::RenderFrameHost* render_frame_host,
      content::RenderFrameHost::LifecycleState old_state,
      content::RenderFrameHost::LifecycleState new_state) override;

 private:
  void NotifyFrameForWebMessageListener(
      content::RenderFrameHost* render_frame_host);
  void NotifyFrameForAllDocumentStartJavaScripts(
      content::RenderFrameHost* render_frame_host);
  void NotifyFrameForAddDocumentStartJavaScript(
      const DocumentStartJavaScript* script,
      content::RenderFrameHost* render_frame_host);
  void NotifyFrameForRemoveDocumentStartJavaScript(
      int32_t script_id,
      content::RenderFrameHost* render_frame_host);

  int32_t next_script_id_ = 0;
  std::vector<DocumentStartJavaScript> scripts_;
  std::vector<std::unique_ptr<JsObject>> js_objects_;
  std::map<content::GlobalRenderFrameHostId,
           std::vector<std::unique_ptr<JsToBrowserMessaging>>>
      js_to_browser_messagings_;
};

}  // namespace js_injection

#endif  // COMPONENTS_JS_INJECTION_BROWSER_JS_COMMUNICATION_HOST_H_
