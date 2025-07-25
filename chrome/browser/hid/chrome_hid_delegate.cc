// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hid/chrome_hid_delegate.h"

#include <cstddef>
#include <utility>

#include "base/containers/contains.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "chrome/browser/hid/hid_chooser_context.h"
#include "chrome/browser/hid/hid_chooser_context_factory.h"
#include "chrome/browser/hid/hid_connection_tracker.h"
#include "chrome/browser/hid/hid_connection_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/hid/hid_chooser.h"
#include "chrome/browser/ui/hid/hid_chooser_controller.h"
#include "chrome/common/chrome_features.h"
#include "components/permissions/object_permission_context_base.h"
#include "content/public/browser/render_frame_host.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace {

HidChooserContext* GetChooserContext(content::BrowserContext* browser_context) {
  // |browser_context| might be null in a service worker case when the browser
  // context is destroyed before service worker's destruction.
  auto* profile = Profile::FromBrowserContext(browser_context);
  return profile ? HidChooserContextFactory::GetForProfile(profile) : nullptr;
}

HidConnectionTracker* GetConnectionTracker(
    content::BrowserContext* browser_context,
    bool create) {
  // |browser_context| might be null in a service worker case when the browser
  // context is destroyed before service worker's destruction.
  auto* profile = Profile::FromBrowserContext(browser_context);
  return profile ? HidConnectionTrackerFactory::GetForProfile(profile, create)
                 : nullptr;
}

}  // namespace

// Manages the HidDelegate observers for a single browser context.
class ChromeHidDelegate::ContextObservation
    : public permissions::ObjectPermissionContextBase::PermissionObserver,
      public HidChooserContext::DeviceObserver {
 public:
  ContextObservation(ChromeHidDelegate* parent,
                     content::BrowserContext* browser_context)
      : parent_(parent), browser_context_(browser_context) {
    auto* chooser_context = GetChooserContext(browser_context_);
    device_observation_.Observe(chooser_context);
    permission_observation_.Observe(chooser_context);
  }

  ContextObservation(ContextObservation&) = delete;
  ContextObservation& operator=(ContextObservation&) = delete;
  ~ContextObservation() override = default;

  // permissions::ObjectPermissionContextBase::PermissionObserver:
  void OnPermissionRevoked(const url::Origin& origin) override {
    for (auto& observer : observer_list_)
      observer.OnPermissionRevoked(origin);
  }

  // HidChooserContext::DeviceObserver:
  void OnDeviceAdded(const device::mojom::HidDeviceInfo& device_info) override {
    for (auto& observer : observer_list_)
      observer.OnDeviceAdded(device_info);
  }

  void OnDeviceRemoved(
      const device::mojom::HidDeviceInfo& device_info) override {
    for (auto& observer : observer_list_)
      observer.OnDeviceRemoved(device_info);
  }

  void OnDeviceChanged(
      const device::mojom::HidDeviceInfo& device_info) override {
    for (auto& observer : observer_list_)
      observer.OnDeviceChanged(device_info);
  }

  void OnHidManagerConnectionError() override {
    for (auto& observer : observer_list_)
      observer.OnHidManagerConnectionError();
  }

  void OnHidChooserContextShutdown() override {
    parent_->observations_.erase(browser_context_);
    // Return since `this` is now deleted.
  }

  void AddObserver(content::HidDelegate::Observer* observer) {
    observer_list_.AddObserver(observer);
  }

  void RemoveObserver(content::HidDelegate::Observer* observer) {
    observer_list_.RemoveObserver(observer);
  }

 private:
  // Safe because `parent_` owns `this`.
  const raw_ptr<ChromeHidDelegate> parent_;

  // Safe because `this` is destroyed when the context is lost.
  const raw_ptr<content::BrowserContext> browser_context_;

  base::ScopedObservation<HidChooserContext, HidChooserContext::DeviceObserver>
      device_observation_{this};
  base::ScopedObservation<
      permissions::ObjectPermissionContextBase,
      permissions::ObjectPermissionContextBase::PermissionObserver>
      permission_observation_{this};
  base::ObserverList<content::HidDelegate::Observer> observer_list_;
};

ChromeHidDelegate::ChromeHidDelegate() = default;

ChromeHidDelegate::~ChromeHidDelegate() = default;

std::unique_ptr<content::HidChooser> ChromeHidDelegate::RunChooser(
    content::RenderFrameHost* render_frame_host,
    std::vector<blink::mojom::HidDeviceFilterPtr> filters,
    std::vector<blink::mojom::HidDeviceFilterPtr> exclusion_filters,
    content::HidChooser::Callback callback) {
  DCHECK(render_frame_host);
  auto* browser_context = render_frame_host->GetBrowserContext();

  // Start observing HidChooserContext for permission and device events.
  GetContextObserver(browser_context);
  DCHECK(base::Contains(observations_, browser_context));

  return std::make_unique<HidChooser>(chrome::ShowDeviceChooserDialog(
      render_frame_host,
      std::make_unique<HidChooserController>(
          render_frame_host, std::move(filters), std::move(exclusion_filters),
          std::move(callback))));
}

