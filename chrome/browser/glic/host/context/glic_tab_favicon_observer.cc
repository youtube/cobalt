// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_tab_favicon_observer.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/host/context/favicon_impl/glic_tab_favicon_adapter.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "components/tabs/public/tab_interface.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/glic/host/context/favicon_impl/glic_tab_favicon_adapter_android.h"
#else
#include "chrome/browser/glic/host/context/favicon_impl/glic_tab_favicon_adapter_desktop.h"
#endif

namespace glic {

class GlicTabFaviconObserver::TabObserver {
 public:
  TabObserver(GlicTabFaviconObserver* owner_observer, tabs::TabInterface* tab)
      : owner_observer_(owner_observer), tab_(tab) {
    notifier_.receivers().set_disconnect_handler(base::BindRepeating(
        &TabObserver::OnDisconnected, base::Unretained(this)));
    will_detach_subscription_ = tab_->RegisterWillDetach(base::BindRepeating(
        &TabObserver::OnWillDetach, base::Unretained(this)));

#if BUILDFLAG(IS_ANDROID)
    adapter_ = std::make_unique<FaviconAdapterAndroid>(tab_, &notifier_);
#else
    adapter_ = std::make_unique<FaviconAdapterDesktop>(tab_, &notifier_);
#endif
  }

  ~TabObserver() = default;

  void Subscribe(::mojo::PendingRemote<mojom::TabFaviconHandler> receiver) {
    notifier_.Subscribe(std::move(receiver));
  }

  bool HasReceivers() const { return !notifier_.receivers().empty(); }

#if BUILDFLAG(IS_ANDROID)
  TabFavicon::Observer* GetTabFaviconObserverForTesting() {
    return adapter_->GetTabFaviconObserverForTesting();
  }
#endif

 private:
  void OnDisconnected(mojo::RemoteSetElementId element_id) {
    if (notifier_.receivers().empty()) {
      owner_observer_->ScheduleCleanupForTab(tab_->GetHandle());
    }
  }

  void OnWillDetach(tabs::TabInterface* tab,
                    tabs::TabInterface::DetachReason reason) {
    if (reason == tabs::TabInterface::DetachReason::kDelete) {
      // Destroys `this`.
      owner_observer_->OnTabWillClose(tab->GetHandle());
    }
  }

  raw_ptr<GlicTabFaviconObserver> owner_observer_;
  raw_ptr<tabs::TabInterface> tab_;
  FaviconNotifier notifier_;

  base::CallbackListSubscription will_detach_subscription_;
  std::unique_ptr<FaviconAdapter> adapter_;
};

GlicTabFaviconObserver::GlicTabFaviconObserver(Profile* profile)
    : profile_(profile) {}
GlicTabFaviconObserver::~GlicTabFaviconObserver() = default;

void GlicTabFaviconObserver::OnTabWillClose(tabs::TabHandle tab_handle) {
  observers_.erase(tab_handle);
}

void GlicTabFaviconObserver::SubscribeToTabFavicon(
    int32_t tab_id,
    mojo::PendingRemote<mojom::TabFaviconHandler> remote) {
  tabs::TabInterface::Handle handle(tab_id);
  tabs::TabInterface* tab = handle.Get();
  if (!tab) {
    remote.reset();
    return;
  }
  if (tab->GetProfile() != profile_) {
    remote.reset();
    return;
  }
  TabObserver* observer_ptr = nullptr;
  auto iter = observers_.find(handle);
  if (iter != observers_.end()) {
    observer_ptr = iter->second.get();
  } else {
    auto observer = std::make_unique<TabObserver>(this, tab);
    observer_ptr = observer.get();
    observers_.insert({handle, std::move(observer)});
  }
  observer_ptr->Subscribe(std::move(remote));
}

// Schedules deleting the tab observer later. This isn't done immediately
// to avoid teardown if the observer is used again quickly.
void GlicTabFaviconObserver::ScheduleCleanupForTab(tabs::TabHandle tab_handle) {
  pending_cleanup_.insert(tab_handle);
  if (cleanup_timer_.IsRunning()) {
    return;
  }
  cleanup_timer_.Start(FROM_HERE, base::Seconds(5),
                       base::BindOnce(&GlicTabFaviconObserver::DoCleanup,
                                      base::Unretained(this)));
}

void GlicTabFaviconObserver::DoCleanup() {
  for (tabs::TabHandle handle : std::exchange(pending_cleanup_, {})) {
    auto iter = observers_.find(handle);
    if (iter != observers_.end()) {
      if (!iter->second->HasReceivers()) {
        observers_.erase(iter);
      }
    }
  }
}

#if BUILDFLAG(IS_ANDROID)
TabFavicon::Observer* GlicTabFaviconObserver::GetTabFaviconObserverForTesting(
    tabs::TabInterface::Handle handle) {
  auto iter = observers_.find(handle);
  if (iter == observers_.end()) {
    return nullptr;
  }
  return iter->second->GetTabFaviconObserverForTesting();
}
#endif

bool GlicTabFaviconObserver::HasTabObserverForTesting(
    tabs::TabInterface::Handle handle) {
  return observers_.contains(handle);
}

void GlicTabFaviconObserver::FireCleanupTimerForTesting() {
  if (cleanup_timer_.IsRunning()) {
    cleanup_timer_.FireNow();
  }
}

}  // namespace glic
