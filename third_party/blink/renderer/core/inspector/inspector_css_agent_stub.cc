// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "third_party/blink/renderer/core/inspector/inspector_css_agent.h"


namespace blink {

void InspectorCSSAgent::Will(const probe::RecalculateStyle&) {}
void InspectorCSSAgent::Did(const probe::RecalculateStyle&) {}
void InspectorCSSAgent::ForcePseudoState(Element*, CSSSelector::PseudoType, bool*) {}
void InspectorCSSAgent::ForceStartingStyle(Element*, bool*) {}
void InspectorCSSAgent::DidMutateStyleSheet(CSSStyleSheet*) {}
void InspectorCSSAgent::DidReplaceStyleSheetText(CSSStyleSheet*, const String&) {}
void InspectorCSSAgent::GetTextPosition(wtf_size_t, const String*, TextPosition*) {}
void InspectorCSSAgent::LocalFontsEnabled(bool*) {}
void InspectorCSSAgent::DidUpdateComputedStyle(Element*, const ComputedStyle*, const ComputedStyle*) {}
void InspectorCSSAgent::Trace(Visitor*) const {}
void InspectorCSSAgent::WillChangeStyleElement(Element*) {}
void InspectorCSSAgent::DocumentDetached(Document*) {}
void InspectorCSSAgent::ActiveStyleSheetsUpdated(Document*) {}
void InspectorCSSAgent::FontsUpdated(const FontFace*, const String&, const FontCustomPlatformData*) {}
void InspectorCSSAgent::MediaQueryResultChanged() {}

} // namespace blink
