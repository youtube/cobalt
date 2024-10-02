// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/usb/web_usb_service_impl.h"

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/document_service.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"
#include "services/device/public/mojom/usb_manager_client.mojom.h"

namespace content {

namespace {

// Deletes the WebUsbService when the connected document is destroyed.
class DocumentHelper : public DocumentService<blink::mojom::WebUsbService> {
 public:
  DocumentHelper(std::unique_ptr<WebUsbServiceImpl> service,
                 RenderFrameHostImpl& render_frame_host,
                 mojo::PendingReceiver<blink::mojom::WebUsbService> receiver)
      : DocumentService(render_frame_host, std::move(receiver)),
        service_(std::move(service)) {
    DCHECK(service_);
  }

  DocumentHelper(const DocumentHelper&) = delete;
  DocumentHelper& operator=(const DocumentHelper&) = delete;

  ~DocumentHelper() override = default;

  // blink::mojom::WebUsbService:
  void GetDevices(GetDevicesCallback callback) override {
    service_->GetDevices(std::move(callback));
  }
  void GetDevice(const std::string& guid,
                 mojo::PendingReceiver<device::mojom::UsbDevice>
                     device_receiver) override {
    service_->GetDevice(guid, std::move(device_receiver));
  }
  void GetPermission(
      std::vector<device::mojom::UsbDeviceFilterPtr> device_filters,
      blink::mojom::WebUsbService::GetPermissionCallback callback) override {
    service_->GetPermission(std::move(device_filters), std::move(callback));
  }
  void ForgetDevice(
      const std::string& guid,
      blink::mojom::WebUsbService::ForgetDeviceCallback callback) override {
    service_->ForgetDevice(guid, std::move(callback));
  }
  void SetClient(
      mojo::PendingAssociatedRemote<device::mojom::UsbDeviceManagerClient>
          client) override {
    service_->SetClient(std::move(client));
  }

 private:
  const std::unique_ptr<WebUsbServiceImpl> service_;
};

}  // namespace

// A UsbDeviceClient represents a UsbDevice pipe that has been passed to the
// renderer process. The UsbDeviceClient pipe allows the browser process to
// continue to monitor how the device is used and cause the connection to be
// closed at will.
class WebUsbServiceImpl::UsbDeviceClient
    : public device::mojom::UsbDeviceClient {
 public:
  UsbDeviceClient(
      WebUsbServiceImpl* service,
      const std::string& device_guid,
      mojo::PendingReceiver<device::mojom::UsbDeviceClient> receiver)
      : service_(service),
        device_guid_(device_guid),
        receiver_(this, std::move(receiver)) {
    receiver_.set_disconnect_handler(
        base::BindOnce(&WebUsbServiceImpl::RemoveDeviceClient,
                       base::Unretained(service_), base::Unretained(this)));
  }

  UsbDeviceClient(const UsbDeviceClient&) = delete;
  UsbDeviceClient& operator=(const UsbDeviceClient&) = delete;

  ~UsbDeviceClient() override {
    if (opened_) {
      // If the connection was opened destroying |receiver_| will cause it to
      // be closed but that event won't be dispatched here because the receiver
      // has been destroyed.
      OnDeviceClosed();
    }
  }

  const std::string& device_guid() const { return device_guid_; }

  // device::mojom::UsbDeviceClient implementation:
  void OnDeviceOpened() override {
    DCHECK(!opened_);
    opened_ = true;
    service_->IncrementConnectionCount();
  }

  void OnDeviceClosed() override {
    DCHECK(opened_);
    opened_ = false;
    service_->DecrementConnectionCount();
  }

 private:
  const raw_ptr<WebUsbServiceImpl> service_;
  const std::string device_guid_;
  bool opened_ = false;
  mojo::Receiver<device::mojom::UsbDeviceClient> receiver_;
};

WebUsbServiceImpl::WebUsbServiceImpl(
    RenderFrameHostImpl* render_frame_host,
    base::WeakPtr<ServiceWorkerContextCore> service_worker_context,
    const url::Origin& origin)
    : render_frame_host_(render_frame_host),
      service_worker_context_(std::move(service_worker_context)),
      origin_(origin) {
  auto* delegate = GetContentClient()->browser()->GetUsbDelegate();
  if (delegate)
    delegate->AddObserver(GetBrowserContext(), this);
}

WebUsbServiceImpl::~WebUsbServiceImpl() {
  auto* delegate = GetContentClient()->browser()->GetUsbDelegate();
  if (delegate)
    delegate->RemoveObserver(GetBrowserContext(), this);
}

// static
void WebUsbServiceImpl::Create(
    RenderFrameHostImpl& render_frame_host,
    mojo::PendingReceiver<blink::mojom::WebUsbService> pending_receiver) {
  if (!render_frame_host.IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kUsb)) {
    mojo::ReportBadMessage("Permissions policy blocks access to USB.");
    return;
  }

