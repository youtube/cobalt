// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/accessibility/read_anything_app_controller.h"

#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/renderer/accessibility/ax_tree_distiller.h"
#include "content/public/renderer/chrome_object_extensions_utils.h"
#include "content/public/renderer/render_frame.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_selection.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_serializer.h"
#include "ui/accessibility/ax_tree_update.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-microtask-queue.h"

using read_anything::mojom::ReadAnythingTheme;
using read_anything::mojom::ReadAnythingThemePtr;

namespace {

// The following methods convert v8::Value types to an AXTreeUpdate. This is not
// a complete conversion (thus way gin::Converter<ui::AXTreeUpdate> is not used
// or implemented) but just converting the bare minimum data types needed for
// the ReadAnythingAppTest.

void SetAXNodeDataChildIds(v8::Isolate* isolate,
                           gin::Dictionary* v8_dict,
                           ui::AXNodeData* ax_node_data) {
  v8::Local<v8::Value> v8_child_ids;
  v8_dict->Get("childIds", &v8_child_ids);
  gin::ConvertFromV8(isolate, v8_child_ids, &ax_node_data->child_ids);
}

void SetAXNodeDataId(v8::Isolate* isolate,
                     gin::Dictionary* v8_dict,
                     ui::AXNodeData* ax_node_data) {
  v8::Local<v8::Value> v8_id;
  v8_dict->Get("id", &v8_id);
  gin::ConvertFromV8(isolate, v8_id, &ax_node_data->id);
}

void SetAXNodeDataLanguage(v8::Isolate* isolate,
                           gin::Dictionary* v8_dict,
                           ui::AXNodeData* ax_node_data) {
  v8::Local<v8::Value> v8_language;
  v8_dict->Get("language", &v8_language);
  std::string language;
  gin::ConvertFromV8(isolate, v8_language, &language);
  ax_node_data->AddStringAttribute(ax::mojom::StringAttribute::kLanguage,
                                   language);
}

void SetAXNodeDataName(v8::Isolate* isolate,
                       gin::Dictionary* v8_dict,
                       ui::AXNodeData* ax_node_data) {
  v8::Local<v8::Value> v8_name;
  v8_dict->Get("name", &v8_name);
  std::string name;
  gin::ConvertFromV8(isolate, v8_name, &name);
  ax_node_data->SetName(name);
  ax_node_data->SetNameFrom(ax::mojom::NameFrom::kContents);
}

void SetAXNodeDataRole(v8::Isolate* isolate,
                       gin::Dictionary* v8_dict,
                       ui::AXNodeData* ax_node_data) {
  v8::Local<v8::Value> v8_role;
  v8_dict->Get("role", &v8_role);
  std::string role_name;
  gin::ConvertFromV8(isolate, v8_role, &role_name);
  if (role_name == "rootWebArea") {
    ax_node_data->role = ax::mojom::Role::kRootWebArea;
  } else if (role_name == "heading") {
    ax_node_data->role = ax::mojom::Role::kHeading;
  } else if (role_name == "link") {
    ax_node_data->role = ax::mojom::Role::kLink;
  } else if (role_name == "paragraph") {
    ax_node_data->role = ax::mojom::Role::kParagraph;
  } else if (role_name == "staticText") {
    ax_node_data->role = ax::mojom::Role::kStaticText;
  } else if (role_name == "button") {
    ax_node_data->role = ax::mojom::Role::kButton;
  }
}

void SetAXNodeDataHtmlTag(v8::Isolate* isolate,
                          gin::Dictionary* v8_dict,
                          ui::AXNodeData* ax_node_data) {
  v8::Local<v8::Value> v8_html_tag;
  v8_dict->Get("htmlTag", &v8_html_tag);
  std::string html_tag;
  gin::Converter<std::string>::FromV8(isolate, v8_html_tag, &html_tag);
  ax_node_data->AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag,
                                   html_tag);
}

