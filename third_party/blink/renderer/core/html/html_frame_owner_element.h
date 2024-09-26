/*
 * Copyright (C) 2006, 2007, 2009 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_FRAME_OWNER_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_FRAME_OWNER_ELEMENT_H_

#include "services/network/public/mojom/trust_tokens.mojom-blink-forward.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "third_party/blink/public/mojom/scroll/scrollbar_mode.mojom-blink.h"
#include "third_party/blink/public/mojom/timing/resource_timing.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/dom_window.h"
#include "third_party/blink/renderer/core/frame/embedded_content_view.h"
#include "third_party/blink/renderer/core/frame/frame_owner.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/permissions_policy/permissions_policy_parser.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/hash_counted_set.h"

namespace blink {

class ExceptionState;
class Frame;
class LayoutEmbeddedContent;
class LazyLoadFrameObserver;
class WebPluginContainerImpl;
class ResourceRequestHead;

class CORE_EXPORT HTMLFrameOwnerElement : public HTMLElement,
                                          public FrameOwner {
 public:
  ~HTMLFrameOwnerElement() override;

  DOMWindow* contentWindow() const;
  Document* contentDocument() const;

  virtual void DisconnectContentFrame();

  // Most subclasses use LayoutEmbeddedContent (either LayoutEmbeddedObject or
  // LayoutIFrame) except for HTMLObjectElement and HTMLEmbedElement which may
  // return any LayoutObject when using fallback content.
  LayoutEmbeddedContent* GetLayoutEmbeddedContent() const;

  // Whether to collapse the frame owner element in the embedder document. That
  // is, to remove it from the layout as if it did not exist.
  virtual void SetCollapsed(bool) {}

  virtual FrameOwnerElementType OwnerType() const = 0;

  Document* getSVGDocument(ExceptionState&) const;

  void SetEmbeddedContentView(EmbeddedContentView*);
  EmbeddedContentView* ReleaseEmbeddedContentView();
  EmbeddedContentView* OwnedEmbeddedContentView() const {
    return embedded_content_view_;
  }

  void SetColorScheme(mojom::blink::ColorScheme);

  class PluginDisposeSuspendScope {
    STACK_ALLOCATED();

   public:
    PluginDisposeSuspendScope() { suspend_count_ += 2; }
    ~PluginDisposeSuspendScope() {
      suspend_count_ -= 2;
      if (suspend_count_ == 1)
        PerformDeferredPluginDispose();
    }

   private:
    void PerformDeferredPluginDispose();

    // Low bit indicates if there are plugins to dispose.
    static int suspend_count_;

    friend class HTMLFrameOwnerElement;
  };

  // FrameOwner overrides:
  Frame* ContentFrame() const final { return content_frame_; }
  void SetContentFrame(Frame&) final;
  void ClearContentFrame() final;
  void AddResourceTiming(mojom::blink::ResourceTimingInfoPtr) final;
  void DispatchLoad() final;
  const FramePolicy& GetFramePolicy() const final { return frame_policy_; }
  void IntrinsicSizingInfoChanged() override {}
  void SetNeedsOcclusionTracking(bool) override {}
  AtomicString BrowsingContextContainerName() const override {
    return FastGetAttribute(html_names::kNameAttr);
  }
  mojom::blink::ScrollbarMode ScrollbarMode() const override {
    return mojom::blink::ScrollbarMode::kAuto;
  }
  int MarginWidth() const override { return -1; }
  int MarginHeight() const override { return -1; }
  bool AllowFullscreen() const override { return false; }
  bool AllowPaymentRequest() const override { return false; }
  bool IsDisplayNone() const override { return !embedded_content_view_; }
  mojom::blink::ColorScheme GetColorScheme() const override;
  bool ShouldLazyLoadChildren() const final;
  void DidReportResourceTiming();
  bool HasPendingFallbackTimingInfo() const;

  // For unit tests, manually trigger the UpdateContainerPolicy method.
  void UpdateContainerPolicyForTests() { UpdateContainerPolicy(); }

  void CancelPendingLazyLoad();

  void ParseAttribute(const AttributeModificationParams&) override;

  // Element overrides:
  bool IsAdRelated() const override;

  // If the iframe is lazy-loaded, initiate its load, and return true if such
  // a load was initiated.
  bool LoadImmediatelyIfLazy();

  void Trace(Visitor*) const override;

 protected:
  HTMLFrameOwnerElement(const QualifiedName& tag_name, Document&);

  void SetSandboxFlags(network::mojom::blink::WebSandboxFlags);

  bool LoadOrRedirectSubframe(const KURL&,
                              const AtomicString& frame_name,
                              bool replace_current_item);
  bool IsKeyboardFocusable() const override;
  void FrameOwnerPropertiesChanged() override;

  void DisposePluginSoon(WebPluginContainerImpl*);

  // Return the origin which is to be used for permissions policy container
  // policies, as "the origin of the URL in the frame's src attribute" (see
  // https://w3c.github.io/webappsec-permissions-policy/#iframe-allow-attribute).
  // This method is intended to be overridden by specific frame classes.
  virtual scoped_refptr<const SecurityOrigin> GetOriginForPermissionsPolicy()
      const {
    return SecurityOrigin::CreateUniqueOpaque();
  }

  // Return a permissions policy container policy for this frame, based on the
  // frame attributes and the effective origin specified in the frame
  // attributes.
  virtual ParsedPermissionsPolicy ConstructContainerPolicy() const = 0;

  // Update the container policy and notify the frame loader client of any
  // changes.
  void UpdateContainerPolicy();

  // Called when the container policy changes. Typically used to sync a
  // container policy update to the browser process.
  virtual void DidChangeContainerPolicy();

  // Return a document policy required policy for this frame, based on the
  // frame attributes.
  virtual DocumentPolicyFeatureState ConstructRequiredPolicy() const {
    return DocumentPolicyFeatureState{};
  }

  // Update the required policy and notify the frame loader client of any
  // changes.
  void UpdateRequiredPolicy();

  // Return a set of Trust Tokens parameters for requests for this frame,
  // based on the frame attributes.
  virtual network::mojom::blink::TrustTokenParamsPtr ConstructTrustTokenParams()
      const;
  void ReportFallbackResourceTimingIfNeeded();

 protected:
  bool is_swapping_frames() const { return is_swapping_frames_; }

  // Checks that the number of frames on the page are within the current limit.
  bool IsCurrentlyWithinFrameLimit() const;

 private:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // This enum represents which auto lazy-load mechanism is used.
  enum class AutomaticLazyLoadReason {
    // If the frame is neither embeds nor ads, or the flags are not enabled,
    // mark it as not eligible.
    kNotEligible = 0,
    // For LazyEmbeds
    kEmbeds = 1,
    // For LazyAds
    kAds = 2,
    // It's possible that the frame is eligible for both LazyEmbeds and LazyAds.
    // TOOD(crbug.com/1341892) Remove kBothEmbedsAndAds once we confirm that we
    // can ignore
    // this case because the impact on the analysis is minimal.
    kBothEmbedsAndAds = 3,

    kMaxValue = kBothEmbedsAndAds,
  };

  // Intentionally private to prevent redundant checks when the type is
  // already HTMLFrameOwnerElement.
  bool IsLocal() const final { return true; }
  bool IsRemote() const final { return false; }
  bool IsFrameOwnerElement() const final { return true; }
  void SetIsSwappingFrames(bool is_swapping) override {
    is_swapping_frames_ = is_swapping;
  }

  // Checks if the passed `url` is eligible for automatic lazy-loading.
  // Also this method checks the url is cross-origin or not.
  bool IsEligibleForLazyEmbeds(const KURL& url) const;
  bool IsEligibleForLazyAds(const KURL& url);
  void MaybeSetTimeoutToStartFrameLoading(
      const KURL& url,
      bool is_loading_attr_lazy,
      AutomaticLazyLoadReason auto_lazy_load_reason);

  // Check if the frame should be lazy-loaded and apply when conditions are
  // passed. Return true when lazy-load is applied.
  bool LazyLoadIfPossible(const KURL&,
                          const ResourceRequestHead&,
                          WebFrameLoadType frame_load_type);

  virtual network::mojom::ReferrerPolicy ReferrerPolicyAttribute() {
    return network::mojom::ReferrerPolicy::kDefault;
  }

  Member<Frame> content_frame_;
  Member<EmbeddedContentView> embedded_content_view_;
  FramePolicy frame_policy_;

  Member<LazyLoadFrameObserver> lazy_load_frame_observer_;
  mojom::blink::ResourceTimingInfoPtr fallback_timing_info_;
  bool should_lazy_load_children_;
  bool is_swapping_frames_{false};
};

class SubframeLoadingDisabler {
  STACK_ALLOCATED();

 public:
  explicit SubframeLoadingDisabler(Node& root)
      : SubframeLoadingDisabler(&root) {}

  explicit SubframeLoadingDisabler(Node* root) : root_(root) {
    if (root_)
      DisabledSubtreeRoots().insert(root_);
  }

  ~SubframeLoadingDisabler() {
    if (root_)
      DisabledSubtreeRoots().erase(root_);
  }

  static bool CanLoadFrame(HTMLFrameOwnerElement& owner) {
    for (Node* node = &owner; node; node = node->ParentOrShadowHostNode()) {
      if (DisabledSubtreeRoots().Contains(node))
        return false;
    }
    return true;
  }

 private:
  // The use of UntracedMember<Node>  is safe as all SubtreeRootSet
  // references are on the stack and reachable in case a conservative
  // GC hits.
  // TODO(sof): go back to HeapHashSet<> once crbug.com/684551 has been
  // resolved.
  using SubtreeRootSet = HashCountedSet<UntracedMember<Node>>;

  CORE_EXPORT static SubtreeRootSet& DisabledSubtreeRoots();

  Node* root_;
};

template <>
struct DowncastTraits<HTMLFrameOwnerElement> {
  static bool AllowFrom(const FrameOwner& owner) { return owner.IsLocal(); }
  static bool AllowFrom(const Node& node) { return node.IsFrameOwnerElement(); }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_FRAME_OWNER_ELEMENT_H_
