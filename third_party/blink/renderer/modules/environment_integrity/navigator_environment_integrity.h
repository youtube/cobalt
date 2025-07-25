// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ENVIRONMENT_INTEGRITY_NAVIGATOR_ENVIRONMENT_INTEGRITY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ENVIRONMENT_INTEGRITY_NAVIGATOR_ENVIRONMENT_INTEGRITY_H_

#include "third_party/blink/public/mojom/environment_integrity/environment_integrity_service.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver_with_tracker.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/network/blink_schemeful_site.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class ExceptionState;
class ScriptPromise;
class ScriptState;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class EnvironmentIntegrityResult {
  kOk = 0,
  kTimedOut = 1,
  kUnknownError = 2,
  kNotSupported = 3,
  kHeldBack = 4,
  kMaxValue = kHeldBack
};

class MODULES_EXPORT NavigatorEnvironmentIntegrity final
    : public GarbageCollected<NavigatorEnvironmentIntegrity>,
      public Supplement<Navigator> {
 public:
  static const char kSupplementName[];

  explicit NavigatorEnvironmentIntegrity(Navigator&);

  // Gets, or creates, NavigatorEnvironmentIntegrity supplement on Navigator.
  // See platform/supplementable.h
  static NavigatorEnvironmentIntegrity& From(ExecutionContext*, Navigator&);

  ScriptPromise getEnvironmentIntegrity(ScriptState*,
                                        const String&,
                                        ExceptionState&);

  static ScriptPromise getEnvironmentIntegrity(ScriptState*,
                                               Navigator&,
                                               const String&,
                                               ExceptionState&);

  void Trace(Visitor* visitor) const override;

#if BUILDFLAG(IS_ANDROID)
 private:
  void ResolveEnvironmentIntegrity(
      ScriptPromiseResolverWithTracker<EnvironmentIntegrityResult>* resolver,
      mojom::blink::EnvironmentIntegrityResponseCode response_code,
      const Vector<uint8_t>& token);

  HeapMojoRemote<mojom::blink::EnvironmentIntegrityService>
      remote_environment_integrity_service_;
#endif
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ENVIRONMENT_INTEGRITY_NAVIGATOR_ENVIRONMENT_INTEGRITY_H_