void SetAXNodeDataTextDirection(v8::Isolate* isolate,
                                gin::Dictionary* v8_dict,
                                ui::AXNodeData* ax_node_data) {
  v8::Local<v8::Value> v8_direction;
  v8_dict->Get("direction", &v8_direction);
  int direction;
  gin::ConvertFromV8(isolate, v8_direction, &direction);
  ax_node_data->AddIntAttribute(ax::mojom::IntAttribute::kTextDirection,
                                direction);
}

void SetAXNodeDataTextStyle(v8::Isolate* isolate,
                            gin::Dictionary* v8_dict,
                            ui::AXNodeData* ax_node_data) {
  v8::Local<v8::Value> v8_text_style;
  v8_dict->Get("textStyle", &v8_text_style);
  std::string text_style;
  gin::ConvertFromV8(isolate, v8_text_style, &text_style);
  if (text_style.find("underline") != std::string::npos) {
    ax_node_data->AddTextStyle(ax::mojom::TextStyle::kUnderline);
  }
  if (text_style.find("overline") != std::string::npos) {
    ax_node_data->AddTextStyle(ax::mojom::TextStyle::kOverline);
  }
  if (text_style.find("italic") != std::string::npos) {
    ax_node_data->AddTextStyle(ax::mojom::TextStyle::kItalic);
  }
  if (text_style.find("bold") != std::string::npos) {
    ax_node_data->AddTextStyle(ax::mojom::TextStyle::kBold);
  }
}

void SetAXNodeDataUrl(v8::Isolate* isolate,
                      gin::Dictionary* v8_dict,
                      ui::AXNodeData* ax_node_data) {
  v8::Local<v8::Value> v8_url;
  v8_dict->Get("url", &v8_url);
  std::string url;
  gin::ConvertFromV8(isolate, v8_url, &url);
  ax_node_data->AddStringAttribute(ax::mojom::StringAttribute::kUrl, url);
}

void SetSelectionAnchorObjectId(v8::Isolate* isolate,
                                gin::Dictionary* v8_dict,
                                ui::AXTreeData* ax_tree_data) {
  v8::Local<v8::Value> v8_anchor_object_id;
  v8_dict->Get("anchor_object_id", &v8_anchor_object_id);
  gin::ConvertFromV8(isolate, v8_anchor_object_id,
                     &ax_tree_data->sel_anchor_object_id);
}

void SetSelectionFocusObjectId(v8::Isolate* isolate,
                               gin::Dictionary* v8_dict,
                               ui::AXTreeData* ax_tree_data) {
  v8::Local<v8::Value> v8_focus_object_id;
  v8_dict->Get("focus_object_id", &v8_focus_object_id);
  gin::ConvertFromV8(isolate, v8_focus_object_id,
                     &ax_tree_data->sel_focus_object_id);
}

void SetSelectionAnchorOffset(v8::Isolate* isolate,
                              gin::Dictionary* v8_dict,
                              ui::AXTreeData* ax_tree_data) {
  v8::Local<v8::Value> v8_anchor_offset;
  v8_dict->Get("anchor_offset", &v8_anchor_offset);
  gin::ConvertFromV8(isolate, v8_anchor_offset,
                     &ax_tree_data->sel_anchor_offset);
}

void SetSelectionFocusOffset(v8::Isolate* isolate,
                             gin::Dictionary* v8_dict,
                             ui::AXTreeData* ax_tree_data) {
  v8::Local<v8::Value> v8_focus_offset;
  v8_dict->Get("focus_offset", &v8_focus_offset);
  gin::ConvertFromV8(isolate, v8_focus_offset, &ax_tree_data->sel_focus_offset);
}

void SetSelectionIsBackward(v8::Isolate* isolate,
                            gin::Dictionary* v8_dict,
                            ui::AXTreeData* ax_tree_data) {
  v8::Local<v8::Value> v8_sel_is_backward;
  v8_dict->Get("is_backward", &v8_sel_is_backward);
  gin::ConvertFromV8(isolate, v8_sel_is_backward,
                     &ax_tree_data->sel_is_backward);
}

