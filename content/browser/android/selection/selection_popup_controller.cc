// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/selection/selection_popup_controller.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "content/browser/android/selection/composited_touch_handle_drawable.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view_android.h"
#include "content/public/android/content_jni_headers/SelectionPopupControllerImpl_jni.h"
#include "content/public/browser/context_menu_params.h"
#include "third_party/blink/public/common/context_menu_data/edit_flags.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom-blink.h"
#include "ui/gfx/geometry/point_conversions.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace content {

jlong JNI_SelectionPopupControllerImpl_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jweb_contents) {
  WebContents* web_contents = WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);

  // Owns itself and gets destroyed when |WebContentsDestroyed| is called.
  auto* controller = new SelectionPopupController(env, obj, web_contents);
  controller->Initialize();
  return reinterpret_cast<intptr_t>(controller);
}

SelectionPopupController::SelectionPopupController(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    WebContents* web_contents)
    : RenderWidgetHostConnector(web_contents) {
  java_obj_ = JavaObjectWeakGlobalRef(env, obj);
  auto* wcva = static_cast<WebContentsViewAndroid*>(
      static_cast<WebContentsImpl*>(web_contents)->GetView());
  wcva->set_selection_popup_controller(this);
}

SelectionPopupController::~SelectionPopupController() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_obj_.get(env);
  if (!obj.is_null()) {
    Java_SelectionPopupControllerImpl_nativeSelectionPopupControllerDestroyed(
        env, obj);
  }
}

ScopedJavaLocalRef<jobject> SelectionPopupController::GetContext() const {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> obj = java_obj_.get(env);
  if (obj.is_null())
    return nullptr;

  return Java_SelectionPopupControllerImpl_getContext(env, obj);
}

void SelectionPopupController::SetTextHandlesTemporarilyHidden(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean hidden) {
  if (rwhva_)
    rwhva_->SetTextHandlesTemporarilyHidden(hidden);
}

std::unique_ptr<ui::TouchHandleDrawable>
SelectionPopupController::CreateTouchHandleDrawable() {
  ScopedJavaLocalRef<jobject> activityContext = GetContext();
  // If activityContext is null then Application context is used instead on
  // the java side in CompositedTouchHandleDrawable.
  auto* view = web_contents()->GetNativeView();
  return std::unique_ptr<ui::TouchHandleDrawable>(
      new CompositedTouchHandleDrawable(view, activityContext));
}

void SelectionPopupController::MoveRangeSelectionExtent(
    const gfx::PointF& extent) {
  auto* web_contents_impl = static_cast<WebContentsImpl*>(web_contents());
  if (!web_contents_impl)
    return;

  web_contents_impl->MoveRangeSelectionExtent(gfx::ToRoundedPoint(extent));
}

void SelectionPopupController::SelectBetweenCoordinates(
    const gfx::PointF& base,
    const gfx::PointF& extent) {
  auto* web_contents_impl = static_cast<WebContentsImpl*>(web_contents());
  if (!web_contents_impl)
    return;

  gfx::Point base_point = gfx::ToRoundedPoint(base);
  gfx::Point extent_point = gfx::ToRoundedPoint(extent);
  if (base_point == extent_point)
    return;

  web_contents_impl->SelectRange(base_point, extent_point);
}

void SelectionPopupController::UpdateRenderProcessConnection(
    RenderWidgetHostViewAndroid* old_rwhva,
    RenderWidgetHostViewAndroid* new_rwhva) {
  if (old_rwhva)
    old_rwhva->set_selection_popup_controller(nullptr);
  if (new_rwhva)
    new_rwhva->set_selection_popup_controller(this);
  rwhva_ = new_rwhva;
}

void SelectionPopupController::OnSelectionEvent(
    ui::SelectionEventType event,
    const gfx::RectF& selection_rect) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_obj_.get(env);
  if (obj.is_null())
    return;

  Java_SelectionPopupControllerImpl_onSelectionEvent(
      env, obj, event, selection_rect.x(), selection_rect.y(),
      selection_rect.right(), selection_rect.bottom());
}

