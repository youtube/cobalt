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

#include "base/compiler_specific.h"

#include "starboard/accessibility.h"
#include "starboard/memory.h"

#if SB_HAS(CAPTIONS)
bool SbAccessibilityGetCaptionSettings(
  SbAccessibilityCaptionSettings* caption_settings) {
  if (!caption_settings ||
      !SbMemoryIsZero(caption_settings,
                      sizeof(SbAccessibilityCaptionSettings))) {
    return false;
  }

  caption_settings->background_color = kSbAccessibilityCaptionColorWhite;

  caption_settings->background_color_state =
    kSbAccessibilityCaptionStateUnsupported;

  caption_settings->background_opacity =
    kSbAccessibilityCaptionOpacityPercentage100;

  caption_settings->background_opacity_state =
    kSbAccessibilityCaptionStateUnsupported;

  caption_settings->character_edge_style =
    kSbAccessibilityCaptionCharacterEdgeStyleNone;

  caption_settings->character_edge_style_state =
    kSbAccessibilityCaptionStateUnsupported;

  caption_settings->font_color = kSbAccessibilityCaptionColorWhite;

  caption_settings->font_color_state = kSbAccessibilityCaptionStateUnsupported;

  caption_settings->font_size = kSbAccessibilityCaptionFontSizePercentage100;

  caption_settings->font_size_state = kSbAccessibilityCaptionStateUnsupported;

  caption_settings->font_opacity = kSbAccessibilityCaptionOpacityPercentage100;

  caption_settings->font_opacity_state =
    kSbAccessibilityCaptionStateUnsupported;

  caption_settings->window_color = kSbAccessibilityCaptionColorWhite;

  caption_settings->window_color_state =
    kSbAccessibilityCaptionStateUnsupported;

  caption_settings->window_opacity =
    kSbAccessibilityCaptionOpacityPercentage100;

  caption_settings->window_opacity_state =
    kSbAccessibilityCaptionStateUnsupported;

  caption_settings->is_enabled = false;

  caption_settings->supports_is_enabled = false;
  caption_settings->supports_set_enabled = false;

  return true;
}
#endif  // SB_HAS(CAPTIONS)