  // Avoid creating the WebUsbService if there is no USB delegate to provide the
  // implementation.
  if (!GetContentClient()->browser()->GetUsbDelegate()) {
    return;
  }

  if (render_frame_host.IsNestedWithinFencedFrame()) {
    // The renderer is supposed to disallow the use of USB services when inside
    // a fenced frame. Anything getting past the renderer checks must be marked
    // as a bad request.
    mojo::ReportBadMessage("WebUSB is not allowed in a fenced frame tree.");
    return;
  }

  // DocumentHelper observes the lifetime of the document connected to
  // `render_frame_host` and destroys the WebUsbService when the Mojo connection
  // is disconnected, RenderFrameHost is deleted, or the RenderFrameHost commits
  // a cross-document navigation. It forwards its Mojo interface to
  // WebUsbServiceImpl.
  new DocumentHelper(
      std::make_unique<WebUsbServiceImpl>(
          &render_frame_host,
          /*service_worker_context=*/nullptr,
          render_frame_host.GetMainFrame()->GetLastCommittedOrigin()),
      render_frame_host, std::move(pending_receiver));
}

// static
void WebUsbServiceImpl::Create(
    base::WeakPtr<ServiceWorkerContextCore> service_worker_context,
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::WebUsbService> pending_receiver) {
  DCHECK(service_worker_context);

  // Avoid creating the WebUsbService if there is no USB delegate to provide
  // the implementation or if `origin` is not eligible to access WebUSB from a
  // service worker.
  auto* delegate = GetContentClient()->browser()->GetUsbDelegate();
  if (!delegate || !delegate->IsServiceWorkerAllowedForOrigin(origin)) {
    return;
  }

  // This makes the WebUsbService a self-owned receiver so it will self-destruct
  // when a mojo interface error occurs.
  mojo::MakeSelfOwnedReceiver(std::make_unique<WebUsbServiceImpl>(
                                  /*render_frame_host=*/nullptr,
                                  std::move(service_worker_context), origin),
                              std::move(pending_receiver));
}

BrowserContext* WebUsbServiceImpl::GetBrowserContext() const {
  if (render_frame_host_) {
    return render_frame_host_->GetBrowserContext();
  }
  if (service_worker_context_) {
    return service_worker_context_->wrapper()->browser_context();
  }
  return nullptr;
}

std::vector<uint8_t> WebUsbServiceImpl::GetProtectedInterfaceClasses() const {
  // Specified in https://wicg.github.io/webusb#protected-interface-classes
  std::vector<uint8_t> classes = {
      device::mojom::kUsbAudioClass,       device::mojom::kUsbHidClass,
      device::mojom::kUsbMassStorageClass, device::mojom::kUsbSmartCardClass,
      device::mojom::kUsbVideoClass,       device::mojom::kUsbAudioVideoClass,
      device::mojom::kUsbWirelessClass,
  };

  auto* delegate = GetContentClient()->browser()->GetUsbDelegate();
  if (delegate) {
    delegate->AdjustProtectedInterfaceClasses(GetBrowserContext(), origin_,
                                              render_frame_host_, classes);
  }

  return classes;
}

