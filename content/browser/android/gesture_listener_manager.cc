// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/raw_ptr.h"
#include "content/browser/android/gesture_listener_manager.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view_android.h"
#include "content/public/android/content_jni_headers/GestureListenerManagerImpl_jni.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom-forward.h"
#include "ui/events/android/gesture_event_type.h"
#include "ui/events/blink/did_overscroll_params.h"
#include "ui/gfx/geometry/size_f.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace content {

namespace {

int ToGestureEventType(WebInputEvent::Type type) {
  switch (type) {
    case WebInputEvent::Type::kGestureScrollBegin:
      return ui::GESTURE_EVENT_TYPE_SCROLL_START;
    case WebInputEvent::Type::kGestureScrollEnd:
      return ui::GESTURE_EVENT_TYPE_SCROLL_END;
    case WebInputEvent::Type::kGestureScrollUpdate:
      return ui::GESTURE_EVENT_TYPE_SCROLL_BY;
    case WebInputEvent::Type::kGestureFlingStart:
      return ui::GESTURE_EVENT_TYPE_FLING_START;
    case WebInputEvent::Type::kGestureFlingCancel:
      return ui::GESTURE_EVENT_TYPE_FLING_CANCEL;
    case WebInputEvent::Type::kGestureShowPress:
      return ui::GESTURE_EVENT_TYPE_SHOW_PRESS;
    case WebInputEvent::Type::kGestureTap:
      return ui::GESTURE_EVENT_TYPE_SINGLE_TAP_CONFIRMED;
    case WebInputEvent::Type::kGestureTapUnconfirmed:
      return ui::GESTURE_EVENT_TYPE_SINGLE_TAP_UNCONFIRMED;
    case WebInputEvent::Type::kGestureTapDown:
      return ui::GESTURE_EVENT_TYPE_TAP_DOWN;
    case WebInputEvent::Type::kGestureTapCancel:
      return ui::GESTURE_EVENT_TYPE_TAP_CANCEL;
    case WebInputEvent::Type::kGestureDoubleTap:
      return ui::GESTURE_EVENT_TYPE_DOUBLE_TAP;
    case WebInputEvent::Type::kGestureLongPress:
      return ui::GESTURE_EVENT_TYPE_LONG_PRESS;
    case WebInputEvent::Type::kGestureLongTap:
      return ui::GESTURE_EVENT_TYPE_LONG_TAP;
    case WebInputEvent::Type::kGesturePinchBegin:
      return ui::GESTURE_EVENT_TYPE_PINCH_BEGIN;
    case WebInputEvent::Type::kGesturePinchEnd:
      return ui::GESTURE_EVENT_TYPE_PINCH_END;
    case WebInputEvent::Type::kGesturePinchUpdate:
      return ui::GESTURE_EVENT_TYPE_PINCH_BY;
    case WebInputEvent::Type::kGestureTwoFingerTap:
    default:
      NOTREACHED() << "Invalid source gesture type: "
                   << WebInputEvent::GetName(type);
      return -1;
  }
}

}  // namespace

// Reset scroll, hide popups on navigation finish/render process gone event.
class GestureListenerManager::ResetScrollObserver : public WebContentsObserver {
 public:
  ResetScrollObserver(WebContents* web_contents,
                      GestureListenerManager* manager);

  ResetScrollObserver(const ResetScrollObserver&) = delete;
  ResetScrollObserver& operator=(const ResetScrollObserver&) = delete;

  void DidFinishNavigation(NavigationHandle* navigation_handle) override;
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;

 private:
  const raw_ptr<GestureListenerManager> manager_;
};

GestureListenerManager::ResetScrollObserver::ResetScrollObserver(
    WebContents* web_contents,
    GestureListenerManager* manager)
    : WebContentsObserver(web_contents), manager_(manager) {}

void GestureListenerManager::ResetScrollObserver::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  manager_->OnNavigationFinished(navigation_handle);
}

void GestureListenerManager::ResetScrollObserver::
    PrimaryMainFrameRenderProcessGone(base::TerminationStatus status) {
  manager_->OnRenderProcessGone();
}

GestureListenerManager::GestureListenerManager(JNIEnv* env,
                                               const JavaParamRef<jobject>& obj,
                                               WebContentsImpl* web_contents)
    : RenderWidgetHostConnector(web_contents),
      reset_scroll_observer_(new ResetScrollObserver(web_contents, this)),
      web_contents_(web_contents),
      java_ref_(env, obj) {}

GestureListenerManager::~GestureListenerManager() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return;
  Java_GestureListenerManagerImpl_onNativeDestroyed(env, j_obj);
}

void GestureListenerManager::ResetGestureDetection(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  if (rwhva_)
    rwhva_->ResetGestureDetection();
}

void GestureListenerManager::SetDoubleTapSupportEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean enabled) {
  if (rwhva_)
    rwhva_->SetDoubleTapSupportEnabled(enabled);
}

void GestureListenerManager::SetMultiTouchZoomSupportEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean enabled) {
  if (rwhva_)
    rwhva_->SetMultiTouchZoomSupportEnabled(enabled);
}

void GestureListenerManager::SetRootScrollOffsetUpdateFrequency(
    JNIEnv* env,
    jint frequency) {
  auto new_frequency =
      static_cast<cc::mojom::RootScrollOffsetUpdateFrequency>(frequency);
  if (root_scroll_offset_update_frequency_ == new_frequency) {
    return;
  }

  root_scroll_offset_update_frequency_ = new_frequency;
  if (rwhva_)
    rwhva_->UpdateRootScrollOffsetUpdateFrequency();
}