void SetAXTreeUpdateRootId(v8::Isolate* isolate,
                           gin::Dictionary* v8_dict,
                           ui::AXTreeUpdate* snapshot) {
  v8::Local<v8::Value> v8_root_id;
  v8_dict->Get("rootId", &v8_root_id);
  gin::ConvertFromV8(isolate, v8_root_id, &snapshot->root_id);
}

ui::AXTreeUpdate GetSnapshotFromV8SnapshotLite(
    v8::Isolate* isolate,
    v8::Local<v8::Value> v8_snapshot_lite) {
  ui::AXTreeUpdate snapshot;
  ui::AXTreeData ax_tree_data;
  ax_tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
  snapshot.has_tree_data = true;
  snapshot.tree_data = ax_tree_data;
  gin::Dictionary v8_snapshot_dict(isolate);
  if (!gin::ConvertFromV8(isolate, v8_snapshot_lite, &v8_snapshot_dict)) {
    return snapshot;
  }
  SetAXTreeUpdateRootId(isolate, &v8_snapshot_dict, &snapshot);

  v8::Local<v8::Value> v8_nodes;
  v8_snapshot_dict.Get("nodes", &v8_nodes);
  std::vector<v8::Local<v8::Value>> v8_nodes_vector;
  if (!gin::ConvertFromV8(isolate, v8_nodes, &v8_nodes_vector)) {
    return snapshot;
  }
  for (v8::Local<v8::Value> v8_node : v8_nodes_vector) {
    gin::Dictionary v8_node_dict(isolate);
    if (!gin::ConvertFromV8(isolate, v8_node, &v8_node_dict)) {
      continue;
    }
    ui::AXNodeData ax_node_data;
    SetAXNodeDataId(isolate, &v8_node_dict, &ax_node_data);
    SetAXNodeDataRole(isolate, &v8_node_dict, &ax_node_data);
    SetAXNodeDataName(isolate, &v8_node_dict, &ax_node_data);
    SetAXNodeDataChildIds(isolate, &v8_node_dict, &ax_node_data);
    SetAXNodeDataHtmlTag(isolate, &v8_node_dict, &ax_node_data);
    SetAXNodeDataLanguage(isolate, &v8_node_dict, &ax_node_data);
    SetAXNodeDataTextDirection(isolate, &v8_node_dict, &ax_node_data);
    SetAXNodeDataTextStyle(isolate, &v8_node_dict, &ax_node_data);
    SetAXNodeDataUrl(isolate, &v8_node_dict, &ax_node_data);
    snapshot.nodes.push_back(ax_node_data);
  }

  v8::Local<v8::Value> v8_selection;
  v8_snapshot_dict.Get("selection", &v8_selection);
  gin::Dictionary v8_selection_dict(isolate);
  if (!gin::ConvertFromV8(isolate, v8_selection, &v8_selection_dict)) {
    return snapshot;
  }
  SetSelectionAnchorObjectId(isolate, &v8_selection_dict, &snapshot.tree_data);
  SetSelectionFocusObjectId(isolate, &v8_selection_dict, &snapshot.tree_data);
  SetSelectionAnchorOffset(isolate, &v8_selection_dict, &snapshot.tree_data);
  SetSelectionFocusOffset(isolate, &v8_selection_dict, &snapshot.tree_data);
  SetSelectionIsBackward(isolate, &v8_selection_dict, &snapshot.tree_data);
  return snapshot;
}

}  // namespace

// static
gin::WrapperInfo ReadAnythingAppController::kWrapperInfo = {
    gin::kEmbedderNativeGin};

// static
ReadAnythingAppController* ReadAnythingAppController::Install(
    content::RenderFrame* render_frame) {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      render_frame->GetWebFrame()->MainWorldScriptContext();
  if (context.IsEmpty()) {
    return nullptr;
  }
  v8::MicrotasksScope microtask_scope(isolate, context->GetMicrotaskQueue(),
                                      v8::MicrotasksScope::kDoNotRunMicrotasks);

  v8::Context::Scope context_scope(context);

  ReadAnythingAppController* controller =
      new ReadAnythingAppController(render_frame);
  gin::Handle<ReadAnythingAppController> handle =
      gin::CreateHandle(isolate, controller);
  if (handle.IsEmpty()) {
    return nullptr;
  }

  v8::Local<v8::Object> chrome =
      content::GetOrCreateChromeObject(isolate, context);
  chrome->Set(context, gin::StringToV8(isolate, "readAnything"), handle.ToV8())
      .Check();
  return controller;
}

