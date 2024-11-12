// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SYSTEM_INFO_SYSTEM_INFO_API_H_
#define EXTENSIONS_BROWSER_API_SYSTEM_INFO_SYSTEM_INFO_API_H_

#include "base/memory/raw_ptr.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"

namespace extensions {

// A Profile-scoped object which is registered as an observer of EventRouter
// to observe the systemInfo event listener arrival/removal.
class SystemInfoAPI : public BrowserContextKeyedAPI,
                      public EventRouter::Observer {
 public:
  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<SystemInfoAPI>* GetFactoryInstance();

  explicit SystemInfoAPI(content::BrowserContext* context);

  SystemInfoAPI(const SystemInfoAPI&) = delete;
  SystemInfoAPI& operator=(const SystemInfoAPI&) = delete;

  ~SystemInfoAPI() override;

  // KeyedService implementation.
  void Shutdown() override;

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;
  void OnListenerRemoved(const EventListenerInfo& details) override;

 private:
  friend class BrowserContextKeyedAPIFactory<SystemInfoAPI>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "SystemInfoAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;

  raw_ptr<content::BrowserContext> browser_context_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SYSTEM_INFO_SYSTEM_INFO_API_H_
