// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_APP_MODEL_H_
#define CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_APP_MODEL_H_

#include "base/containers/contains.h"
#include "base/values.h"
#include "chrome/common/accessibility/read_anything.mojom.h"
#include "chrome/common/accessibility/read_anything_constants.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/accessibility/ax_event_generator.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_selection.h"
#include "ui/accessibility/ax_tree_manager.h"

namespace ui {
class AXNode;
class AXSerializableTree;
}  // namespace ui

namespace ukm {
class MojoUkmRecorder;
}

// A class that holds state for the ReadAnythingAppController for the Read
// Anything WebUI app.
class ReadAnythingAppModel {
 public:
  ReadAnythingAppModel();
  ~ReadAnythingAppModel();
  ReadAnythingAppModel(const ReadAnythingAppModel& other) = delete;
  ReadAnythingAppModel& operator=(const ReadAnythingAppModel&) = delete;

  bool requires_distillation() { return requires_distillation_; }
  void set_requires_distillation(bool value) { requires_distillation_ = value; }
  bool requires_post_process_selection() {
    return requires_post_process_selection_;
  }
  void set_requires_post_process_selection(bool value) {
    requires_post_process_selection_ = value;
  }
  bool selection_from_action() { return selection_from_action_; }
  void set_selection_from_action(bool value) { selection_from_action_ = value; }

  const std::string& default_language_code() const {
    return default_language_code_;
  }

  void set_default_language_code(const std::string code) {
    default_language_code_ = code;
  }

  std::vector<std::string> GetSupportedFonts() const;

  // TODO(b/1266555): Ensure there is proper test coverage for all methods.
  // Theme
  const std::string& font_name() const { return font_name_; }
  float font_size() const { return font_size_; }
  float letter_spacing() const { return letter_spacing_; }
  float line_spacing() const { return line_spacing_; }
  int color_theme() const { return color_theme_; }
  int highlight_granularity() const { return highlight_granularity_; }
  const SkColor& foreground_color() const { return foreground_color_; }
  const SkColor& background_color() const { return background_color_; }
  float speech_rate() const { return speech_rate_; }
  const base::Value::Dict& voices() const { return voices_; }

  // Selection.
  bool has_selection() const { return has_selection_; }
  ui::AXNodeID start_node_id() const { return start_node_id_; }
  ui::AXNodeID end_node_id() const { return end_node_id_; }
  int32_t start_offset() const { return start_offset_; }
  int32_t end_offset() const { return end_offset_; }

  bool distillation_in_progress() const { return distillation_in_progress_; }
  bool active_tree_selectable() const { return active_tree_selectable_; }
  bool is_empty() const {
    return display_node_ids_.empty() && selection_node_ids_.empty();
  }

  bool page_finished_loading_for_data_collection() const {
    return page_finished_loading_for_data_collection_;
  }

  const ukm::SourceId& active_ukm_source_id() const {
    return active_ukm_source_id_;
  }

  const std::vector<ui::AXNodeID>& content_node_ids() const {
    return content_node_ids_;
  }
  const std::set<ui::AXNodeID>& display_node_ids() const {
    return display_node_ids_;
  }
  const std::set<ui::AXNodeID>& selection_node_ids() const {
    return selection_node_ids_;
  }

  // Returns the active tree id. For PDFs, this will return the tree id of the
  // PDF iframe, since that is where the PDF contents are. If that tree id is
  // not yet in the model, AXTreeIDUnknown will be returned.
  ui::AXTreeID GetActiveTreeId() const;

  void SetDistillationInProgress(bool distillation) {
    distillation_in_progress_ = distillation;
  }
  void SetActiveTreeSelectable(bool active_tree_selectable) {
    active_tree_selectable_ = active_tree_selectable;
  }
  void SetActiveUkmSourceId(ukm::SourceId source_id);

  void SetActiveTreeId(ui::AXTreeID tree_id) { active_tree_id_ = tree_id; }

