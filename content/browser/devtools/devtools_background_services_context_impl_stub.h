// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_BACKGROUND_SERVICES_CONTEXT_IMPL_STUB_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_BACKGROUND_SERVICES_CONTEXT_IMPL_STUB_H_

namespace content {

class BrowserContext;
class ServiceWorkerContextWrapper;

class CONTENT_EXPORT DevToolsBackgroundServicesContextImpl
    : public DevToolsBackgroundServicesContext {
 public:
  DevToolsBackgroundServicesContextImpl(
      BrowserContext* browser_context,
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context);
  ~DevToolsBackgroundServicesContextImpl() override;

  DevToolsBackgroundServicesContextImpl(
      const DevToolsBackgroundServicesContextImpl&) = delete;
  DevToolsBackgroundServicesContextImpl& operator=(
      const DevToolsBackgroundServicesContextImpl&) = delete;

  // DevToolsBackgroundServicesContext overrides:
  bool IsRecording(DevToolsBackgroundService service) override;
  void LogBackgroundServiceEvent(
      uint64_t service_worker_registration_id,
      blink::StorageKey storage_key,
      DevToolsBackgroundService service,
      const std::string& event_name,
      const std::string& instance_id,
      const std::map<std::string, std::string>& event_metadata) override;
};

}  // namespace content


#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_BACKGROUND_SERVICES_CONTEXT_IMPL_STUB_H_