void GestureListenerManager::GestureEventAck(
    const blink::WebGestureEvent& event,
    blink::mojom::InputEventResultState ack_result,
    blink::mojom::ScrollResultDataPtr scroll_result_data) {
  // This is called to fix crash happening while WebContents is being
  // destroyed. See https://crbug.com/803244#c20
  if (web_contents_->IsBeingDestroyed())
    return;
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return;
  if (event.GetType() == blink::WebInputEvent::Type::kGestureScrollBegin) {
    Java_GestureListenerManagerImpl_onScrollBegin(
        env, j_obj, /*isDirectionUp*/ event.data.scroll_begin.delta_y_hint > 0);
    return;
  }
  bool consumed = ack_result == blink::mojom::InputEventResultState::kConsumed;
  if (event.GetType() == blink::WebInputEvent::Type::kGestureFlingStart &&
      consumed) {
    Java_GestureListenerManagerImpl_onFlingStart(
        env, j_obj, /*isDirectionUp*/ event.data.scroll_begin.delta_y_hint > 0);
    return;
  }
  float x = -1.f, y = -1.f;
  if (scroll_result_data && scroll_result_data->root_scroll_offset) {
    x = scroll_result_data->root_scroll_offset->x();
    y = scroll_result_data->root_scroll_offset->y();
  }

  Java_GestureListenerManagerImpl_onEventAck(
      env, j_obj, static_cast<int>(event.GetType()), consumed, x, y);
}

void GestureListenerManager::DidStopFlinging() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return;
  Java_GestureListenerManagerImpl_onFlingEnd(env, j_obj);
}

bool GestureListenerManager::FilterInputEvent(const WebInputEvent& event) {
  if (event.GetType() != WebInputEvent::Type::kGestureTap &&
      event.GetType() != WebInputEvent::Type::kGestureLongTap &&
      event.GetType() != WebInputEvent::Type::kGestureLongPress &&
      event.GetType() != WebInputEvent::Type::kMouseDown)
    return false;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return false;

  web_contents_->GetNativeView()->RequestFocus();

  if (event.GetType() == WebInputEvent::Type::kMouseDown)
    return false;

  const WebGestureEvent& gesture = static_cast<const WebGestureEvent&>(event);
  int gesture_type = ToGestureEventType(event.GetType());
  float dip_scale = web_contents_->GetNativeView()->GetDipScale();
  return Java_GestureListenerManagerImpl_filterTapOrPressEvent(
      env, j_obj, gesture_type, gesture.PositionInWidget().x() * dip_scale,
      gesture.PositionInWidget().y() * dip_scale);
}

void GestureListenerManager::DidOverscroll(
    const ui::DidOverscrollParams& params) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  if (j_obj.is_null())
    return;
  float x = params.accumulated_overscroll.x();
  float y = params.accumulated_overscroll.y();
  return Java_GestureListenerManagerImpl_didOverscroll(env, j_obj, x, y);
}

// All positions and sizes (except |top_shown_pix|) are in CSS pixels.
// Note that viewport_width/height is a best effort based.
void GestureListenerManager::UpdateScrollInfo(const gfx::PointF& scroll_offset,
                                              float page_scale_factor,
                                              const float min_page_scale,
                                              const float max_page_scale,
                                              const gfx::SizeF& content,
                                              const gfx::SizeF& viewport,
                                              const float content_offset,
                                              const float top_shown_pix,
                                              bool top_changed) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  web_contents_->GetNativeView()->UpdateFrameInfo({viewport, content_offset});
  Java_GestureListenerManagerImpl_updateScrollInfo(
      env, obj, scroll_offset.x(), scroll_offset.y(), page_scale_factor,
      min_page_scale, max_page_scale, content.width(), content.height(),
      viewport.width(), viewport.height(), top_shown_pix, top_changed);
}

void GestureListenerManager::UpdateOnTouchDown() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_GestureListenerManagerImpl_updateOnTouchDown(env, obj);
}

void GestureListenerManager::OnRootScrollOffsetChanged(
    const gfx::PointF& root_scroll_offset) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  Java_GestureListenerManagerImpl_onRootScrollOffsetChanged(
      env, obj, root_scroll_offset.x(), root_scroll_offset.y());
}

void GestureListenerManager::UpdateRenderProcessConnection(
    RenderWidgetHostViewAndroid* old_rwhva,
    RenderWidgetHostViewAndroid* new_rwhva) {
  if (old_rwhva)
    old_rwhva->SetGestureListenerManager(nullptr);
  if (new_rwhva)
    new_rwhva->SetGestureListenerManager(this);
  rwhva_ = new_rwhva;
}

void GestureListenerManager::OnNavigationFinished(
    NavigationHandle* navigation_handle) {
  if (navigation_handle->IsInPrimaryMainFrame() &&
      navigation_handle->HasCommitted() &&
      !navigation_handle->IsSameDocument()) {
    ResetPopupsAndInput(false);
  }
}

void GestureListenerManager::OnRenderProcessGone() {
  ResetPopupsAndInput(true);
}

bool GestureListenerManager::IsScrollInProgressForTesting() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return false;

  return Java_GestureListenerManagerImpl_isScrollInProgress(env, obj);
}

void GestureListenerManager::ResetPopupsAndInput(bool render_process_gone) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_GestureListenerManagerImpl_resetPopupsAndInput(env, obj,
                                                      render_process_gone);
}

jlong JNI_GestureListenerManagerImpl_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jweb_contents) {
  auto* web_contents = WebContents::FromJavaWebContents(jweb_contents);
  CHECK(web_contents) << "Should be created with a valid WebContents.";

  // Owns itself and gets destroyed when |WebContentsDestroyed| is called.
  auto* manager = new GestureListenerManager(
      env, obj, static_cast<WebContentsImpl*>(web_contents));
  manager->Initialize();
  return reinterpret_cast<intptr_t>(manager);
}

}  // namespace content