void WebUsbServiceImpl::GetDevices(GetDevicesCallback callback) {
  auto* delegate = GetContentClient()->browser()->GetUsbDelegate();
  if (!delegate) {
    std::move(callback).Run(std::vector<device::mojom::UsbDeviceInfoPtr>());
    return;
  }

  delegate->GetDevices(
      GetBrowserContext(),
      base::BindOnce(&WebUsbServiceImpl::OnGetDevices,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void WebUsbServiceImpl::OnGetDevices(
    GetDevicesCallback callback,
    std::vector<device::mojom::UsbDeviceInfoPtr> device_info_list) {
  auto* delegate = GetContentClient()->browser()->GetUsbDelegate();
  DCHECK(delegate);

  std::vector<device::mojom::UsbDeviceInfoPtr> device_infos;
  for (auto& device_info : device_info_list) {
    if (delegate->HasDevicePermission(GetBrowserContext(), origin_,
                                      *device_info)) {
      device_infos.push_back(device_info.Clone());
    }
  }
  std::move(callback).Run(std::move(device_infos));
}

void WebUsbServiceImpl::GetDevice(
    const std::string& guid,
    mojo::PendingReceiver<device::mojom::UsbDevice> device_receiver) {
  auto* delegate = GetContentClient()->browser()->GetUsbDelegate();
  if (!delegate) {
    return;
  }

  auto* browser_context = GetBrowserContext();
  auto* device_info = delegate->GetDeviceInfo(browser_context, guid);
  if (!device_info ||
      !delegate->HasDevicePermission(browser_context, origin_, *device_info)) {
    return;
  }

  // Connect Blink to the native device and keep a receiver to this for the
  // UsbDeviceClient interface so we can receive DeviceOpened/Closed events.
  // This receiver will also be closed to notify the device service to close
  // the connection if permission is revoked.
  mojo::PendingRemote<device::mojom::UsbDeviceClient> device_client;
  device_clients_.push_back(std::make_unique<UsbDeviceClient>(
      this, guid, device_client.InitWithNewPipeAndPassReceiver()));

  delegate->GetDevice(GetBrowserContext(), guid, GetProtectedInterfaceClasses(),
                      std::move(device_receiver), std::move(device_client));
}

void WebUsbServiceImpl::GetPermission(
    std::vector<device::mojom::UsbDeviceFilterPtr> device_filters,
    GetPermissionCallback callback) {
  auto* delegate = GetContentClient()->browser()->GetUsbDelegate();
  if (!delegate ||
      !delegate->CanRequestDevicePermission(GetBrowserContext(), origin_)) {
    std::move(callback).Run(nullptr);
    return;
  }

  usb_chooser_ = delegate->RunChooser(
      *render_frame_host_, std::move(device_filters), std::move(callback));
}

void WebUsbServiceImpl::ForgetDevice(const std::string& guid,
                                     ForgetDeviceCallback callback) {
  auto* delegate = GetContentClient()->browser()->GetUsbDelegate();
  if (delegate) {
    auto* browser_context = GetBrowserContext();
    auto* device_info = delegate->GetDeviceInfo(browser_context, guid);
    if (device_info &&
        delegate->HasDevicePermission(browser_context, origin_, *device_info)) {
      delegate->RevokeDevicePermissionWebInitiated(browser_context, origin_,
                                                   *device_info);
    }
  }
  std::move(callback).Run();
}

void WebUsbServiceImpl::SetClient(
    mojo::PendingAssociatedRemote<device::mojom::UsbDeviceManagerClient>
        client) {
  DCHECK(client);
  clients_.Add(std::move(client));
}

void WebUsbServiceImpl::OnPermissionRevoked(const url::Origin& origin) {
  if (origin_ != origin) {
    return;
  }

  // Close the connection between Blink and the device if the device lost
  // permission.
  auto* delegate = GetContentClient()->browser()->GetUsbDelegate();
  auto* browser_context = GetBrowserContext();
  base::EraseIf(device_clients_, [=](const auto& client) {
    auto* device_info =
        delegate->GetDeviceInfo(browser_context, client->device_guid());
    if (!device_info)
      return true;

    return !delegate->HasDevicePermission(browser_context, origin_,
                                          *device_info);
  });
}

void WebUsbServiceImpl::OnDeviceAdded(
    const device::mojom::UsbDeviceInfo& device_info) {
  if (!GetContentClient()->browser()->GetUsbDelegate()->HasDevicePermission(
          GetBrowserContext(), origin_, device_info)) {
    return;
  }
  for (auto& client : clients_)
    client->OnDeviceAdded(device_info.Clone());
}

void WebUsbServiceImpl::OnDeviceRemoved(
    const device::mojom::UsbDeviceInfo& device_info) {
  base::EraseIf(device_clients_, [&device_info](const auto& client) {
    return device_info.guid == client->device_guid();
  });

  if (!GetContentClient()->browser()->GetUsbDelegate()->HasDevicePermission(
          GetBrowserContext(), origin_, device_info)) {
    return;
  }
  for (auto& client : clients_)
    client->OnDeviceRemoved(device_info.Clone());
}

void WebUsbServiceImpl::OnDeviceManagerConnectionError() {
  // Close the connection with blink.
  clients_.Clear();
}

// device::mojom::UsbDeviceClient implementation:
void WebUsbServiceImpl::IncrementConnectionCount() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!render_frame_host_)
    return;

  if (connection_count_++ == 0) {
    auto* web_contents = static_cast<WebContentsImpl*>(
        WebContents::FromRenderFrameHost(render_frame_host_));
    web_contents->IncrementUsbActiveFrameCount();
  }
}

void WebUsbServiceImpl::DecrementConnectionCount() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!render_frame_host_)
    return;

  DCHECK_GT(connection_count_, 0);
  if (--connection_count_ == 0) {
    auto* web_contents = static_cast<WebContentsImpl*>(
        WebContents::FromRenderFrameHost(render_frame_host_));
    web_contents->DecrementUsbActiveFrameCount();
  }
}

void WebUsbServiceImpl::RemoveDeviceClient(const UsbDeviceClient* client) {
  base::EraseIf(device_clients_, [client](const auto& this_client) {
    return client == this_client.get();
  });
}

}  // namespace content
