// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_UI_BROWSER_INTERFACE_BROKER_REGISTRY_H_
#define CONTENT_PUBLIC_BROWSER_WEB_UI_BROWSER_INTERFACE_BROKER_REGISTRY_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/per_web_ui_browser_interface_broker.h"
#include "content/public/browser/web_ui_controller.h"

namespace content {

// A lightweight class to help interface registration. Shouldn't be used outside
// of registration process.
template <typename ControllerType>
class InterfaceRegistrationHelper {
 public:
  explicit InterfaceRegistrationHelper(
      std::vector<BinderInitializer>* binder_initializers)
      : binder_initializers_(binder_initializers) {}
  ~InterfaceRegistrationHelper() = default;

  template <typename Interface>
  InterfaceRegistrationHelper<ControllerType>& Add() {
    // Insert a function that populates an interface broker instance's
    // binder_map.
    binder_initializers_->push_back(
        base::BindRepeating([](WebUIBinderMap* binder_map) {
          binder_map->Add<Interface>(base::BindRepeating(
              [](WebUIController* controller,
                 mojo::PendingReceiver<Interface> receiver) {
                auto* concrete_controller = controller->GetAs<ControllerType>();

                DCHECK(concrete_controller)
                    << "The requesting WebUIController is of a different type.";

                concrete_controller->BindInterface(std::move(receiver));
              }));
        }));
    return *this;
  }

 private:
  raw_ptr<std::vector<BinderInitializer>> binder_initializers_;
};

// Maintains a mapping from WebUIController::Type to a list of interfaces
// exposed to MojoJS, and provides methods to set up an interface broker that
// only brokers the registered interfaces for the WebUIController.
//
// To register interfaces for WebUI, use the following code:
//
// registry.ForWebUI<ControllerType>
//    .Add<Interface1>()
//    .Add<Interface2>();
class CONTENT_EXPORT WebUIBrowserInterfaceBrokerRegistry {
 public:
  WebUIBrowserInterfaceBrokerRegistry();
  ~WebUIBrowserInterfaceBrokerRegistry();
  WebUIBrowserInterfaceBrokerRegistry(
      const WebUIBrowserInterfaceBrokerRegistry&) = delete;
  WebUIBrowserInterfaceBrokerRegistry& operator=(
      const WebUIBrowserInterfaceBrokerRegistry&) = delete;

  template <typename ControllerType>
  InterfaceRegistrationHelper<ControllerType> ForWebUI() {
    // WebUIController::GetType() requires a instantiated WebUIController
    // (because it's a virtual method and can't be static). Here we only have
    // type information, so we need to figure out the type from the controller's
    // class declaration.
    WebUIController::Type type = &ControllerType::kWebUIControllerType;

    DCHECK(binder_initializers_.count(type) == 0)
        << "Interfaces for a WebUI should be registered together.";

    return InterfaceRegistrationHelper<ControllerType>(
        &binder_initializers_[type]);
  }

  // Creates an unbounded interface broker for |controller|. Caller should call
  // Bind() method on the returned broker with a PendingReceiver that receives
  // MojoJS.bindInterface requests from the renderer. Returns nullptr if
  // controller doesn't have registered interface broker initializers.
  std::unique_ptr<PerWebUIBrowserInterfaceBroker> CreateInterfaceBroker(
      WebUIController& controller);

  // Add interface |binder| to all WebUIs registered here.
  //
  // This method should only be used in tests. This method should be called
  // after ContentBrowserClient::RegisterWebUIInterfaceBrokers and before WebUIs
  // being created.
  template <typename Interface>
  void AddBinderForTesting(
      base::RepeatingCallback<void(WebUIController*,
                                   mojo::PendingReceiver<Interface>)> binder) {
    for (auto& it : binder_initializers_) {
      it.second.push_back(base::BindRepeating(
          [](decltype(binder) binder, WebUIBinderMap* binder_map) {
            binder_map->Add<Interface>(binder);
          },
          binder));
    }
  }

 private:
  std::map<WebUIController::Type, std::vector<BinderInitializer>>
      binder_initializers_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_UI_BROWSER_INTERFACE_BROKER_REGISTRY_H_
