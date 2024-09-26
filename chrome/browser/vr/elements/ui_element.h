// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_UI_ELEMENT_H_
#define CHROME_BROWSER_VR_ELEMENTS_UI_ELEMENT_H_

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/vr/audio_delegate.h"
#include "chrome/browser/vr/databinding/binding_base.h"
#include "chrome/browser/vr/elements/corner_radii.h"
#include "chrome/browser/vr/elements/draw_phase.h"
#include "chrome/browser/vr/elements/ui_element_name.h"
#include "chrome/browser/vr/elements/ui_element_type.h"
#include "chrome/browser/vr/frame_lifecycle.h"
#include "chrome/browser/vr/model/camera_model.h"
#include "chrome/browser/vr/model/reticle_model.h"
#include "chrome/browser/vr/model/sounds.h"
#include "chrome/browser/vr/target_property.h"
#include "chrome/browser/vr/vr_ui_export.h"
#include "ui/gfx/animation/keyframe/animation_curve.h"
#include "ui/gfx/animation/keyframe/keyframe_effect.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_operations.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace base {
class TimeTicks;
}

namespace vr {

class KeyframeModel;
class SkiaSurfaceProvider;
class UiElementRenderer;
class InputEvent;
struct CameraModel;
struct EditedText;

enum LayoutAlignment {
  NONE = 0,
  LEFT,
  RIGHT,
  TOP,
  BOTTOM,
};

struct VR_UI_EXPORT EventHandlers {
  EventHandlers();
  EventHandlers(const EventHandlers& other);
  ~EventHandlers();
  base::RepeatingCallback<void()> hover_enter;
  base::RepeatingCallback<void()> hover_leave;
  base::RepeatingCallback<void(const gfx::PointF&)> hover_move;
  base::RepeatingCallback<void()> button_down;
  base::RepeatingCallback<void()> button_up;
  base::RepeatingCallback<void(const gfx::PointF&)> touch_move;
  base::RepeatingCallback<void(bool)> focus_change;
};

struct HitTestRequest {
  gfx::Point3F ray_origin;
  gfx::Point3F ray_target;
  float max_distance_to_plane = 1000.f;
};

// The result of performing a hit test.
struct HitTestResult {
  enum class Type {
    // The given ray does not pass through the element.
    kNone = 0,
    // The given ray does not pass through the element, but passes through the
    // element's plane.
    kHitsPlane,
    // The given ray passes through the element.
    kHits,
  };