  ui::AXNode* GetAXNode(ui::AXNodeID ax_node_id) const;
  bool IsNodeIgnoredForReadAnything(ui::AXNodeID ax_node_id) const;
  bool NodeIsContentNode(ui::AXNodeID ax_node_id) const;
  void OnThemeChanged(read_anything::mojom::ReadAnythingThemePtr new_theme);
  void OnSettingsRestoredFromPrefs(
      read_anything::mojom::LineSpacing line_spacing,
      read_anything::mojom::LetterSpacing letter_spacing,
      const std::string& font,
      double font_size,
      read_anything::mojom::Colors color,
      double speech_rate,
      base::Value::Dict* voices,
      read_anything::mojom::HighlightGranularity granularity);
  void OnScroll(bool on_selection, bool from_reading_mode) const;

  void Reset(const std::vector<ui::AXNodeID>& content_node_ids);
  bool PostProcessSelection();
  // Helper functions for the rendering algorithm. Post-process the AXTree and
  // cache values before sending an `updateContent` notification to the Read
  // Anything app.ts.
  // ComputeDisplayNodeIdsForDistilledTree computes display nodes from the
  // content nodes. These display nodes will be displayed in Read Anything
  // app.ts by default.
  // ComputeSelectionNodeIds computes selection nodes from
  // the user's selection. The selection nodes list is only populated when the
  // user's selection contains nodes outside of the display nodes list. By
  // keeping two separate lists of nodes, we can switch back to displaying the
  // default distilled content without recomputing the nodes when the user
  // clears their selection or selects content inside the distilled content.
  void ComputeDisplayNodeIdsForDistilledTree();

  ui::AXSerializableTree* GetTreeFromId(ui::AXTreeID tree_id) const;
  void AddTree(ui::AXTreeID tree_id,
               std::unique_ptr<ui::AXSerializableTree> tree);

  bool ContainsTree(ui::AXTreeID tree_id) const;

  void UnserializePendingUpdates(ui::AXTreeID tree_id);

  void ClearPendingUpdates();

  void AccessibilityEventReceived(const ui::AXTreeID& tree_id,
                                  const std::vector<ui::AXTreeUpdate>& updates,
                                  const std::vector<ui::AXEvent>& events);

  void OnAXTreeDestroyed(const ui::AXTreeID& tree_id);

  std::map<ui::AXTreeID, std::vector<ui::AXTreeUpdate>>&
  GetPendingUpdatesForTesting();

  std::map<ui::AXTreeID, std::unique_ptr<ui::AXTreeManager>>*
  GetTreesForTesting();

  void EraseTreeForTesting(ui::AXTreeID tree_id);

  double GetLineSpacingValue(
      read_anything::mojom::LineSpacing line_spacing) const;
  double GetLetterSpacingValue(
      read_anything::mojom::LetterSpacing letter_spacing) const;

  void IncreaseTextSize();
  void DecreaseTextSize();
  void ResetTextSize();

  // PDF handling.
  void SetIsPdf(const GURL& url);
  bool is_pdf() const { return is_pdf_; }
  ui::AXTreeID GetPDFWebContents() const;

  // Checks assumptions made about the PDF's structure, specifically that the
  // main web contents AXTree has one child (the pdf web contents), and that
  // the pdf web contents AXTree has one child (the pdf iframe). If there is
  // not enough information to check a certain assumption, ie the model does
  // not contain a certain tree, this function could still return true. When
  // tree updates are received for the missing tree(s), this function should
  // be ran again to check for the correct structure.
  bool IsPDFFormatted() const;

 private:
  void EraseTree(ui::AXTreeID tree_id);

  void InsertDisplayNode(ui::AXNodeID node);
  void ResetSelection();
  void InsertSelectionNode(ui::AXNodeID node);
  void UpdateSelection();
  void ComputeSelectionNodeIds();
  bool SelectionInsideDisplayNodes();

  void AddPendingUpdates(const ui::AXTreeID tree_id,
                         const std::vector<ui::AXTreeUpdate>& updates);

  void UnserializeUpdates(const std::vector<ui::AXTreeUpdate>& updates,
                          const ui::AXTreeID& tree_id);

  const std::vector<ui::AXTreeUpdate>& GetOrCreatePendingUpdateAt(
      ui::AXTreeID tree_id);

  void ProcessNonGeneratedEvents(const std::vector<ui::AXEvent>& events);
  void ProcessGeneratedEvents(const ui::AXEventGenerator& event_generator);

  ui::AXNode* GetParentForSelection(ui::AXNode* node);

