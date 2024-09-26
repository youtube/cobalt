// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/hardware_display_plane.h"

#include <drm_fourcc.h>
#include <drm_mode.h>

#include <string>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/trace_event/traced_value.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/drm_gpu_util.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager.h"

namespace ui {

namespace {

void ParseSupportedFormatsAndModifiers(
    drmModePropertyBlobPtr blob,
    std::vector<uint32_t>* supported_formats,
    std::vector<drm_format_modifier>* supported_format_modifiers) {
  auto* data = static_cast<const uint8_t*>(blob->data);
  auto* header = reinterpret_cast<const drm_format_modifier_blob*>(data);
  auto* formats =
      reinterpret_cast<const uint32_t*>(data + header->formats_offset);
  auto* modifiers = reinterpret_cast<const drm_format_modifier*>(
      data + header->modifiers_offset);

  for (uint32_t k = 0; k < header->count_formats; k++)
    supported_formats->push_back(formats[k]);

  for (uint32_t k = 0; k < header->count_modifiers; k++)
    supported_format_modifiers->push_back(modifiers[k]);
}

std::string IdSetToString(const base::flat_set<uint32_t>& ids) {
  std::vector<std::string> string_ids;
  for (auto id : ids)
    string_ids.push_back(std::to_string(id));
  return "[" + base::JoinString(string_ids, ", ") + "]";
}

}  // namespace

HardwareDisplayPlane::Properties::Properties() = default;
HardwareDisplayPlane::Properties::~Properties() = default;

HardwareDisplayPlane::HardwareDisplayPlane(uint32_t id) : id_(id) {}

HardwareDisplayPlane::~HardwareDisplayPlane() = default;

bool HardwareDisplayPlane::CanUseForCrtcId(uint32_t crtc_id) const {
  return possible_crtc_ids_.contains(crtc_id);
}

void HardwareDisplayPlane::WriteIntoTrace(perfetto::TracedValue context) const {
  auto dict = std::move(context).WriteDictionary();

  dict.Add("plane_id", id_);
  dict.Add("owning_crtc", owning_crtc_);
  dict.Add("in_use", in_use_);

  dict.Add("possible_crtc_ids", possible_crtc_ids_);

  auto type = dict.AddItem("type");
  switch (properties_.type.value) {
    case DRM_PLANE_TYPE_OVERLAY:
      std::move(type).WriteString("DRM_PLANE_TYPE_OVERLAY");
      break;
    case DRM_PLANE_TYPE_PRIMARY:
      std::move(type).WriteString("DRM_PLANE_TYPE_PRIMARY");
      break;
    case DRM_PLANE_TYPE_CURSOR:
      std::move(type).WriteString("DRM_PLANE_TYPE_CURSOR");
      break;
    default:
      NOTREACHED();
  }
}

bool HardwareDisplayPlane::Initialize(DrmDevice* drm) {
  InitializeProperties(drm);

  ScopedDrmPlanePtr drm_plane(drm->GetPlane(id_));
  DCHECK(drm_plane);

  possible_crtc_ids_ =
      drm->plane_manager()->CrtcMaskToCrtcIds(drm_plane->possible_crtcs);

  if (properties_.in_formats.id) {
    ScopedDrmPropertyBlobPtr blob(
        drm->GetPropertyBlob(properties_.in_formats.value));
    DCHECK(blob) << "No blob found with id=" << properties_.in_formats.value;
    ParseSupportedFormatsAndModifiers(blob.get(), &supported_formats_,
                                      &supported_format_modifiers_);
  }

  if (supported_formats_.empty()) {
    for (uint32_t i = 0; i < drm_plane->count_formats; ++i)
      supported_formats_.push_back(drm_plane->formats[i]);
  }

  if (properties_.type.id)
    type_ = properties_.type.value;

  if (properties_.plane_color_encoding.id) {
    color_encoding_bt601_ = GetEnumValueForName(
        *drm, properties_.plane_color_encoding.id, "ITU-R BT.601 YCbCr");
    color_range_limited_ = GetEnumValueForName(
        *drm, properties_.plane_color_range.id, "YCbCr limited range");
  }

  VLOG(3) << "Initialized plane=" << id_
          << " possible_crtc_ids=" << IdSetToString(possible_crtc_ids_)
          << " supported_formats_count=" << supported_formats_.size()
          << " supported_modifiers_count="
          << supported_format_modifiers_.size();
  return true;
}

bool HardwareDisplayPlane::IsSupportedFormat(uint32_t format) {
  if (!format)
    return false;

  if (last_used_format_ == format)
    return true;

  for (auto& element : supported_formats_) {
    if (element == format) {
      last_used_format_ = format;
      return true;
    }
  }

  last_used_format_ = 0;
  return false;
}

const std::vector<uint32_t>& HardwareDisplayPlane::supported_formats() const {
  return supported_formats_;
}

std::vector<uint64_t> HardwareDisplayPlane::ModifiersForFormat(
    uint32_t format) const {
  std::vector<uint64_t> modifiers;

  auto it = base::ranges::find(supported_formats_, format);
  if (it == supported_formats_.end())
    return modifiers;

  uint32_t format_index = it - supported_formats_.begin();
  for (const auto& modifier : supported_format_modifiers_) {
    // modifier.formats is a bitmask of the formats the modifier
    // applies to, starting at format modifier.offset. That is, if bit
    // n is set in modifier.formats, the modifier applies to format
    // modifier.offset + n.  In the expression below, if format_index
    // is lower than modifier.offset or more than 63 higher than
    // modifier.offset, the right hand side of the shift is 64 or
    // above and the result will be 0. That means that the modifier
    // doesn't apply to this format, which is what we want.
    if (modifier.formats & (1ull << (format_index - modifier.offset)))
      modifiers.push_back(modifier.modifier);
  }

  return modifiers;
}

void HardwareDisplayPlane::InitializeProperties(DrmDevice* drm) {
  ScopedDrmObjectPropertyPtr props =
      drm->GetObjectProperties(id_, DRM_MODE_OBJECT_PLANE);
  GetDrmPropertyForName(drm, props.get(), "CRTC_ID", &properties_.crtc_id);
  GetDrmPropertyForName(drm, props.get(), "CRTC_X", &properties_.crtc_x);
  GetDrmPropertyForName(drm, props.get(), "CRTC_Y", &properties_.crtc_y);
  GetDrmPropertyForName(drm, props.get(), "CRTC_W", &properties_.crtc_w);
  GetDrmPropertyForName(drm, props.get(), "CRTC_H", &properties_.crtc_h);
  GetDrmPropertyForName(drm, props.get(), "FB_ID", &properties_.fb_id);
  GetDrmPropertyForName(drm, props.get(), "SRC_X", &properties_.src_x);
  GetDrmPropertyForName(drm, props.get(), "SRC_Y", &properties_.src_y);
  GetDrmPropertyForName(drm, props.get(), "SRC_W", &properties_.src_w);
  GetDrmPropertyForName(drm, props.get(), "SRC_H", &properties_.src_h);
  GetDrmPropertyForName(drm, props.get(), "type", &properties_.type);
  GetDrmPropertyForName(drm, props.get(), "rotation", &properties_.rotation);
  GetDrmPropertyForName(drm, props.get(), "IN_FORMATS",
                        &properties_.in_formats);
  GetDrmPropertyForName(drm, props.get(), "IN_FENCE_FD",
                        &properties_.in_fence_fd);
  GetDrmPropertyForName(drm, props.get(), "PLANE_CTM", &properties_.plane_ctm);
  GetDrmPropertyForName(drm, props.get(), "COLOR_ENCODING",
                        &properties_.plane_color_encoding);
  GetDrmPropertyForName(drm, props.get(), "COLOR_RANGE",
                        &properties_.plane_color_range);
}

}  // namespace ui