ReadAnythingAppController::ReadAnythingAppController(
    content::RenderFrame* render_frame)
    : render_frame_id_(render_frame->GetRoutingID()) {
  distiller_ = std::make_unique<AXTreeDistiller>(
      render_frame,
      base::BindRepeating(&ReadAnythingAppController::OnAXTreeDistilled,
                          weak_ptr_factory_.GetWeakPtr()));
}

ReadAnythingAppController::~ReadAnythingAppController() = default;

void ReadAnythingAppController::AccessibilityEventReceived(
    const ui::AXTreeID& tree_id,
    const std::vector<ui::AXTreeUpdate>& updates,
    const std::vector<ui::AXEvent>& events) {
  model_.AccessibilityEventReceived(tree_id, updates, this);

  if (model_.loading() || tree_id != model_.active_tree_id()) {
    return;
  }

  for (auto& event : events) {
    if (event.event_type == ax::mojom::Event::kDocumentSelectionChanged &&
        event.event_from == ax::mojom::EventFrom::kUser) {
      PostProcessSelection();
    }
  }
}

void ReadAnythingAppController::ExecuteJavaScript(std::string script) {
  content::RenderFrame* render_frame =
      content::RenderFrame::FromRoutingID(render_frame_id_);
  if (!render_frame) {
    return;
  }
  // TODO(b/1266555): Use v8::Function rather than javascript. If possible,
  // replace this function call with firing an event.
  render_frame->ExecuteJavaScript(base::ASCIIToUTF16(script));
}

void ReadAnythingAppController::OnActiveAXTreeIDChanged(
    const ui::AXTreeID& tree_id,
    ukm::SourceId ukm_source_id) {
  if (tree_id == model_.active_tree_id()) {
    return;
  }
  ui::AXTreeID previous_active_tree_id = model_.active_tree_id();
  model_.SetActiveTreeId(tree_id);
  model_.SetActiveUkmSourceId(ukm_source_id);
  // Delete all pending updates on the formerly active AXTree.
  // TODO(crbug.com/1266555): If distillation is in progress, cancel the
  // distillation request.
  model_.ClearPendingUpdates();

  // TODO(b/1266555): Use v8::Function rather than javascript. If possible,
  // replace this function call with firing an event.
  ExecuteJavaScript("chrome.readAnything.showLoading();");
  model_.SetLoading(true);

  // When the UI first constructs, this function may be called before tree_id
  // has been added to the tree list in AccessibilityEventReceived. In that
  // case, do not distill.
  if (model_.active_tree_id() != ui::AXTreeIDUnknown() &&
      model_.ContainsTree(model_.active_tree_id())) {
    Distill();
  }
}

void ReadAnythingAppController::OnAXTreeDestroyed(const ui::AXTreeID& tree_id) {
  model_.OnAXTreeDestroyed(tree_id);
}

void ReadAnythingAppController::OnAtomicUpdateFinished(
    ui::AXTree* tree,
    bool root_changed,
    const std::vector<Change>& changes) {
  // TODO(crbug.com/1266555): This method may be called when child trees finish
  // updating. We should re-distill if tree is a child of the active tree.
  if (model_.active_tree_id() == ui::AXTreeIDUnknown() ||
      tree->GetAXTreeID() != model_.active_tree_id()) {
    return;
  }
  bool need_to_draw = false;
  for (Change change : changes) {
    switch (change.type) {
      case NODE_CREATED:
      case SUBTREE_CREATED:
        Distill();
        return;
      case NODE_REPARENTED:
      case SUBTREE_REPARENTED:
        if (base::Contains(model_.content_node_ids(), change.node->id())) {
          Distill();
          return;
        } else if (base::Contains(model_.display_node_ids(),
                                  change.node->id())) {
          need_to_draw = true;
        }
        break;
      case NODE_CHANGED:
        break;
    }
  }
  if (need_to_draw) {
    Draw();
  }
}