bool ChromeHidDelegate::CanRequestDevicePermission(
    content::BrowserContext* browser_context,
    const url::Origin& origin) {
  return browser_context &&
         GetChooserContext(browser_context)->CanRequestObjectPermission(origin);
}

bool ChromeHidDelegate::HasDevicePermission(
    content::BrowserContext* browser_context,
    const url::Origin& origin,
    const device::mojom::HidDeviceInfo& device) {
  return browser_context && GetChooserContext(browser_context)
                                ->HasDevicePermission(origin, device);
}

void ChromeHidDelegate::RevokeDevicePermission(
    content::BrowserContext* browser_context,
    const url::Origin& origin,
    const device::mojom::HidDeviceInfo& device) {
  if (browser_context) {
    GetChooserContext(browser_context)->RevokeDevicePermission(origin, device);
  }
}

device::mojom::HidManager* ChromeHidDelegate::GetHidManager(
    content::BrowserContext* browser_context) {
  if (!browser_context) {
    return nullptr;
  }
  return GetChooserContext(browser_context)->GetHidManager();
}

void ChromeHidDelegate::AddObserver(content::BrowserContext* browser_context,
                                    Observer* observer) {
  if (!browser_context) {
    return;
  }
  GetContextObserver(browser_context)->AddObserver(observer);
}

void ChromeHidDelegate::RemoveObserver(
    content::BrowserContext* browser_context,
    content::HidDelegate::Observer* observer) {
  if (!browser_context) {
    return;
  }
  DCHECK(base::Contains(observations_, browser_context));
  GetContextObserver(browser_context)->RemoveObserver(observer);
}

const device::mojom::HidDeviceInfo* ChromeHidDelegate::GetDeviceInfo(
    content::BrowserContext* browser_context,
    const std::string& guid) {
  auto* chooser_context = GetChooserContext(browser_context);
  if (!chooser_context) {
    return nullptr;
  }
  return chooser_context->GetDeviceInfo(guid);
}

bool ChromeHidDelegate::IsFidoAllowedForOrigin(
    content::BrowserContext* browser_context,
    const url::Origin& origin) {
  auto* chooser_context = GetChooserContext(browser_context);
  return chooser_context && chooser_context->IsFidoAllowedForOrigin(origin);
}

bool ChromeHidDelegate::IsServiceWorkerAllowedForOrigin(
    const url::Origin& origin) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // WebHID is only available on extension service workers with feature flag
  // enabled for now.
  if (base::FeatureList::IsEnabled(
          features::kEnableWebHidOnExtensionServiceWorker) &&
      origin.scheme() == extensions::kExtensionScheme)
    return true;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  return false;
}

ChromeHidDelegate::ContextObservation* ChromeHidDelegate::GetContextObserver(
    content::BrowserContext* browser_context) {
  DCHECK(browser_context);
  if (!base::Contains(observations_, browser_context)) {
    observations_.emplace(browser_context, std::make_unique<ContextObservation>(
                                               this, browser_context));
  }
  return observations_[browser_context].get();
}

void ChromeHidDelegate::IncrementConnectionCount(
    content::BrowserContext* browser_context,
    const url::Origin& origin) {
  // Don't track connection when the feature isn't enabled or the connection
  // isn't made by an extension origin.
  if (!base::FeatureList::IsEnabled(
          features::kEnableWebHidOnExtensionServiceWorker) ||
      origin.scheme() != extensions::kExtensionScheme) {
    return;
  }

  auto* hid_connection_tracker =
      GetConnectionTracker(browser_context, /*create=*/true);
  if (hid_connection_tracker) {
    hid_connection_tracker->IncrementConnectionCount(origin);
  }
}

void ChromeHidDelegate::DecrementConnectionCount(
    content::BrowserContext* browser_context,
    const url::Origin& origin) {
  // Don't track connection when the feature isn't enabled or the connection
  // isn't made by an extension origin.
  if (!base::FeatureList::IsEnabled(
          features::kEnableWebHidOnExtensionServiceWorker) ||
      origin.scheme() != extensions::kExtensionScheme) {
    return;
  }
  auto* hid_connection_tracker =
      GetConnectionTracker(browser_context, /*create=*/false);
  if (hid_connection_tracker) {
    hid_connection_tracker->DecrementConnectionCount(origin);
  }
}