void SelectionPopupController::OnDragUpdate(
    const ui::TouchSelectionDraggable::Type type,
    const gfx::PointF& position) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_obj_.get(env);
  if (obj.is_null())
    return;

  Java_SelectionPopupControllerImpl_onDragUpdate(
      env, obj, static_cast<int>(type), position.x(), position.y());
}

void SelectionPopupController::OnSelectionChanged(const std::string& text) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_obj_.get(env);
  if (obj.is_null())
    return;
  ScopedJavaLocalRef<jstring> jtext = ConvertUTF8ToJavaString(env, text);
  Java_SelectionPopupControllerImpl_onSelectionChanged(env, obj, jtext);
}

bool SelectionPopupController::ShowSelectionMenu(
    RenderFrameHost* render_frame_host,
    const ContextMenuParams& params,
    int handle_height) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_obj_.get(env);
  if (obj.is_null())
    return false;

  // Display paste pop-up only when selection is empty and editable.
  const bool from_touch = params.source_type == ui::MENU_SOURCE_TOUCH ||
                          params.source_type == ui::MENU_SOURCE_LONG_PRESS ||
                          params.source_type == ui::MENU_SOURCE_TOUCH_HANDLE ||
                          params.source_type == ui::MENU_SOURCE_STYLUS;

  const bool from_mouse = params.source_type == ui::MENU_SOURCE_MOUSE;

  const bool from_selection_adjustment =
      params.source_type == ui::MENU_SOURCE_ADJUST_SELECTION ||
      params.source_type == ui::MENU_SOURCE_ADJUST_SELECTION_RESET;

  // If source_type is not in the list then return.
  if (!from_touch && !from_mouse && !from_selection_adjustment)
    return false;

  // Don't show paste pop-up for non-editable textarea.
  if (!params.is_editable && params.selection_text.empty())
    return false;

  const bool can_select_all =
      !!(params.edit_flags & blink::ContextMenuDataEditFlags::kCanSelectAll);
  const bool can_edit_richly =
      !!(params.edit_flags & blink::ContextMenuDataEditFlags::kCanEditRichly);
  const bool is_password_type =
      params.input_field_type ==
      blink::mojom::ContextMenuDataInputFieldType::kPassword;
  const ScopedJavaLocalRef<jstring> jselected_text =
      ConvertUTF16ToJavaString(env, params.selection_text);
  const bool should_suggest = params.source_type == ui::MENU_SOURCE_TOUCH ||
                              params.source_type == ui::MENU_SOURCE_LONG_PRESS;

  Java_SelectionPopupControllerImpl_showSelectionMenu(
      env, obj, params.selection_rect.x(), params.selection_rect.y(),
      params.selection_rect.right(), params.selection_rect.bottom(),
      handle_height, params.is_editable, is_password_type, jselected_text,
      params.selection_start_offset, can_select_all, can_edit_richly,
      should_suggest, params.source_type,
      render_frame_host->GetJavaRenderFrameHost());
  return true;
}

void SelectionPopupController::OnSelectAroundCaretAck(
    blink::mojom::SelectAroundCaretResultPtr result) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_obj_.get(env);
  if (obj.is_null())
    return;
  if (result.is_null()) {
    Java_SelectionPopupControllerImpl_onSelectAroundCaretFailure(env, obj);
  } else {
    Java_SelectionPopupControllerImpl_onSelectAroundCaretSuccess(
        env, obj, result->extended_start_adjust, result->extended_end_adjust,
        result->word_start_adjust, result->word_end_adjust);
  }
}

void SelectionPopupController::HidePopupsAndPreserveSelection() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_obj_.get(env);
  if (obj.is_null())
    return;

  Java_SelectionPopupControllerImpl_hidePopupsAndPreserveSelection(env, obj);
}

void SelectionPopupController::RestoreSelectionPopupsIfNecessary() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_obj_.get(env);
  if (obj.is_null())
    return;

  Java_SelectionPopupControllerImpl_restoreSelectionPopupsIfNecessary(env, obj);
}

}  // namespace content