void ReadAnythingAppController::Distill() {
  ui::AXSerializableTree* tree =
      model_.GetTreeFromId(model_.active_tree_id()).get();
  std::unique_ptr<ui::AXTreeSource<const ui::AXNode*>> tree_source(
      tree->CreateTreeSource());
  ui::AXTreeSerializer<const ui::AXNode*> serializer(tree_source.get());
  ui::AXTreeUpdate snapshot;
  CHECK(serializer.SerializeChanges(tree->root(), &snapshot));
  model_.SetDistillationInProgress(true);
  distiller_->Distill(*tree, snapshot, model_.active_ukm_source_id());
}

void ReadAnythingAppController::OnAXTreeDistilled(
    const ui::AXTreeID& tree_id,
    const std::vector<ui::AXNodeID>& content_node_ids) {
  // Reset state.
  model_.Reset(content_node_ids);

  // Return early if any of the following scenarios occurred while waiting for
  // distillation to complete:
  // 1. tree_id != model_.active_tree_id(): The active tree was changed.
  // 2. model_.active_tree_id() == ui::AXTreeIDUnknown(): The active tree was
  // change to
  //    an unknown tree id.
  // 3. !model_.ContainsTree(tree_id): The distilled tree was destroyed.
  // 4. tree_id == ui::AXTreeIDUnknown(): The distiller sent back an unknown
  //    tree id which occurs when there was an error.
  if (tree_id != model_.active_tree_id() ||
      model_.active_tree_id() == ui::AXTreeIDUnknown() ||
      !model_.ContainsTree(tree_id) || tree_id == ui::AXTreeIDUnknown()) {
    return;
  }
  if (!model_.content_node_ids().empty()) {
    // If there are content_node_ids, this means the AXTree was successfully
    // distilled.
    model_.ComputeDisplayNodeIdsForDistilledTree();
  }

  // Draw selection (if one exists) and the content.
  PostProcessSelection();

  // TODO(crbug.com/1266555): If no content nodes were identified, the
  // controller should handle drawing the empty state (like how it handles the
  // loading state) instead of the JS.

  // Once drawing is complete, unserialize all of the pending updates on the
  // active tree and send out a new distillation request.
  model_.UnserializePendingUpdates(tree_id);
}

void ReadAnythingAppController::PostProcessSelection() {
  if (model_.PostProcessSelection()) {
    Draw();
  }
  DrawSelection();
}

void ReadAnythingAppController::Draw() {
  // TODO(abigailbklein): Use v8::Function rather than javascript. If possible,
  // replace this function call with firing an event.
  ExecuteJavaScript("chrome.readAnything.updateContent();");
  model_.SetLoading(false);
}

void ReadAnythingAppController::DrawSelection() {
  // TODO(abigailbklein): Use v8::Function rather than javascript. If possible,
  // replace this function call with firing an event.
  ExecuteJavaScript("chrome.readAnything.updateSelection();");
}

void ReadAnythingAppController::OnThemeChanged(ReadAnythingThemePtr new_theme) {
  model_.OnThemeChanged(std::move(new_theme));

  // TODO(abigailbklein): Use v8::Function rather than javascript. If possible,
  // replace this function call with firing an event.
  ExecuteJavaScript("chrome.readAnything.updateTheme();");
}

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
void ReadAnythingAppController::ScreenAIServiceReady() {
  distiller_->ScreenAIServiceReady();
}
#endif

