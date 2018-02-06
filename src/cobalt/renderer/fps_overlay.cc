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

#include "cobalt/renderer/fps_overlay.h"

#include <string>
#include <vector>

#include "base/stringprintf.h"
#include "cobalt/render_tree/brush.h"
#include "cobalt/render_tree/color_rgba.h"
#include "cobalt/render_tree/composition_node.h"
#include "cobalt/render_tree/font.h"
#include "cobalt/render_tree/glyph_buffer.h"
#include "cobalt/render_tree/rect_node.h"
#include "cobalt/render_tree/resource_provider.h"
#include "cobalt/render_tree/text_node.h"

namespace cobalt {
namespace renderer {

namespace {
const render_tree::ColorRGBA kTextColor(1.0f, 1.0f, 1.0f, 1.0f);
const render_tree::ColorRGBA kBackgroundColor(1.0f, 0.6f, 0.6f, 1.0f);
const char* kTypefaceName = "Roboto";
const float kFontSize = 14.0f;

// Distance between the text bounds and the background bounds.
const int kTextMarginInPixels = 5;

// Spacing between each line of text.
const int kTextVerticalSpacingInPixels = 5;

scoped_refptr<render_tree::Node> ConvertLinesToOverlay(
    render_tree::ResourceProvider* resource_provider, render_tree::Font* font,
    const std::vector<std::string>& lines) {
  // First create a composition node that composes all the lines of text
  // together.
  float y_offset = kTextMarginInPixels;
  render_tree::CompositionNode::Builder text_builder;
  for (const auto& line : lines) {
    scoped_refptr<render_tree::GlyphBuffer> glyph_buffer =
        resource_provider->CreateGlyphBuffer(line, font);
    math::RectF bounds(glyph_buffer->GetBounds());

    text_builder.AddChild(new render_tree::TextNode(
        math::Vector2dF(-bounds.x() + kTextMarginInPixels,
                        -bounds.y() + y_offset),
        glyph_buffer, kTextColor));

    y_offset += bounds.height() + kTextVerticalSpacingInPixels;
  }

  scoped_refptr<render_tree::CompositionNode> text =
      new render_tree::CompositionNode(text_builder.Pass());

  // Now compose that onto a solid background.
  math::RectF background_bounds = text->GetBounds();
  background_bounds.set_height(background_bounds.height() +
                               2 * kTextMarginInPixels);
  background_bounds.set_y(0);
  background_bounds.set_width(background_bounds.width() +
                              2 * kTextMarginInPixels);
  background_bounds.set_x(0);

  render_tree::CompositionNode::Builder text_with_background_builder;
  text_with_background_builder.AddChild(new render_tree::RectNode(
      background_bounds,
      scoped_ptr<render_tree::Brush>(
          new render_tree::SolidColorBrush(kBackgroundColor))));
  text_with_background_builder.AddChild(text);

  return new render_tree::CompositionNode(text_with_background_builder.Pass());
}

scoped_refptr<render_tree::Node> ConvertFPSStatsToOverlay(
    render_tree::ResourceProvider* resource_provider, render_tree::Font* font,
    const base::CValCollectionTimerStatsFlushResults& fps_stats) {
  std::vector<std::string> lines;
  lines.push_back(base::StringPrintf(
      "Samples: %d", static_cast<unsigned int>(fps_stats.sample_count)));
  lines.push_back(base::StringPrintf("Average: %.1fms",
                                     fps_stats.average.InMillisecondsF()));
  lines.push_back(
      base::StringPrintf("Min: %.1fms", fps_stats.minimum.InMillisecondsF()));
  lines.push_back(
      base::StringPrintf("Max: %.1fms", fps_stats.maximum.InMillisecondsF()));
  lines.push_back(base::StringPrintf(
      "25th Pct: %.1fms", fps_stats.percentile_25th.InMillisecondsF()));
  lines.push_back(base::StringPrintf(
      "50th Pct: %.1fms", fps_stats.percentile_50th.InMillisecondsF()));
  lines.push_back(base::StringPrintf(
      "75th Pct: %.1fms", fps_stats.percentile_75th.InMillisecondsF()));
  lines.push_back(base::StringPrintf(
      "95th Pct: %.1fms", fps_stats.percentile_95th.InMillisecondsF()));

  return ConvertLinesToOverlay(resource_provider, font, lines);
}
}  // namespace

FpsOverlay::FpsOverlay(render_tree::ResourceProvider* resource_provider)
    : resource_provider_(resource_provider) {
  font_ = resource_provider_
              ->GetLocalTypeface(kTypefaceName, render_tree::FontStyle())
              ->CreateFontWithSize(kFontSize);
}

void FpsOverlay::UpdateOverlay(
    const base::CValCollectionTimerStatsFlushResults& fps_stats) {
  cached_overlay_ =
      ConvertFPSStatsToOverlay(resource_provider_, font_, fps_stats);
}

scoped_refptr<render_tree::Node> FpsOverlay::AnnotateRenderTreeWithOverlay(
    render_tree::Node* original_tree) {
  if (!cached_overlay_) {
    return original_tree;
  } else {
    // Compose the overlay onto the top left corner of the original render tree.
    render_tree::CompositionNode::Builder builder;
    builder.AddChild(original_tree);
    builder.AddChild(cached_overlay_);
    return new render_tree::CompositionNode(builder.Pass());
  }
}

}  // namespace renderer
}  // namespace cobalt
