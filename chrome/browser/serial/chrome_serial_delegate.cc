// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/serial/chrome_serial_delegate.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/serial/serial_chooser_context.h"
#include "chrome/browser/serial/serial_chooser_context_factory.h"
#include "chrome/browser/serial/web_serial_chooser.h"
#include "chrome/browser/ui/serial/serial_chooser_controller.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/security_principal.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"

namespace {

SerialChooserContext* GetChooserContext(content::RenderFrameHost* frame) {
  auto* profile = Profile::FromBrowserContext(frame->GetBrowserContext());
  return SerialChooserContextFactory::GetForProfile(profile);
}

}  // namespace

ChromeSerialDelegate::ChromeSerialDelegate() = default;

ChromeSerialDelegate::~ChromeSerialDelegate() = default;

bool ChromeSerialDelegate::MayUseSerial(content::RenderFrameHost* frame) {
  content::RenderFrameHost* main_rfh = frame->GetMainFrame();

  // Because permission is scoped to the profile, guest contexts (like
  // <webview>, <controlledframe>, and SlimWebView), despite having isolated
  // StoragePartitions, would share serial permissions with the rest of the
  // profile. Therefore, serial is not allowed in these contexts.
  if (main_rfh->GetSiteInstance()->GetSecurityPrincipal().IsGuest()) {
    return false;
  }

  if (main_rfh->GetStoragePartition() !=
      main_rfh->GetBrowserContext()->GetDefaultStoragePartition()) {
    return !main_rfh->GetLastCommittedURL().SchemeIsHTTPOrHTTPS();
  }

  return true;
}

std::unique_ptr<content::SerialChooser> ChromeSerialDelegate::RunChooser(
    content::RenderFrameHost* frame,
    std::vector<blink::mojom::SerialPortFilterPtr> filters,
    std::vector<device::BluetoothUUID> allowed_bluetooth_service_class_ids,
    content::SerialChooser::Callback callback) {
  return WebSerialChooser::Create(
      frame,
      std::make_unique<SerialChooserController>(
          frame, std::move(filters),
          std::move(allowed_bluetooth_service_class_ids), std::move(callback)));
}

bool ChromeSerialDelegate::CanRequestPortPermission(
    content::RenderFrameHost* frame) {
  if (!MayUseSerial(frame)) {
    return false;
  }

  return GetChooserContext(frame)->CanRequestObjectPermission(
      frame->GetMainFrame()->GetLastCommittedOrigin());
}

bool ChromeSerialDelegate::HasPortPermission(
    content::RenderFrameHost* frame,
    const device::mojom::SerialPortInfo& port) {
  if (!MayUseSerial(frame)) {
    return false;
  }

  return GetChooserContext(frame)->HasPortPermission(
      frame->GetMainFrame()->GetLastCommittedOrigin(), port);
}

void ChromeSerialDelegate::RevokePortPermissionWebInitiated(
    content::RenderFrameHost* frame,
    const base::UnguessableToken& token) {
  return GetChooserContext(frame)->RevokePortPermissionWebInitiated(
      frame->GetMainFrame()->GetLastCommittedOrigin(), token);
}

const device::mojom::SerialPortInfo* ChromeSerialDelegate::GetPortInfo(
    content::RenderFrameHost* frame,
    const base::UnguessableToken& token) {
  return GetChooserContext(frame)->GetPortInfo(token);
}

device::mojom::SerialPortManager* ChromeSerialDelegate::GetPortManager(
    content::RenderFrameHost* frame) {
  return GetChooserContext(frame)->GetPortManager();
}

void ChromeSerialDelegate::AddObserver(content::RenderFrameHost* frame,
                                       Observer* observer) {
  return GetChooserContext(frame)->AddPortObserver(observer);
}

void ChromeSerialDelegate::RemoveObserver(content::RenderFrameHost* frame,
                                          Observer* observer) {
  return GetChooserContext(frame)->RemovePortObserver(observer);
}
