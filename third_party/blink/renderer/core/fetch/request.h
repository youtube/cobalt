// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_REQUEST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_REQUEST_H_

#include "services/network/public/mojom/fetch_api.mojom-blink-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink-forward.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/fetch/body.h"
#include "third_party/blink/renderer/core/fetch/fetch_request_data.h"
#include "third_party/blink/renderer/core/fetch/headers.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class AbortSignal;
class BodyStreamBuffer;
class ExceptionState;
class RequestInit;

class CORE_EXPORT Request final : public ScriptWrappable,
                                  public ActiveScriptWrappable<Request>,
                                  public Body {
  DEFINE_WRAPPERTYPEINFO();

 public:
  using ForServiceWorkerFetchEvent =
      FetchRequestData::ForServiceWorkerFetchEvent;

  // These "create" function must be called with entering an appropriate
  // V8 context.
  // From Request.idl:
  static Request* Create(ScriptState* script_state,
                         const V8RequestInfo* input,
                         const RequestInit* init,
                         ExceptionState& exception_state);

  static Request* Create(ScriptState*, const String&, ExceptionState&);
  static Request* Create(ScriptState*,
                         const String&,
                         const RequestInit*,
                         ExceptionState&);
  static Request* Create(ScriptState*, Request*, ExceptionState&);
  static Request* Create(ScriptState*,
                         Request*,
                         const RequestInit*,
                         ExceptionState&);
  static Request* Create(ScriptState*, FetchRequestData*);
  static Request* Create(ScriptState*,
                         mojom::blink::FetchAPIRequestPtr,
                         ForServiceWorkerFetchEvent);

  Request(ScriptState*, FetchRequestData*, Headers*, AbortSignal*);
  Request(ScriptState*, FetchRequestData*);
  Request(const Request&) = delete;
  Request& operator=(const Request&) = delete;

  static absl::optional<network::mojom::CredentialsMode> ParseCredentialsMode(
      const String& credentials_mode);

  // From Request.idl:
  String method() const;
  const KURL& url() const;
  Headers* getHeaders() const { return headers_; }
  String destination() const;
  String referrer() const;
  String getReferrerPolicy() const;
  String mode() const;
  String credentials() const;
  String cache() const;
  String redirect() const;
  String integrity() const;
  bool keepalive() const;
  bool isHistoryNavigation() const;
  AbortSignal* signal() const { return signal_; }
  String targetAddressSpace() const;

  // From Request.idl:
  // This function must be called with entering an appropriate V8 context.
  Request* clone(ScriptState*, ExceptionState&);

  // ScriptWrappable override
  bool HasPendingActivity() const override {
    return Body::HasPendingActivity();
  }

  FetchRequestData* PassRequestData(ScriptState*);
  mojom::blink::FetchAPIRequestPtr CreateFetchAPIRequest() const;
  bool HasBody() const;
  BodyStreamBuffer* BodyBuffer() override { return request_->Buffer(); }
  const BodyStreamBuffer* BodyBuffer() const override {
    return request_->Buffer();
  }
  mojom::blink::RequestContextType GetRequestContextType() const;
  network::mojom::RequestDestination GetRequestDestination() const;
  network::mojom::RequestMode GetRequestMode() const;

  void Trace(Visitor*) const override;

 private:
  const FetchRequestData* GetRequest() const { return request_; }
  static Request* CreateRequestWithRequestOrString(ScriptState*,
                                                   Request*,
                                                   const String&,
                                                   const RequestInit*,
                                                   ExceptionState&);

  String ContentType() const override;
  String MimeType() const override;

  const Member<FetchRequestData> request_;
  const Member<Headers> headers_;
  const Member<AbortSignal> signal_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_REQUEST_H_