gin::ObjectTemplateBuilder ReadAnythingAppController::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<ReadAnythingAppController>::GetObjectTemplateBuilder(
             isolate)
      .SetProperty("rootId", &ReadAnythingAppController::RootId)
      .SetProperty("startNodeId", &ReadAnythingAppController::StartNodeId)
      .SetProperty("startOffset", &ReadAnythingAppController::StartOffset)
      .SetProperty("endNodeId", &ReadAnythingAppController::EndNodeId)
      .SetProperty("endOffset", &ReadAnythingAppController::EndOffset)
      .SetProperty("backgroundColor",
                   &ReadAnythingAppController::BackgroundColor)
      .SetProperty("fontName", &ReadAnythingAppController::FontName)
      .SetProperty("fontSize", &ReadAnythingAppController::FontSize)
      .SetProperty("foregroundColor",
                   &ReadAnythingAppController::ForegroundColor)
      .SetProperty("letterSpacing", &ReadAnythingAppController::LetterSpacing)
      .SetProperty("lineSpacing", &ReadAnythingAppController::LineSpacing)
      .SetMethod("getChildren", &ReadAnythingAppController::GetChildren)
      .SetMethod("getTextDirection",
                 &ReadAnythingAppController::GetTextDirection)
      .SetMethod("getHtmlTag", &ReadAnythingAppController::GetHtmlTag)
      .SetMethod("getLanguage", &ReadAnythingAppController::GetLanguage)
      .SetMethod("getTextContent", &ReadAnythingAppController::GetTextContent)
      .SetMethod("getUrl", &ReadAnythingAppController::GetUrl)
      .SetMethod("shouldBold", &ReadAnythingAppController::ShouldBold)
      .SetMethod("isOverline", &ReadAnythingAppController::IsOverline)
      .SetMethod("onConnected", &ReadAnythingAppController::OnConnected)
      .SetMethod("onLinkClicked", &ReadAnythingAppController::OnLinkClicked)
      .SetMethod("onSelectionChange",
                 &ReadAnythingAppController::OnSelectionChange)
      .SetMethod("setContentForTesting",
                 &ReadAnythingAppController::SetContentForTesting)
      .SetMethod("setThemeForTesting",
                 &ReadAnythingAppController::SetThemeForTesting);
}

ui::AXNodeID ReadAnythingAppController::RootId() const {
  ui::AXSerializableTree* tree =
      model_.GetTreeFromId(model_.active_tree_id()).get();
  return tree->root()->id();
}

ui::AXNodeID ReadAnythingAppController::StartNodeId() const {
  return model_.start_node_id();
}

int ReadAnythingAppController::StartOffset() const {
  return model_.start_offset();
}

ui::AXNodeID ReadAnythingAppController::EndNodeId() const {
  return model_.end_node_id();
}

int ReadAnythingAppController::EndOffset() const {
  return model_.end_offset();
}

SkColor ReadAnythingAppController::BackgroundColor() const {
  return model_.background_color();
}

std::string ReadAnythingAppController::FontName() const {
  return model_.font_name();
}

float ReadAnythingAppController::FontSize() const {
  return model_.font_size();
}

SkColor ReadAnythingAppController::ForegroundColor() const {
  return model_.foreground_color();
}

float ReadAnythingAppController::LetterSpacing() const {
  return model_.letter_spacing();
}

float ReadAnythingAppController::LineSpacing() const {
  return model_.line_spacing();
}

std::vector<ui::AXNodeID> ReadAnythingAppController::GetChildren(
    ui::AXNodeID ax_node_id) const {
  std::vector<ui::AXNodeID> child_ids;
  ui::AXNode* ax_node = model_.GetAXNode(ax_node_id);
  DCHECK(ax_node);
  const std::set<ui::AXNodeID>* node_ids = model_.selection_node_ids().empty()
                                               ? &model_.display_node_ids()
                                               : &model_.selection_node_ids();
  for (auto it = ax_node->UnignoredChildrenBegin();
       it != ax_node->UnignoredChildrenEnd(); ++it) {
    if (base::Contains(*node_ids, it->id())) {
      child_ids.push_back(it->id());
    }
  }
  return child_ids;
}

