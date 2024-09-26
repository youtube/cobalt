// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEBUI_WEB_UI_IOS_IMPL_H_
#define IOS_WEB_WEBUI_WEB_UI_IOS_IMPL_H_

#include <map>
#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#import "ios/web/public/web_state.h"
#include "ios/web/public/webui/web_ui_ios.h"

namespace web {

class WebUIIOSImpl : public web::WebUIIOS,
                     public base::SupportsWeakPtr<WebUIIOSImpl> {
 public:
  explicit WebUIIOSImpl(WebState* web_state);

  WebUIIOSImpl(const WebUIIOSImpl&) = delete;
  WebUIIOSImpl& operator=(const WebUIIOSImpl&) = delete;

  ~WebUIIOSImpl() override;

  // WebUIIOS implementation:
  WebState* GetWebState() const override;
  WebUIIOSController* GetController() const override;
  void SetController(std::unique_ptr<WebUIIOSController> controller) override;
  void AddMessageHandler(
      std::unique_ptr<WebUIIOSMessageHandler> handler) override;
  void RegisterMessageCallback(base::StringPiece message,
                               MessageCallback callback) override;
  void ProcessWebUIIOSMessage(const GURL& source_url,
                              base::StringPiece message,
                              const base::Value::List& args) override;
  void CallJavascriptFunction(base::StringPiece function_name,
                              base::span<const base::ValueView> args) override;
  void ResolveJavascriptCallback(const base::ValueView callback_id,
                                 const base::ValueView response) override;
  void RejectJavascriptCallback(const base::ValueView callback_id,
                                const base::ValueView response) override;
  void FireWebUIListenerSpan(base::span<const base::ValueView> values) override;

 private:
  // Executes JavaScript asynchronously on the page.
  void ExecuteJavascript(const std::u16string& javascript);

  // A map of message name -> message handling callback.
  using MessageCallbackMap =
      std::map<std::string, MessageCallback, std::less<>>;
  MessageCallbackMap message_callbacks_;

  // The WebUIIOSMessageHandlers we own.
  std::vector<std::unique_ptr<WebUIIOSMessageHandler>> handlers_;

  // Non-owning pointer to the WebState this WebUIIOS is associated with.
  WebState* web_state_;

  std::unique_ptr<WebUIIOSController> controller_;
};

}  // namespace web

#endif  // IOS_WEB_WEBUI_WEB_UI_IOS_IMPL_H_
