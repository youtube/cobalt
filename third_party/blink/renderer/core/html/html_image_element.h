/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_IMAGE_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_IMAGE_ELEMENT_H_

#include <memory>

#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/create_element_flags.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/canvas/image_element_base.h"
#include "third_party/blink/renderer/core/html/forms/form_associated.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_image_loader.h"
#include "third_party/blink/renderer/core/html/lazy_load_image_observer.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/graphics/image_orientation.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

class ExceptionState;
class HTMLFormElement;
class ImageCandidate;
class LayoutSize;
class ShadowRoot;

class CORE_EXPORT HTMLImageElement final
    : public HTMLElement,
      public ImageElementBase,
      public ActiveScriptWrappable<HTMLImageElement>,
      public FormAssociated,
      public LocalFrameView::LifecycleNotificationObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  class ViewportChangeListener;

  static HTMLImageElement* CreateForJSConstructor(Document&);
  static HTMLImageElement* CreateForJSConstructor(Document&, unsigned width);
  static HTMLImageElement* CreateForJSConstructor(Document&,
                                                  unsigned width,
                                                  unsigned height);

  // Returns dimension type of the attribute value or inline dimensions usable
  // for LazyLoad, whether the dimension is absolute or not and if the absolute
  // value is small enough to be skipped for lazyloading.
  enum class LazyLoadDimensionType {
    kNotAbsolute,
    kAbsoluteNotSmall,
    kAbsoluteSmall,
  };
  static LazyLoadDimensionType GetAttributeLazyLoadDimensionType(
      const String& attribute_value);
  static LazyLoadDimensionType GetInlineStyleDimensionsType(
      const CSSPropertyValueSet* property_set);

  HTMLImageElement(Document&, const CreateElementFlags);
  explicit HTMLImageElement(Document&, bool created_by_parser = false);
  ~HTMLImageElement() override;
  void Trace(Visitor*) const override;

  unsigned width();
  unsigned height();

  unsigned naturalWidth() const;
  unsigned naturalHeight() const;

  unsigned LayoutBoxWidth() const;
  unsigned LayoutBoxHeight() const;

  const String& currentSrc() const;

  bool IsServerMap() const;

  String AltText() const final;

  ImageResourceContent* CachedImage() const {
    return GetImageLoader().GetContent();
  }
  void LoadDeferredImageFromMicrotask() {
    GetImageLoader().LoadDeferredImage(/*force_blocking*/ false,
                                       /*update_from_microtask*/ true);
  }
  void LoadDeferredImageBlockingLoad() {
    GetImageLoader().LoadDeferredImage(/*force_blocking*/ true);
  }
  void SetImageForTest(ImageResourceContent* content) {
    GetImageLoader().SetImageForTest(content);
  }

  void StartLoadingImageDocument(ImageResourceContent* image_content);

  void setHeight(unsigned);
  void setWidth(unsigned);

  bool IsDefaultIntrinsicSize() const {
    return is_default_overridden_intrinsic_size_;
  }

  int x() const;
  int y() const;

  ScriptPromise decode(ScriptState*, ExceptionState&);

  bool complete() const;

  bool HasPendingActivity() const final {
    return GetImageLoader().HasPendingActivity();
  }

  bool CanContainRangeEndPoint() const override { return false; }

  const AtomicString ImageSourceURL() const override;

  HTMLFormElement* formOwner() const override;
  void FormRemovedFromTree(const Node& form_root);
  virtual void EnsureCollapsedOrFallbackContent();
  virtual void EnsureFallbackForGeneratedContent();
  virtual void EnsurePrimaryContent();
  bool IsCollapsed() const;

  void SetAutoSizesUsecounter();

  // CanvasImageSource interface implementation.
  gfx::SizeF DefaultDestinationSize(
      const gfx::SizeF&,
      const RespectImageOrientationEnum) const override;

  // public so that HTMLPictureElement can call this as well.
  void SelectSourceURL(ImageLoader::UpdateFromElementBehavior);

  void SetIsFallbackImage() { is_fallback_image_ = true; }

  absl::optional<float> GetResourceWidth() const;
  float SourceSize(Element&);

  void ForceReload() const;

  FormAssociated* ToFormAssociatedOrNull() override { return this; }
  void AssociateWith(HTMLFormElement*) override;

  bool ElementCreatedByParser() const { return element_created_by_parser_; }

  LazyLoadImageObserver::VisibleLoadTimeMetrics&
  EnsureVisibleLoadTimeMetrics() {
    if (!visible_load_time_metrics_) {
      visible_load_time_metrics_ =
          std::make_unique<LazyLoadImageObserver::VisibleLoadTimeMetrics>();
    }
    return *visible_load_time_metrics_;
  }

  // Updates if any optimized image policy is violated. When any policy is
  // violated, the image should be rendered as a placeholder image.
  void SetImagePolicyViolated() {
    is_legacy_format_or_unoptimized_image_ = true;
  }
  bool IsImagePolicyViolated() {
    return is_legacy_format_or_unoptimized_image_;
  }

  // Keeps track of whether the image comes from an ad.
  void SetIsAdRelated();
  bool IsAdRelated() const override { return is_ad_related_; }

  // Keeps track whether this image is an LCP element.
  void SetIsLCPElement() { is_lcp_element_ = true; }
  bool IsLCPElement() const { return is_lcp_element_; }

  bool IsChangedShortlyAfterMouseover() const {
    return is_changed_shortly_after_mouseover_;
  }

  void InvalidateAttributeMapping();

  bool IsRichlyEditableForAccessibility() const override { return false; }

  static bool SupportedImageType(const String& type,
                                 const HashSet<String>* disabled_image_types);

  bool is_lazy_loaded() const { return is_lazy_loaded_; }

 protected:
  // Controls how an image element appears in the layout. See:
  // https://html.spec.whatwg.org/C/#image-request
  enum class LayoutDisposition : uint8_t {
    // Displayed as a partially or completely loaded image. Corresponds to the
    // `current request` state being: `unavailable`, `partially available`, or
    // `completely available`.
    kPrimaryContent,
    // Showing a broken image icon and 'alt' text, if any. Corresponds to the
    // `current request` being in the `broken` state.
    kFallbackContent,
    // No layout object. Corresponds to the `current request` being in the
    // `broken` state when the resource load failed with an error that has the
    // |shouldCollapseInitiator| flag set.
    kCollapsed
  };

  void DidMoveToNewDocument(Document& old_document) override;

  void DidAddUserAgentShadowRoot(ShadowRoot&) override;
  void AdjustStyle(ComputedStyleBuilder&) override;

 private:
  bool AreAuthorShadowsAllowed() const override { return false; }

  void ParseAttribute(const AttributeModificationParams&) override;
  bool IsPresentationAttribute(const QualifiedName&) const override;
  void CollectStyleForPresentationAttribute(
      const QualifiedName&,
      const AtomicString&,
      MutableCSSPropertyValueSet*) override;
  // For mapping attributes from the <source> element, if any.
  bool HasExtraStyleForPresentationAttribute() const override {
    return source_;
  }
  void CollectExtraStyleForPresentationAttribute(
      MutableCSSPropertyValueSet*) override;
  void SetLayoutDisposition(LayoutDisposition, bool force_reattach = false);

  void AttachLayoutTree(AttachContext&) override;
  LayoutObject* CreateLayoutObject(const ComputedStyle&) override;

  bool CanStartSelection() const override { return false; }

  bool IsURLAttribute(const Attribute&) const override;
  bool HasLegalLinkAttribute(const QualifiedName&) const override;
  const QualifiedName& SubResourceAttributeName() const override;

  bool draggable() const override;

  InsertionNotificationRequest InsertedInto(ContainerNode&) override;
  void RemovedFrom(ContainerNode&) override;
  NamedItemType GetNamedItemType() const override {
    return NamedItemType::kNameOrIdWithName;
  }
  bool IsInteractiveContent() const override;
  Image* ImageContents() override;

  void ResetFormOwner();
  ImageCandidate FindBestFitImageFromPictureParent();
  void SetBestFitURLAndDPRFromImageCandidate(const ImageCandidate&);
  LayoutSize DensityCorrectedIntrinsicDimensions() const;
  HTMLImageLoader& GetImageLoader() const override { return *image_loader_; }
  void NotifyViewportChanged();
  void CreateMediaQueryListIfDoesNotExist();

  // LocalFrameView::LifecycleNotificationObserver
  void DidFinishLifecycleUpdate(const LocalFrameView&) override;

  Member<HTMLImageLoader> image_loader_;
  Member<ViewportChangeListener> listener_;
  Member<HTMLFormElement> form_;
  AtomicString best_fit_image_url_;
  float image_device_pixel_ratio_;
  Member<HTMLSourceElement> source_;
  LayoutDisposition layout_disposition_;
  bool form_was_set_by_parser_ : 1;
  bool element_created_by_parser_ : 1;
  bool is_fallback_image_ : 1;
  bool is_default_overridden_intrinsic_size_ : 1;
  // This flag indicates if the image violates one or more optimized image
  // policies. When any policy is violated, the image should be rendered as a
  // placeholder image.
  bool is_legacy_format_or_unoptimized_image_ : 1;
  bool is_ad_related_ : 1;
  bool is_lcp_element_ : 1;
  bool is_changed_shortly_after_mouseover_ : 1;
  bool has_sizes_attribute_in_img_or_sibling_ : 1;
  bool is_lazy_loaded_ : 1;

  std::unique_ptr<LazyLoadImageObserver::VisibleLoadTimeMetrics>
      visible_load_time_metrics_;

  bool image_ad_use_counter_recorded_ = false;

  // The last rectangle reported to the the `PageTimingMetricsSender`.
  // `last_reported_ad_rect_` is empty if there's no report before, or if the
  // last report was used to signal the removal of this element (i.e. both cases
  // will be handled the same way).
  gfx::Rect last_reported_ad_rect_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_IMAGE_ELEMENT_H_
