// Copyright 2017 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/accessibility/screen_reader.h"

#include <string>

#include "base/stringprintf.h"
#include "cobalt/accessibility/internal/live_region.h"
#include "cobalt/accessibility/text_alternative.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/html_body_element.h"
#include "cobalt/dom/node_list.h"

namespace cobalt {
namespace accessibility {
namespace {
// When text is removed from a live region, add the word "Removed" to indicate
// that the text has been removed.
// TODO: Support other languages.
const char kRemovedFormatString[] = "Removed. %s";
}

ScreenReader::ScreenReader(dom::Document* document, TTSEngine* tts_engine,
                           dom::MutationObserverTaskManager* task_manager)
    : document_(document), tts_engine_(tts_engine) {
  document_->AddObserver(this);
  live_region_observer_ = new dom::MutationObserver(
      base::Bind(&ScreenReader::MutationObserverCallback,
                 base::Unretained(this)),
      task_manager);
}

ScreenReader::~ScreenReader() {
  live_region_observer_->Disconnect();
  document_->RemoveObserver(this);
}

void ScreenReader::OnLoad() {
  dom::MutationObserverInit init;
  init.set_subtree(true);
  // Track character data and node addition/removal for live regions.
  init.set_character_data(true);
  init.set_child_list(true);

  if (document_->body()) {
    live_region_observer_->Observe(document_->body(), init);
  }
}

void ScreenReader::OnFocusChanged() {
  // TODO: Only utter text if text-to-speech is enabled.
  scoped_refptr<dom::Element> element = document_->active_element();
  if (element) {
    std::string text_alternative = ComputeTextAlternative(element);
    tts_engine_->SpeakNow(text_alternative);
  }
}

// MutationObserver callback for tracking the creationg and destruction of
// live regions.
void ScreenReader::MutationObserverCallback(
    const MutationRecordSequence& sequence,
    const scoped_refptr<dom::MutationObserver>& observer) {
  // TODO: Only check live regions if text-to-speech is enabled.
  DCHECK_EQ(observer, live_region_observer_);
  for (size_t i = 0; i < sequence.size(); ++i) {
    scoped_refptr<dom::MutationRecord> record = sequence.at(i);

    // Check if the target node is part of a live region.
    scoped_ptr<internal::LiveRegion> live_region =
        internal::LiveRegion::GetLiveRegionForNode(record->target());
    if (!live_region) {
      continue;
    }

    const char* format_string = NULL;
    // Get a list of changed nodes, based on the type of change this was and
    // whether or not the live region cares about this type of change.
    scoped_refptr<dom::NodeList> changed_nodes;
    if (record->type() == base::Tokens::characterData() &&
        live_region->IsMutationRelevant(
            internal::LiveRegion::kMutationTypeText)) {
      changed_nodes = new dom::NodeList();
      changed_nodes->AppendNode(record->target());
    } else if (record->type() == base::Tokens::childList()) {
      if (record->added_nodes() &&
          live_region->IsMutationRelevant(
              internal::LiveRegion::kMutationTypeAddition)) {
        changed_nodes = record->added_nodes();
      }
      if (record->removed_nodes() &&
          live_region->IsMutationRelevant(
              internal::LiveRegion::kMutationTypeRemoval)) {
        changed_nodes = record->removed_nodes();
        format_string = kRemovedFormatString;
      }
    }
    // If we don't have changed nodes, that means that the change was not
    // relevant to this live region.
    if (!changed_nodes || (changed_nodes->length() <= 0)) {
      continue;
    }

    // Check if any of the changed nodes should update the live region
    // atomically.
    bool is_atomic = false;
    for (size_t i = 0; i < changed_nodes->length(); ++i) {
      if (live_region->IsAtomic(changed_nodes->Item(i))) {
        is_atomic = true;
        break;
      }
    }

    std::string text_alternative;
    if (is_atomic) {
      // If the live region updates atomically, then ignore changed_nodes
      // and just get the text alternative from the entire region.
      text_alternative = ComputeTextAlternative(live_region->root());
    } else {
      // Otherwise append all the text alternatives for each node.
      for (size_t i = 0; i < changed_nodes->length(); ++i) {
        text_alternative += ComputeTextAlternative(changed_nodes->Item(i));
      }
    }

    // If we still don't have a text_alternative to speak, continue to the next
    // mutation.
    if (text_alternative.empty()) {
      continue;
    }

    // Provide additional context through a format string, if necessary.
    if (format_string) {
      text_alternative = StringPrintf(format_string, text_alternative.c_str());
    }

    // Utter the text according to whether this live region is assertive or not.
    if (live_region->IsAssertive()) {
      tts_engine_->SpeakNow(text_alternative);
    } else {
      tts_engine_->Speak(text_alternative);
    }
  }
}

}  // namespace accessibility
}  // namespace cobalt