std::string ReadAnythingAppController::GetHtmlTag(
    ui::AXNodeID ax_node_id) const {
  ui::AXNode* ax_node = model_.GetAXNode(ax_node_id);
  DCHECK(ax_node);

  if (ui::IsTextField(ax_node->GetRole())) {
    return "div";
  }

  // Replace mark element with bold element for readability
  std::string html_tag =
      ax_node->GetStringAttribute(ax::mojom::StringAttribute::kHtmlTag);
  return html_tag == ui::ToString(ax::mojom::Role::kMark) ? "b" : html_tag;
}

std::string ReadAnythingAppController::GetLanguage(
    ui::AXNodeID ax_node_id) const {
  ui::AXNode* ax_node = model_.GetAXNode(ax_node_id);
  DCHECK(ax_node);
  if (model_.NodeIsContentNode(ax_node_id)) {
    return ax_node->GetLanguage();
  }
  return ax_node->GetStringAttribute(ax::mojom::StringAttribute::kLanguage);
}

std::string ReadAnythingAppController::GetTextContent(
    ui::AXNodeID ax_node_id) const {
  ui::AXNode* ax_node = model_.GetAXNode(ax_node_id);
  DCHECK(ax_node);
  return ax_node->GetTextContentUTF8();
}

std::string ReadAnythingAppController::GetTextDirection(
    ui::AXNodeID ax_node_id) const {
  ui::AXNode* ax_node = model_.GetAXNode(ax_node_id);
  if (!ax_node) {
    return std::string();
  }

  auto text_direction = static_cast<ax::mojom::WritingDirection>(
      ax_node->GetIntAttribute(ax::mojom::IntAttribute::kTextDirection));

  // Vertical writing is displayed horizontally with "auto".
  switch (text_direction) {
    case ax::mojom::WritingDirection::kLtr:
      return "ltr";
    case ax::mojom::WritingDirection::kRtl:
      return "rtl";
    case ax::mojom::WritingDirection::kTtb:
      return "auto";
    case ax::mojom::WritingDirection::kBtt:
      return "auto";
    default:
      return std::string();
  }
}

std::string ReadAnythingAppController::GetUrl(ui::AXNodeID ax_node_id) const {
  ui::AXNode* ax_node = model_.GetAXNode(ax_node_id);
  DCHECK(ax_node);
  return ax_node->GetStringAttribute(ax::mojom::StringAttribute::kUrl);
}

bool ReadAnythingAppController::ShouldBold(ui::AXNodeID ax_node_id) const {
  ui::AXNode* ax_node = model_.GetAXNode(ax_node_id);
  DCHECK(ax_node);
  bool isBold = ax_node->HasTextStyle(ax::mojom::TextStyle::kBold);
  bool isItalic = ax_node->HasTextStyle(ax::mojom::TextStyle::kItalic);
  bool isUnderline = ax_node->HasTextStyle(ax::mojom::TextStyle::kUnderline);
  return isBold || isItalic || isUnderline;
}

bool ReadAnythingAppController::IsOverline(ui::AXNodeID ax_node_id) const {
  ui::AXNode* ax_node = model_.GetAXNode(ax_node_id);
  DCHECK(ax_node);
  return ax_node->HasTextStyle(ax::mojom::TextStyle::kOverline);
}

void ReadAnythingAppController::OnConnected() {
  mojo::PendingReceiver<read_anything::mojom::PageHandlerFactory>
      page_handler_factory_receiver =
          page_handler_factory_.BindNewPipeAndPassReceiver();
  page_handler_factory_->CreatePageHandler(
      receiver_.BindNewPipeAndPassRemote(),
      page_handler_.BindNewPipeAndPassReceiver());
  content::RenderFrame* render_frame =
      content::RenderFrame::FromRoutingID(render_frame_id_);
  if (!render_frame) {
    return;
  }
  render_frame->GetBrowserInterfaceBroker()->GetInterface(
      std::move(page_handler_factory_receiver));
}

void ReadAnythingAppController::OnLinkClicked(ui::AXNodeID ax_node_id) const {
  DCHECK_NE(model_.active_tree_id(), ui::AXTreeIDUnknown());
  // Prevent link clicks while distillation is in progress, as it means that the
  // tree may have changed in an unexpected way.
  // TODO(crbug.com/1266555): Consider how to show this in a more user-friendly
  // way.
  if (model_.distillation_in_progress()) {
    return;
  }
  page_handler_->OnLinkClicked(model_.active_tree_id(), ax_node_id);
}

