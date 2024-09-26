// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_UTILITY_CONTENT_UTILITY_CLIENT_H_
#define CONTENT_PUBLIC_UTILITY_CONTENT_UTILITY_CLIENT_H_

#include <map>
#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "content/common/content_export.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace mojo {
class BinderMap;
class ServiceFactory;
}  // namespace mojo

namespace content {

// Embedder API for participating in utility process logic.
class CONTENT_EXPORT ContentUtilityClient {
 public:
  virtual ~ContentUtilityClient() {}

  // Notifies us that the UtilityThread has been created.
  virtual void UtilityThreadStarted() {}

  // Allows the embedder to register interface binders to handle interface
  // requests coming in from the browser process. These are requests that the
  // browser issues through the ChildProcessHost's BindReceiver() API on the
  // corresponding UtilityProcessHost.
  virtual void ExposeInterfacesToBrowser(mojo::BinderMap* binders) {}

  // Called on the main thread immediately after the IO thread is created.
  virtual void PostIOThreadCreated(
      base::SingleThreadTaskRunner* io_thread_task_runner) {}

  // Allows the embedder to handle an incoming service request. If this is
  // called, this utility process was started for the sole purpose of running
  // the service identified by |service_name|.
  //
  // The embedder should return |true| to indicate that |request| has been
  // handled by running the expected service. It is the embedder's
  // responsibility to ensure that this utility process exits (see
  // |UtilityThread::ReleaseProcess()|) once the running service terminates.
  //
  // If the embedder returns |false| this process is terminated immediately.
  virtual bool HandleServiceRequestDeprecated(
      const std::string& service_name,
      mojo::ScopedMessagePipeHandle service_pipe);

  // Allows the embedder to handle an incoming service interface request to run
  // a service on the IO thread.
  //
  // Only called from the IO thread.
  virtual void RegisterIOThreadServices(mojo::ServiceFactory& services) {}

  // Allows the embedder to handle an incoming service interface request to run
  // a service on the main thread.
  //
  // Only called from the main thread.
  virtual void RegisterMainThreadServices(mojo::ServiceFactory& services) {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_UTILITY_CONTENT_UTILITY_CLIENT_H_
