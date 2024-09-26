// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_CONTROLLER_SERVICE_WORKER_CONNECTOR_H_
#define CONTENT_RENDERER_SERVICE_WORKER_CONTROLLER_SERVICE_WORKER_CONNECTOR_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_container.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_fetch_handler_bypass_option.mojom-shared.h"

namespace content {

namespace mojom {
class ServiceWorkerContainerHost;
}  // namespace mojom

// Vends a connection to the controller service worker for a given
// ServiceWorkerContainerHost. This is co-owned by
// ServiceWorkerProviderContext::ControlleeState and
// ServiceWorkerSubresourceLoader{,Factory}.
class CONTENT_EXPORT ControllerServiceWorkerConnector
    : public blink::mojom::ControllerServiceWorkerConnector,
      public base::RefCounted<ControllerServiceWorkerConnector> {
 public:
  // Observes the connection to the controller.
  class Observer {
   public:
    virtual void OnConnectionClosed() = 0;
  };

  enum class State {
    // The controller connection is dropped. Calling
    // GetControllerServiceWorker() in this state will result in trying to
    // get the new controller pointer from the browser.
    kDisconnected,

    // The controller connection is established.
    kConnected,

    // It is notified that the client lost the controller. This could only
    // happen due to an exceptional condition like the service worker could
    // no longer be read from the script cache. Calling
    // GetControllerServiceWorker() in this state will always return nullptr.
    kNoController,

    // The container host is shutting down. Calling
    // GetControllerServiceWorker() in this state will always return nullptr.
    kNoContainerHost,
  };

  // This class should only be created if a controller exists for the client.
  // |remote_controller| may be nullptr if the caller does not yet have a Mojo
  // connection to the controller. |state_| is set to kDisconnected in that
  // case.
  // Creates and holds the ownership of |container_host_| (as |this|
  // will be created on a different thread from the thread that has the
  // original |remote_container_host|).
  ControllerServiceWorkerConnector(
      mojo::PendingRemote<blink::mojom::ServiceWorkerContainerHost>
          remote_container_host,
      mojo::PendingRemote<blink::mojom::ControllerServiceWorker>
          remote_controller,
      const std::string& client_id,
      blink::mojom::ServiceWorkerFetchHandlerBypassOption
          fetch_handler_bypass_option);

  ControllerServiceWorkerConnector(const ControllerServiceWorkerConnector&) =
      delete;
  ControllerServiceWorkerConnector& operator=(
      const ControllerServiceWorkerConnector&) = delete;

  // This may return nullptr if the connection to the ContainerHost (in the
  // browser process) is already terminated.
  blink::mojom::ControllerServiceWorker* GetControllerServiceWorker(
      blink::mojom::ControllerServiceWorkerPurpose purpose);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void OnContainerHostConnectionClosed();
  void OnControllerConnectionClosed();

  void EnsureFileAccess(const std::vector<base::FilePath>& file_paths,
                        base::OnceClosure callback);

  void AddBinding(
      mojo::PendingReceiver<blink::mojom::ControllerServiceWorkerConnector>
          receiver);

  // blink::mojom::ControllerServiceWorkerConnector:
  void UpdateController(
      mojo::PendingRemote<blink::mojom::ControllerServiceWorker> controller)
      override;

  State state() const { return state_; }

  const std::string& client_id() const { return client_id_; }

  blink::mojom::ServiceWorkerFetchHandlerBypassOption
  fetch_handler_bypass_option() const {
    return fetch_handler_bypass_option_;
  }

 private:
  void SetControllerServiceWorker(
      mojo::PendingRemote<blink::mojom::ControllerServiceWorker> controller);

  State state_ = State::kDisconnected;

  friend class base::RefCounted<ControllerServiceWorkerConnector>;
  ~ControllerServiceWorkerConnector() override;

  mojo::ReceiverSet<blink::mojom::ControllerServiceWorkerConnector> receivers_;

  // Connection to the container host in the browser process.
  mojo::Remote<blink::mojom::ServiceWorkerContainerHost> container_host_;

  // Connection to the controller service worker, which lives in a renderer
  // process that's not necessarily the same as this connector.
  mojo::Remote<blink::mojom::ControllerServiceWorker>
      controller_service_worker_;

  base::ObserverList<Observer>::Unchecked observer_list_;

  // The web-exposed client id, used for FetchEvent#clientId (i.e.,
  // ServiceWorkerContainerHost::client_uuid).
  std::string client_id_;

  blink::mojom::ServiceWorkerFetchHandlerBypassOption
      fetch_handler_bypass_option_ =
          blink::mojom::ServiceWorkerFetchHandlerBypassOption::kDefault;
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_CONTROLLER_SERVICE_WORKER_CONNECTOR_H_