void ReadAnythingAppController::OnSelectionChange(ui::AXNodeID anchor_node_id,
                                                  int anchor_offset,
                                                  ui::AXNodeID focus_node_id,
                                                  int focus_offset) const {
  DCHECK_NE(model_.active_tree_id(), ui::AXTreeIDUnknown());
  // Prevent link clicks while distillation is in progress, as it means that the
  // tree may have changed in an unexpected way.
  // TODO(crbug.com/1266555): Consider how to show this in a more user-friendly
  // way.
  if (model_.distillation_in_progress()) {
    return;
  }

  // Ignore the selection if it's collapsed, which is created by a simple click.
  if ((anchor_offset == focus_offset) && (anchor_node_id == focus_node_id)) {
    return;
  }

  ui::AXNode* focus_node = model_.GetAXNode(focus_node_id);
  ui::AXNode* anchor_node = model_.GetAXNode(anchor_node_id);
  // Some text fields, like Gmail, allow a <div> to be returned as a focus
  // node for selection, most frequently when a triple click causes an entire
  // range of text to be selected, including non-text nodes. This can cause
  // inconsistencies in how the selection is handled. e.g. the focus node can
  // be before the anchor node and set to a non-text node, which can cause
  // page_handler_->OnSelectionChange to be incorrectly triggered, resulting in
  // a failing DCHECK. Therefore, return early if this happens.
  if (!focus_node->IsText() || !anchor_node->IsText()) {
    return;
  }

  // If the selection change matches the tree's selection, this means it was
  // set by the controller. Javascript selections set by the controller are
  // always forward selections. This means the anchor node always comes before
  // the focus node.
  if (anchor_node_id == model_.start_node_id() &&
      anchor_offset == model_.start_offset() &&
      focus_node_id == model_.end_node_id() &&
      focus_offset == model_.end_offset()) {
    return;
  }

  page_handler_->OnSelectionChange(model_.active_tree_id(), anchor_node_id,
                                   anchor_offset, focus_node_id, focus_offset);
}

// TODO(crbug.com/1266555): Change line_spacing and letter_spacing types from
// int to their corresponding enums.
void ReadAnythingAppController::SetThemeForTesting(const std::string& font_name,
                                                   float font_size,
                                                   SkColor foreground_color,
                                                   SkColor background_color,
                                                   int line_spacing,
                                                   int letter_spacing) {
  auto line_spacing_enum =
      static_cast<read_anything::mojom::LineSpacing>(line_spacing);
  auto letter_spacing_enum =
      static_cast<read_anything::mojom::LetterSpacing>(letter_spacing);
  OnThemeChanged(ReadAnythingTheme::New(font_name, font_size, foreground_color,
                                        background_color, line_spacing_enum,
                                        letter_spacing_enum));
}

void ReadAnythingAppController::SetContentForTesting(
    v8::Local<v8::Value> v8_snapshot_lite,
    std::vector<ui::AXNodeID> content_node_ids) {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  ui::AXTreeUpdate snapshot =
      GetSnapshotFromV8SnapshotLite(isolate, v8_snapshot_lite);
  ui::AXEvent selectionEvent;
  selectionEvent.event_type = ax::mojom::Event::kDocumentSelectionChanged;
  selectionEvent.event_from = ax::mojom::EventFrom::kUser;
  AccessibilityEventReceived(snapshot.tree_data.tree_id, {snapshot}, {});
  OnActiveAXTreeIDChanged(snapshot.tree_data.tree_id, ukm::kInvalidSourceId);
  OnAXTreeDistilled(snapshot.tree_data.tree_id, content_node_ids);

  // Trigger a selection event (for testing selections).
  AccessibilityEventReceived(snapshot.tree_data.tree_id, {snapshot},
                             {selectionEvent});
}
