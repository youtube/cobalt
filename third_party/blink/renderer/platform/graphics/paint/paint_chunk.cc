// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk.h"

#include "third_party/blink/renderer/platform/graphics/paint/drawing_display_item.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_artifact.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

template <typename T>
bool PointerValueEquals(const std::unique_ptr<T>& a,
                        const std::unique_ptr<T>& b) {
  if (!a && !b) {
    return true;
  }
  if (!a || !b) {
    return false;
  }
  return *a == *b;
}
}  // namespace

struct SameSizeAsPaintChunk {
  wtf_size_t begin_index;
  wtf_size_t end_index;
  PaintChunk::Id id;
  PaintChunk::BackgroundColorInfo background_color;
  PropertyTreeState properties;
  gfx::Rect bounds;
  gfx::Rect drawable_bounds;
  gfx::Rect rect_known_to_be_opaque;
  void* hit_test_data;
  void* region_capture_data;
  void* layer_selection;
  bool b[2];
};

ASSERT_SIZE(PaintChunk, SameSizeAsPaintChunk);

bool PaintChunk::EqualsForUnderInvalidationChecking(
    const PaintChunk& other) const {
  return size() == other.size() && id == other.id &&
         properties == other.properties && bounds == other.bounds &&
         drawable_bounds == other.drawable_bounds &&
         raster_effect_outset == other.raster_effect_outset &&
         PointerValueEquals(hit_test_data, other.hit_test_data) &&
         PointerValueEquals(region_capture_data, other.region_capture_data) &&
         effectively_invisible == other.effectively_invisible;
  // Derived fields like rect_known_to_be_opaque are not checked because they
  // are updated when we create the next chunk or release chunks. We ensure
  // their correctness with unit tests and under-invalidation checking of
  // display items.
}

size_t PaintChunk::MemoryUsageInBytes() const {
  size_t total_size = sizeof(*this);
  if (hit_test_data) {
    total_size += sizeof(*hit_test_data);
    total_size += hit_test_data->touch_action_rects.CapacityInBytes();
    total_size += hit_test_data->wheel_event_rects.CapacityInBytes();
  }
  total_size += sizeof(region_capture_data);
  if (region_capture_data) {
    total_size += sizeof(*region_capture_data);
  }
  return total_size;
}

static String ToStringImpl(const PaintChunk& c, const String& id_string) {
  StringBuilder sb;
  sb.AppendFormat(
      "PaintChunk(begin=%u, end=%u, id=%s cacheable=%d props=(%s) bounds=%s "
      "rect_known_to_be_opaque=%s effectively_invisible=%d drawscontent=%d",
      c.begin_index, c.end_index, id_string.Utf8().c_str(), c.is_cacheable,
      c.properties.ToString().Utf8().c_str(), c.bounds.ToString().c_str(),
      c.rect_known_to_be_opaque.ToString().c_str(), c.effectively_invisible,
      c.DrawsContent());
  if (c.hit_test_data) {
    sb.Append(", hit_test_data=");
    sb.Append(c.hit_test_data->ToString());
  }
  if (c.region_capture_data) {
    sb.Append(", region_capture_data=");
    sb.Append(ToString(*c.region_capture_data));
  }
  sb.Append(')');
  return sb.ToString();
}

String PaintChunk::ToString() const {
  return ToStringImpl(*this, id.ToString());
}

String PaintChunk::ToString(const PaintArtifact& paint_artifact) const {
  return ToStringImpl(*this, id.ToString(paint_artifact));
}

std::ostream& operator<<(std::ostream& os, const PaintChunk& chunk) {
  return os << chunk.ToString().Utf8();
}

}  // namespace blink
