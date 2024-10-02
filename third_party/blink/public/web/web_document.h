/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_DOCUMENT_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_DOCUMENT_H_

#include "net/cookies/site_for_cookies.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "services/network/public/mojom/restricted_cookie_manager.mojom-shared.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/platform/cross_variant_mojo_util.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_css_origin.h"
#include "third_party/blink/public/web/web_draggable_region.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/skia/include/core/SkColor.h"

namespace blink {

class Document;
class WebElement;
class WebFormElement;
class WebFormControlElement;
class WebElementCollection;
class WebString;
class WebURL;
struct WebDistillabilityFeatures;

using WebStyleSheetKey = WebString;

// An enumeration used to enumerate usage of APIs that may prevent a document
// from entering the back forward cache. |kAllow| means usage of the API will
// not restrict the back forward cache. |kPossiblyDisallow| means usage of the
// API will be marked as such and the back forward cache may not allow the
// document to enter at its discretion.
enum class BackForwardCacheAware { kAllow, kPossiblyDisallow };

// Provides readonly access to some properties of a DOM document.
class BLINK_EXPORT WebDocument : public WebNode {
 public:
  WebDocument() = default;
  WebDocument(const WebDocument& e) = default;

  WebDocument& operator=(const WebDocument& e) {
    WebNode::Assign(e);
    return *this;
  }
  void Assign(const WebDocument& e) { WebNode::Assign(e); }

  const DocumentToken& Token() const;
  WebURL Url() const;

  // Note: Security checks should use the getSecurityOrigin(), not url().
  WebSecurityOrigin GetSecurityOrigin() const;
  bool IsSecureContext() const;

  WebString Encoding() const;
  WebString ContentLanguage() const;
  WebString GetReferrer() const;
  absl::optional<SkColor> ThemeColor();
  // The url of the OpenSearch Description Document (if any).
  WebURL OpenSearchDescriptionURL() const;

  // Returns the frame the document belongs to or 0 if the document is
  // frameless.
  WebLocalFrame* GetFrame() const;
  bool IsHTMLDocument() const;
  bool IsXHTMLDocument() const;
  bool IsPluginDocument() const;
  WebURL BaseURL() const;
  ukm::SourceId GetUkmSourceId() const;

  // The SiteForCookies is used to compute whether this document
  // appears in a "third-party" context for the purpose of third-party
  // cookie blocking.
  net::SiteForCookies SiteForCookies() const;

  // The `HasStorageAccess` boolean is used to determine whether this document
  // has opted into using the Storage Access API. This is relevant when
  // attempting to access cookies in a context where third-party cookies may be
  // blocked.
  bool HasStorageAccess() const;

  WebSecurityOrigin TopFrameOrigin() const;
  WebElement DocumentElement() const;
  WebElement Body() const;
  WebElement Head();
  WebString Title() const;
  WebString ContentAsTextForTesting() const;
  WebElementCollection All() const;
  WebVector<WebFormElement> Forms() const;
  WebURL CompleteURL(const WebString&) const;
  WebElement GetElementById(const WebString&) const;
  WebElement FocusedElement() const;

  // The unassociated form controls are form control elements that are not
  // associated to a <form> element.
  WebVector<WebFormControlElement> UnassociatedFormControls() const;

  // Inserts the given CSS source code as a style sheet in the document.
  WebStyleSheetKey InsertStyleSheet(
      const WebString& source_code,
      const WebStyleSheetKey* = nullptr,
      WebCssOrigin = WebCssOrigin::kAuthor,
      BackForwardCacheAware = BackForwardCacheAware::kAllow);

  // Removes the CSS which was previously inserted by a call to
  // InsertStyleSheet().
  void RemoveInsertedStyleSheet(const WebStyleSheetKey&,
                                WebCssOrigin = WebCssOrigin::kAuthor);

  // Arranges to call WebLocalFrameClient::didMatchCSS(frame(), ...) when one of
  // the selectors matches or stops matching an element in this document.
  // Each call to this method overrides any previous calls.
  void WatchCSSSelectors(const WebVector<WebString>& selectors);

  WebVector<WebDraggableRegion> DraggableRegions() const;

  WebDistillabilityFeatures DistillabilityFeatures();

  void SetShowBeforeUnloadDialog(bool show_dialog);

  cc::ElementId GetVisualViewportScrollingElementIdForTesting();

  bool IsLoaded();

  // Returns true if the document is in prerendering.
  bool IsPrerendering();

  // Return true if  accessibility processing has been enabled.
  bool IsAccessibilityEnabled();

  // Adds `callback` to the post-prerendering activation steps.
  // https://wicg.github.io/nav-speculation/prerendering.html#document-post-prerendering-activation-steps-list
  void AddPostPrerenderingActivationStep(base::OnceClosure callback);

  // Sets a cookie manager which can be used for this document.
  void SetCookieManager(
      CrossVariantMojoRemote<
          network::mojom::RestrictedCookieManagerInterfaceBase> cookie_manager);

#if INSIDE_BLINK
  WebDocument(Document*);
  WebDocument& operator=(Document*);
  operator Document*() const;
#endif
};

DECLARE_WEB_NODE_TYPE_CASTS(WebDocument);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_DOCUMENT_H_
