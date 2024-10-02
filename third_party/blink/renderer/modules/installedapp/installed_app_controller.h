// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INSTALLEDAPP_INSTALLED_APP_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INSTALLEDAPP_INSTALLED_APP_CONTROLLER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/mojom/installedapp/installed_app_provider.mojom-blink.h"
#include "third_party/blink/public/mojom/installedapp/related_application.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/callback_promise_adapter.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_related_application.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

using AppInstalledCallbacks =
    CallbackPromiseAdapter<HeapVector<Member<RelatedApplication>>, void>;

class MODULES_EXPORT InstalledAppController final
    : public GarbageCollected<InstalledAppController>,
      public Supplement<LocalDOMWindow> {
 public:
  static const char kSupplementName[];

  explicit InstalledAppController(LocalDOMWindow&);

  InstalledAppController(const InstalledAppController&) = delete;
  InstalledAppController& operator=(const InstalledAppController&) = delete;

  virtual ~InstalledAppController();

  // Gets a list of related apps from the current page's manifest that belong
  // to the current underlying platform, and are installed.
  void GetInstalledRelatedApps(std::unique_ptr<AppInstalledCallbacks>);

  static InstalledAppController* From(LocalDOMWindow&);

  void Trace(Visitor*) const override;

 private:
  // Callback for the result of GetInstalledRelatedApps.
  //
  // Takes a set of related applications and filters them by those which belong
  // to the current underlying platform, and are actually installed and related
  // to the current page's origin. Passes the filtered list to the callback.
  void OnGetManifestForRelatedApps(
      std::unique_ptr<AppInstalledCallbacks> callbacks,
      const KURL& url,
      mojom::blink::ManifestPtr manifest);

  // Callback from the InstalledAppProvider mojo service.
  void OnFilterInstalledApps(std::unique_ptr<AppInstalledCallbacks>,
                             Vector<mojom::blink::RelatedApplicationPtr>);

  // Handle to the InstalledApp mojo service.
  HeapMojoRemote<mojom::blink::InstalledAppProvider> provider_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INSTALLEDAPP_INSTALLED_APP_CONTROLLER_H_