  // State.
  // Store AXTrees of web contents in the browser's tab strip as AXTreeManagers.
  std::map<ui::AXTreeID, std::unique_ptr<ui::AXTreeManager>> tree_managers_;

  // The AXTreeID of the currently active web contents. For PDFs, this will
  // always be the AXTreeID of the main web contents (not the PDF iframe or its
  // child).
  ui::AXTreeID active_tree_id_ = ui::AXTreeIDUnknown();

  // The UKM source ID of the main frame of the active web contents, whose
  // AXTree has ID active_tree_id_. This is used for metrics collection.
  ukm::SourceId active_ukm_source_id_ = ukm::kInvalidSourceId;

  // Certain websites (e.g. Docs and PDFs) are not distillable with selection.
  bool active_tree_selectable_ = true;

  // PDFs are handled differently than regular webpages. That is because they
  // are stored in a different web contents and the actual PDF text is inside an
  // iframe. In order to get tree information from the PDF web contents, we need
  // to enable accessibility on it first. Then, we will get tree updates from
  // the iframe to send to the distiller.
  // This is the flow:
  //    main web contents -> pdf web contents -> iframe
  // In accessibility terms:
  //    AXTree -(via child tree)-> AXTree -(via child tree)-> AXTree
  // The last AXTree is the one we want to send to the distiller since it
  // contains the PDF text.
  bool is_pdf_ = false;

  // Distillation is slow and happens out-of-process when Screen2x is running.
  // This boolean marks when distillation is in progress to avoid sending
  // new distillation requests during that time.
  bool distillation_in_progress_ = false;

  // A mapping of a tree ID to a queue of pending updates on the active AXTree,
  // which will be unserialized once distillation completes.
  std::map<ui::AXTreeID, std::vector<ui::AXTreeUpdate>> pending_updates_map_;

  // The node IDs identified as main by the distiller. These are static text
  // nodes when generated by Screen2x. When generated by the rules-based
  // distiller, these are heading or paragraph subtrees.
  std::vector<ui::AXNodeID> content_node_ids_;

  // This contains all ancestors and descendants of each content node. These
  // nodes will be displayed in the Read Anything app if there is no user
  // selection or if the users selection is contained within these nodes.
  std::set<ui::AXNodeID> display_node_ids_;

  // If the user's selection contains nodes outside of display_node_ids, this
  // contains all nodes between the start and end nodes of the selection.
  std::set<ui::AXNodeID> selection_node_ids_;

  std::string default_language_code_ = "en-US";

  // Theme information.
  std::string font_name_ = string_constants::kReadAnythingPlaceholderFontName;
  float font_size_ = kReadAnythingDefaultFontScale;
  float letter_spacing_ =
      (int)read_anything::mojom::LetterSpacing::kDefaultValue;
  float line_spacing_ = (int)read_anything::mojom::LineSpacing::kDefaultValue;
  SkColor background_color_ = (int)read_anything::mojom::Colors::kDefaultValue;
  SkColor foreground_color_ = (int)read_anything::mojom::Colors::kDefaultValue;
  int color_theme_ = (int)read_anything::mojom::Colors::kDefaultValue;
  float speech_rate_ = kReadAnythingDefaultSpeechRate;
  base::Value::Dict voices_ = base::Value::Dict();
  int highlight_granularity_ =
      (int)read_anything::mojom::HighlightGranularity::kDefaultValue;

  // Selection information.
  bool has_selection_ = false;
  ui::AXNodeID start_node_id_ = ui::kInvalidAXNodeID;
  ui::AXNodeID end_node_id_ = ui::kInvalidAXNodeID;
  int32_t start_offset_ = -1;
  int32_t end_offset_ = -1;
  bool requires_distillation_ = false;
  bool requires_post_process_selection_ = false;
  bool selection_from_action_ = false;

  std::unique_ptr<ukm::MojoUkmRecorder> ukm_recorder_;

  // Used to keep track of how many selections were made for the
  // active_ukm_source_id_. Only recorded during the select-to-distill flow
  // (when the empty state page is shown).
  int32_t num_selections_ = 0;

  // For screen2x data collection, Chrome is launched from the CLI to open one
  // webpage. We record the result of the distill() call for this entire
  // webpage, so we only make the call once the webpage finished loading.
  bool page_finished_loading_for_data_collection_ = false;
};

#endif  // CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_APP_MODEL_H_