  Type type;
  // The fields below are not set if the result Type is kNone.
  // The hit position in the element's local coordinate space.
  gfx::PointF local_hit_point;
  // The hit position relative to the world.
  gfx::Point3F hit_point;
  // The distance from the ray origin to the hit position.
  float distance_to_plane;
};

class VR_UI_EXPORT UiElement : public gfx::FloatAnimationCurve::Target,
                               public gfx::TransformAnimationCurve::Target,
                               public gfx::SizeAnimationCurve::Target,
                               public gfx::ColorAnimationCurve::Target {
 public:
  UiElement();

  UiElement(const UiElement&) = delete;
  UiElement& operator=(const UiElement&) = delete;

  ~UiElement() override;

  enum OperationIndex {
    kTranslateIndex = 0,
    kRotateIndex = 1,
    kScaleIndex = 2,
  };

  UiElementName name() const { return name_; }
  void SetName(UiElementName name);
  virtual void OnSetName();

  UiElementName owner_name_for_test() const { return owner_name_for_test_; }
  void set_owner_name_for_test(UiElementName name) {
    owner_name_for_test_ = name;
  }

  UiElementType type() const { return type_; }
  void SetType(UiElementType type);
  virtual void OnSetType();
  UiElement* GetDescendantByType(UiElementType type);

  DrawPhase draw_phase() const { return draw_phase_; }
  void SetDrawPhase(DrawPhase draw_phase);
  virtual void OnSetDrawPhase();

  void UpdateBindings();

  // Returns true if the element has been updated in any visible way.
  bool DoBeginFrame(const gfx::Transform& head_pose,
                    bool force_animations_to_completion);

  // Returns true if the element has changed size or position, or otherwise
  // warrants re-rendering the scene.
  virtual bool PrepareToDraw();

  // Returns true if the element updated its texture.
  virtual bool HasDirtyTexture() const;

  virtual void UpdateTexture();

  bool IsHitTestable() const;

  virtual void Render(UiElementRenderer* renderer,
                      const CameraModel& model) const;

  virtual void Initialize(SkiaSurfaceProvider* provider);

  // Controller interaction methods.
  virtual void OnHoverEnter(const gfx::PointF& position,
                            base::TimeTicks timestamp);
  virtual void OnHoverLeave(base::TimeTicks timestamp);
  virtual void OnHoverMove(const gfx::PointF& position,
                           base::TimeTicks timestamp);
  virtual void OnButtonDown(const gfx::PointF& position,
                            base::TimeTicks timestamp);
  virtual void OnButtonUp(const gfx::PointF& position,
                          base::TimeTicks timestamp);
  virtual void OnTouchMove(const gfx::PointF& position,
                           base::TimeTicks timestamp);
  virtual void OnFlingCancel(std::unique_ptr<InputEvent> gesture,
                             const gfx::PointF& position);
  virtual void OnScrollBegin(std::unique_ptr<InputEvent> gesture,
                             const gfx::PointF& position);
  virtual void OnScrollUpdate(std::unique_ptr<InputEvent> gesture,
                              const gfx::PointF& position);
  virtual void OnScrollEnd(std::unique_ptr<InputEvent> gesture,
                           const gfx::PointF& position);

  // Whether the point (relative to the origin of the element), should be
  // considered on the element. All elements are considered rectangular by
  // default though elements may override this function to handle arbitrary
  // shapes. Points within the rectangular area are mapped from 0:1 as follows,
  // though will extend outside this range when outside of the element:
  // [(0.0, 0.0), (1.0, 0.0)
  //  (0.0, 1.0), (1.0, 1.0)]
  virtual bool LocalHitTest(const gfx::PointF& point) const;

  // Performs a hit test for the ray supplied in the request and populates the
  // result. The ray is in the world coordinate space.
  virtual void HitTest(const HitTestRequest& request,
                       HitTestResult* result) const;

  int id() const { return id_; }

  // If true, the object has a non-zero opacity.
  bool IsVisible() const;

  // If true, the object is both visible and opaque.
  bool IsVisibleAndOpaque() const;

  // For convenience, sets opacity to |opacity_when_visible_|.
  virtual void SetVisible(bool visible);
  virtual void SetVisibleImmediately(bool visible);

  void set_opacity_when_visible(float opacity) {
    opacity_when_visible_ = opacity;
  }
  float opacity_when_visible() const { return opacity_when_visible_; }

  bool requires_layout() const { return requires_layout_; }
  void set_requires_layout(bool requires_layout) {
    requires_layout_ = requires_layout;
  }

  bool hit_testable() const { return hit_testable_; }
  void set_hit_testable(bool hit_testable) { hit_testable_ = hit_testable; }

  bool focusable() const { return focusable_; }
  void set_focusable(bool focusable);
  virtual void OnSetFocusable();

  bool scrollable() const { return scrollable_; }
  void set_scrollable(bool scrollable) { scrollable_ = scrollable; }

  bool bubble_events() const { return bubble_events_; }
  void set_bubble_events(bool bubble_events) { bubble_events_ = bubble_events; }

  void set_event_handlers(const EventHandlers& event_handlers) {
    event_handlers_ = event_handlers;
  }

  // Editable elements should override these functions.
  virtual void OnFocusChanged(bool focused);
  virtual void OnInputEdited(const EditedText& info);
  virtual void OnInputCommitted(const EditedText& info);
  virtual void RequestFocus();
  virtual void RequestUnfocus();
  virtual void UpdateInput(const EditedText& info);

  gfx::SizeF size() const;
  void SetSize(float width, float hight);
  virtual void OnSetSize(const gfx::SizeF& size);

  // Setter and getter for the clip rect in relative tex coordinates, the same
  // system used for hit testing.
  gfx::RectF GetClipRect() const;
  void SetClipRect(const gfx::RectF& rect);

  gfx::PointF local_origin() const { return local_origin_; }

  // These are convenience functions for setting the transform operations. They
  // will animate if you've set a transition. If you need to animate more than
  // one operation simultaneously, please use |SetTransformOperations| below.
  void SetLayoutOffset(float x, float y);
  void SetTranslate(float x, float y, float z);
  void SetRotate(float x, float y, float z, float radians);
  void SetScale(float x, float y, float z);

  // Returns the target value of the animation if the corresponding property is
  // being animated, or the current value otherwise.
  gfx::SizeF GetTargetSize() const;
  gfx::TransformOperations GetTargetTransform() const;
  float GetTargetOpacity() const;

  float opacity() const { return opacity_; }
  virtual void SetOpacity(float opacity);

  CornerRadii corner_radii() const { return corner_radii_; }
  void SetCornerRadii(const CornerRadii& radii);
  virtual void OnSetCornerRadii(const CornerRadii& radii);

  float corner_radius() const {
    DCHECK(corner_radii_.AllEqual());
    return corner_radii_.upper_left;
  }

  // Syntax sugar for setting all corner radii to the same value.
  void SetCornerRadius(float corner_radius) {
    SetCornerRadii(
        {corner_radius, corner_radius, corner_radius, corner_radius});
  }

  float computed_opacity() const;
  void set_computed_opacity(float computed_opacity) {
    computed_opacity_ = computed_opacity;
  }

  virtual float ComputedAndLocalOpacityForTest() const;

  LayoutAlignment x_anchoring() const { return x_anchoring_; }
  void set_x_anchoring(LayoutAlignment x_anchoring) {
    DCHECK(x_anchoring == LEFT || x_anchoring == RIGHT || x_anchoring == NONE);
    x_anchoring_ = x_anchoring;
  }

  LayoutAlignment y_anchoring() const { return y_anchoring_; }
  void set_y_anchoring(LayoutAlignment y_anchoring) {
    DCHECK(y_anchoring == TOP || y_anchoring == BOTTOM || y_anchoring == NONE);
    y_anchoring_ = y_anchoring;
  }

  LayoutAlignment x_centering() const { return x_centering_; }
  void set_x_centering(LayoutAlignment x_centering) {
    DCHECK(x_centering == LEFT || x_centering == RIGHT || x_centering == NONE);
    x_centering_ = x_centering;
  }

  LayoutAlignment y_centering() const { return y_centering_; }
  void set_y_centering(LayoutAlignment y_centering) {
    DCHECK(y_centering == TOP || y_centering == BOTTOM || y_centering == NONE);
    y_centering_ = y_centering;
  }

  bool bounds_contain_children() const { return bounds_contain_children_; }
  void set_bounds_contain_children(bool bounds_contain_children) {
    bounds_contain_children_ = bounds_contain_children;
  }

  bool bounds_contain_padding() const { return bounds_contain_padding_; }
  void set_bounds_contain_padding(bool bounds_contain_padding) {
    bounds_contain_padding_ = bounds_contain_padding;
  }

  bool contributes_to_parent_bounds() const {
    return contributes_to_parent_bounds_;
  }
  void set_contributes_to_parent_bounds(bool value) {
    contributes_to_parent_bounds_ = value;
  }

  float left_padding() const { return left_padding_; }
  float right_padding() const { return right_padding_; }
  float top_padding() const { return top_padding_; }
  float bottom_padding() const { return bottom_padding_; }

  void set_padding(float x, float y) {
    left_padding_ = x;
    right_padding_ = x;
    top_padding_ = y;
    bottom_padding_ = y;
  }

  void set_padding(float left, float top, float right, float bottom) {
    left_padding_ = left;
    right_padding_ = right;
    top_padding_ = top;
    bottom_padding_ = bottom;
  }

  const gfx::Transform& inheritable_transform() const {
    return inheritable_transform_;
  }
  void set_inheritable_transform(const gfx::Transform& transform) {
    inheritable_transform_ = transform;
  }

  const gfx::Transform& world_space_transform() const;
  void set_world_space_transform(const gfx::Transform& transform) {
    world_space_transform_ = transform;
    world_space_transform_dirty_ = false;
  }

  gfx::Transform ComputeTargetWorldSpaceTransform() const;
  float ComputeTargetOpacity() const;

  // Transformations are applied relative to the parent element, rather than
  // absolutely.
  void AddChild(std::unique_ptr<UiElement> child);

  // These functions return the removed element.
  std::unique_ptr<UiElement> RemoveChild(UiElement* to_remove);
  std::unique_ptr<UiElement> ReplaceChild(UiElement* to_remove,
                                          std::unique_ptr<UiElement> to_add);

  UiElement* parent() { return parent_; }
  const UiElement* parent() const { return parent_; }

  void AddBinding(std::unique_ptr<BindingBase> binding);
  const std::vector<std::unique_ptr<BindingBase>>& bindings() {
    return bindings_;
  }

  gfx::Point3F GetCenter() const;
  gfx::Vector3dF GetNormal() const;

  // Computes the distance from |ray_origin| to this rectangles's plane, along
  // |ray_vector|. Returns true and populates |distance| if the calculation is
  // possible, and false if the ray is parallel to the plane.
  bool GetRayDistance(const gfx::Point3F& ray_origin,
                      const gfx::Vector3dF& ray_vector,
                      float* distance) const;

  // Projects a 3D world point onto the X and Y axes of the transformed
  // rectangle, returning 2D coordinates relative to the un-transformed unit
  // rectangle. This allows beam intersection points to be mapped to sprite
  // pixel coordinates. Points that fall onto the rectangle will generate X and
  // Y values on the interval [-0.5, 0.5].
  gfx::PointF GetUnitRectangleCoordinates(
      const gfx::Point3F& world_point) const;

  void OnFloatAnimated(const float& value,
                       int target_property_id,
                       gfx::KeyframeModel* keyframe_model) override;
  void OnTransformAnimated(const gfx::TransformOperations& operations,
                           int target_property_id,
                           gfx::KeyframeModel* keyframe_model) override;
  void OnSizeAnimated(const gfx::SizeF& size,
                      int target_property_id,
                      gfx::KeyframeModel* keyframe_model) override;
  void OnColorAnimated(const SkColor& size,
                       int target_property_id,
                       gfx::KeyframeModel* keyframe_model) override;

  void SetTransitionedProperties(const std::set<TargetProperty>& properties);
  void SetTransitionDuration(base::TimeDelta delta);

  void AddKeyframeModel(std::unique_ptr<gfx::KeyframeModel> keyframe_model);
  void RemoveKeyframeModel(int keyframe_model_id);
  void RemoveKeyframeModels(int target_property);
  bool IsAnimatingProperty(TargetProperty property) const;

  // Recursive method that sizes and lays out element subtrees.
  bool SizeAndLayOut();

  // Handles positioning adjustments for children. This will be overridden by
  // UiElements providing custom layout modes. See the documentation of the
  // override for their particular functionality.  This method is specific to
  // children that contribute to the parent's bounds.  These elements may not
  // use anchoring or centering attributes, as they themselves determine where
  // the parent boundaries will be.
  virtual void LayOutContributingChildren();

  // Similar to LayOutContributingChildren, but runs after the parent's size has
  // been determined.  The default implementation applies anchoring.
  virtual void LayOutNonContributingChildren();

  // Recursive method that clips element subtrees, using the clip rect set.
  void ClipChildren();

  UiElement* FirstLaidOutChild() const;
  UiElement* LastLaidOutChild() const;

  virtual gfx::Transform LocalTransform() const;
  virtual gfx::Transform GetTargetLocalTransform() const;

  void UpdateComputedOpacity();
  bool UpdateWorldSpaceTransform(bool parent_changed);

  std::vector<std::unique_ptr<UiElement>>& children() { return children_; }
  const std::vector<std::unique_ptr<UiElement>>& children() const {
    return children_;
  }

  void set_update_phase(UpdatePhase phase) { update_phase_ = phase; }
  UpdatePhase update_phase() const { return update_phase_; }

  // This is true for all elements that respect the given view model matrix. If
  // this is ignored (say for head-locked elements that draw in screen space),
  // then this function should return false.
  virtual bool IsWorldPositioned() const;

  bool updated_visiblity_this_frame() const {
    return updated_visibility_this_frame_;
  }

  void set_cursor_type(CursorType cursor_type) { cursor_type_ = cursor_type; }
  CursorType cursor_type() const { return cursor_type_; }

  std::string DebugName() const;

#ifndef NDEBUG

  // Writes a pretty-printed version of the UiElement subtree to |os|. The
  // vector of counts represents where each ancestor on the ancestor chain is
  // situated in its parent's list of children. This is used to determine
  // whether each ancestor is the last child (which affects the lines we draw in
  // the tree).
  // TODO(vollick): generalize the configuration of the dump to selectively turn
  // off or on a variety of features.
  void DumpHierarchy(std::vector<size_t> counts,
                     std::ostringstream* os,
                     bool include_bindings) const;
  virtual void DumpGeometry(std::ostringstream* os) const;
#endif

  // Set the sounds that play when an applicable handler is executed.  Elements
  // that override element hover and click methods must manage their own sounds.
  void SetSounds(Sounds sounds, AudioDelegate* delegate);

  bool clips_descendants() const { return clips_descendants_; }
  void set_clip_descendants(bool clips) { clips_descendants_ = clips; }

  bool resizable_by_layout() const { return resizable_by_layout_; }
  void set_resizable_by_layout(bool resizable) {
    resizable_by_layout_ = resizable;
  }

  bool descendants_updated() const { return descendants_updated_; }
  void set_descendants_updated(bool updated) { descendants_updated_ = updated; }

  base::TimeTicks last_frame_time() const { return last_frame_time_; }
  void set_last_frame_time(const base::TimeTicks& time) {
    last_frame_time_ = time;
  }

 protected:
  // This method may be overridden by elements that have custom layout
  // requirements.
  virtual bool SizeAndLayOutChildren();

  gfx::RectF GetAbsoluteClipRect() const;

  gfx::KeyframeEffect& animator() { return animator_; }

  virtual const Sounds& GetSounds() const;

  virtual bool ShouldUpdateWorldSpaceTransform(
      bool parent_transform_changed) const;

  void set_world_space_transform_dirty() {
    world_space_transform_dirty_ = true;
  }

  EventHandlers event_handlers_;

 private:
  virtual void OnUpdatedWorldSpaceTransform();

  // Returns true if the element has been updated in any visible way.
  virtual bool OnBeginFrame(const gfx::Transform& head_pose);

  // If true, the element is either locally visible (independent of its
  // ancestors), or its animation will cause it to become locally visible.
  bool IsOrWillBeLocallyVisible() const;

  // Recursive method that clips element subtrees, given that a parent is
  // clipped. Receives a clipping rect in absolute scale.
  void ClipChildren(const gfx::RectF& abs_clip);

  virtual gfx::RectF ComputeContributingChildrenBounds();

  // Valid IDs are non-negative.
  int id_ = -1;

  // If false, the reticle will not hit the element, even if visible.
  bool hit_testable_ = false;

  // If false, clicking on the element doesn't give it focus.
  bool focusable_ = true;

  // A signal to the input routing machinery that this element accepts scrolls.
  bool scrollable_ = false;

  // If true, events such as OnButtonDown, OnHoverEnter, etc, get bubbled up the
  // parent chain.
  bool bubble_events_ = false;

  // The size of the object.  This does not affect children.
  gfx::SizeF size_;

  // The clip of the object. The rect dimensions are relative to the element's
  // size, with the origin at its center. Use the getter and setter to
  // manipulate the rect in relative tex coordinates.
  gfx::RectF clip_rect_ = {-0.5f, 0.5f, 1.0f, 1.0f};

  // Indicates that this element clips its descendants with its size.
  bool clips_descendants_ = false;

  // The local orgin of the element. This can be updated, say, so that an
  // element can contain its children, even if they are not centered about its
  // true origin.
  gfx::PointF local_origin_ = {0.0f, 0.0f};

  // The opacity of the object (between 0.0 and 1.0).
  float opacity_ = 1.0f;

  // SetVisible(true) is an alias for SetOpacity(opacity_when_visible_).
  float opacity_when_visible_ = 1.0f;

  // A signal that this element is to be considered in |LayOutChildren|.
  bool requires_layout_ = true;

  // The corner radius of the object. Analogous to the CSS property,
  // border-radius. This is in meters (same units as |size|).
  CornerRadii corner_radii_ = {0, 0, 0, 0};

  // The computed opacity, incorporating opacity of parent objects.
  float computed_opacity_ = 1.0f;

  // Returns true if the last call to UpdateBindings had any effect. NB: this
  // value is *not* updated for all elements in the tree each frame. It is
  // important to only query this value for elements whose visibility has
  // changed this frame or will be visible.
  bool updated_bindings_this_frame_ = false;

  // Return true if the last call to UpdateComputedOpacity had any effect on
  // visibility.
  bool updated_visibility_this_frame_ = false;

  // If anchoring is specified, the translation will be relative to the
  // specified edge(s) of the parent, rather than the center.  A parent object
  // must be specified when using anchoring.
  LayoutAlignment x_anchoring_ = LayoutAlignment::NONE;
  LayoutAlignment y_anchoring_ = LayoutAlignment::NONE;

  // If centering is specified, the elements layout offset is adjusted such that
  // it is positioned relative to its own edge or corner, rather than center.
  LayoutAlignment x_centering_ = LayoutAlignment::NONE;
  LayoutAlignment y_centering_ = LayoutAlignment::NONE;

  // If this is true, after laying out descendants, this element updates its
  // size to accommodate all descendants, adding in the padding below along the
  // x and y axes.
  bool bounds_contain_children_ = false;
  bool bounds_contain_padding_ = true;
  bool contributes_to_parent_bounds_ = true;
  float left_padding_ = 0.0f;
  float right_padding_ = 0.0f;
  float top_padding_ = 0.0f;
  float bottom_padding_ = 0.0f;

  gfx::KeyframeEffect animator_;

  DrawPhase draw_phase_ = kPhaseNone;

  // The time of the most recent frame.
  base::TimeTicks last_frame_time_;

  // This transform can be used by children to derive position of its parent.
  gfx::Transform inheritable_transform_;

  // An optional, but stable and semantic identifier for an element used in lieu
  // of a string.
  UiElementName name_ = UiElementName::kNone;

  // This name is used in tests and debugging output to associate a "component"
  // element with its logical owner, such as a button icon within a specific,
  // named button instance.
  UiElementName owner_name_for_test_ = UiElementName::kNone;

  // An optional identifier to categorize a reusable element, such as a button
  // background. It can also be used to identify categories of element for
  // common styling. Eg, applying a corner-radius to all tab thumbnails.
  UiElementType type_ = UiElementType::kTypeNone;

  // This local transform operations. They are inherited by descendants and are
  // stored as a list of operations rather than a baked transform to make
  // transitions easier to implement (you may, for example, want to animate just
  // the translation, but leave the rotation and scale in tact).
  gfx::TransformOperations transform_operations_;

  // This is a cached version of the local transform.
  gfx::Transform local_transform_;

  // This is set by the parent and is combined into LocalTransform()
  gfx::TransformOperations layout_offset_;

  // This is the combined, local to world transform. It includes
  // |inheritable_transform_|, |transform_|, and anchoring adjustments.
  gfx::Transform world_space_transform_;
  bool world_space_transform_dirty_ = false;

  raw_ptr<UiElement> parent_ = nullptr;
  std::vector<std::unique_ptr<UiElement>> children_;

  // This is true if a descendant has been added and the total list has not yet
  // been collected by the scene.
  bool descendants_updated_ = false;

  std::vector<std::unique_ptr<BindingBase>> bindings_;

  UpdatePhase update_phase_ = kClean;

  raw_ptr<AudioDelegate> audio_delegate_ = nullptr;
  Sounds sounds_;

  // Indicates that this element may be resized by parent layout elements.
  bool resizable_by_layout_ = false;

  CursorType cursor_type_ = kCursorDefault;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_UI_ELEMENT_H_
