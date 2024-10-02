// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/fake/fake_display_snapshot.h"

#include <inttypes.h>
#include <stdint.h>

#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/display/util/display_util.h"

using base::StringPiece;

namespace display {

namespace {

// Get pixel pitch in millimeters from DPI.
constexpr float PixelPitchMmFromDPI(float dpi) {
  return kInchInMm / dpi;
}

// Extracts text after specified delimiter. If the delimiter doesn't appear
// exactly once the result will be empty and the input string will be
// unmodified. Otherwise, the input string will contain the text before the
// delimiter and the result will be the text after the delimiter.
StringPiece ExtractSuffix(StringPiece* str, StringPiece delimiter) {
  std::vector<StringPiece> parts = base::SplitStringPiece(
      *str, delimiter, base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  if (parts.size() == 2) {
    *str = parts[0];
    return parts[1];
  }

  return StringPiece();
}

// Parses a display mode from |str| in the format HxW[%R], returning null if
// |str| is invalid.
std::unique_ptr<DisplayMode> ParseDisplayMode(const std::string& str) {
  int width = 0;
  int height = 0;
  std::string refresh_rate_str;

  // Check against regex and extract values.
  if (!RE2::FullMatch(str, "(\\d+)x(\\d+)(?:%(\\d+\\.?\\d*))?", &width, &height,
                      &refresh_rate_str)) {
    LOG(ERROR) << "Invalid display mode string \"" << str << "\"";
    return nullptr;
  }

  if (width <= 0 || height <= 0) {
    LOG(ERROR) << "Resolution " << width << "x" << height << " is invalid";
    return nullptr;
  }

  // Refresh rate is optional and will be be 60 if not specified.
  double refresh_rate = 60.0f;
  if (!refresh_rate_str.empty() &&
      !base::StringToDouble(refresh_rate_str, &refresh_rate)) {
    LOG(ERROR) << "Unable to parse display mode \"" << str << "\"";
    return nullptr;
  }

  return std::make_unique<DisplayMode>(gfx::Size(width, height), false,
                                       static_cast<float>(refresh_rate));
}

// Parses a list of alternate display modes, adding each new display mode to
// |builder|. Returns false if any of the modes are invalid.
bool HandleModes(FakeDisplaySnapshot::Builder* builder,
                 StringPiece resolutions) {
  for (const std::string& mode_str :
       base::SplitString(resolutions, ":", base::TRIM_WHITESPACE,
                         base::SPLIT_WANT_NONEMPTY)) {
    std::unique_ptr<DisplayMode> mode = ParseDisplayMode(mode_str);
    if (!mode)
      return false;

    builder->AddMode(std::move(mode));
  }

  return true;
}

// Parses device DPI and updates |builder|. Returns false if an invalid DPI
// string is provided.
bool HandleDPI(FakeDisplaySnapshot::Builder* builder, StringPiece dpi) {
  if (dpi.empty())
    return true;

  int dpi_value = 0;
  if (base::StringToInt(dpi, &dpi_value)) {
    builder->SetDPI(dpi_value);
    return true;
  }

  LOG(ERROR) << "Invalid DPI string \"" << dpi << "\"";
  return false;
}

// Parses a list of display options and set each option true on |builder|.
// Returns false if any invalid options are provided. If an option appears more
// than once it will have no effect the second time.
bool HandleOptions(FakeDisplaySnapshot::Builder* builder, StringPiece options) {
  for (size_t i = 0; i < options.size(); ++i) {
    switch (options[i]) {
      case 'o':
        builder->SetHasOverscan(true);
        break;
      case 'c':
        builder->SetHasColorCorrectionMatrix(true);
        break;
      case 'a':
        builder->SetIsAspectPreservingScaling(true);
        break;
      case 'i':
        builder->SetType(DISPLAY_CONNECTION_TYPE_INTERNAL);
        break;
      default:
        LOG(ERROR) << "Invalid option specifier \"" << options[i] << "\"";
        return false;
    }
  }

  return true;
}

}  // namespace

using Builder = FakeDisplaySnapshot::Builder;

Builder::Builder() {}

Builder::~Builder() {}

std::unique_ptr<FakeDisplaySnapshot> Builder::Build() {
  if (modes_.empty() || id_ == kInvalidDisplayId) {
    NOTREACHED() << "Display modes or display ID missing";
    return nullptr;
  }

  // Add a name if none is provided.
  if (name_.empty())
    name_ = base::StringPrintf("Fake Display with port_id=%" PRId64
                               ", edid_id=%" PRId64,
                               port_display_id_, edid_display_id_);

  // If there is no native mode set, use the first display mode.
  if (!native_mode_)
    native_mode_ = modes_.back().get();

  // Calculate physical size to match set DPI.
  gfx::Size physical_size =
      gfx::ScaleToRoundedSize(native_mode_->size(), PixelPitchMmFromDPI(dpi_));

  return std::make_unique<FakeDisplaySnapshot>(
      id_, port_display_id_, edid_display_id_, connector_index_, origin_,
      physical_size, type_, base_connector_id_, path_topology_,
      is_aspect_preserving_scaling_, has_overscan_, privacy_screen_state_,
      has_content_protection_key_, has_color_correction_matrix_,
      color_correction_in_linear_space_, name_, std::move(modes_),
      current_mode_, native_mode_, product_code_, maximum_cursor_size_,
      color_space_, bits_per_channel_, hdr_static_metadata_,
      variable_refresh_rate_state_, vsync_rate_min_, DrmFormatsAndModifiers());
}

Builder& Builder::SetId(int64_t id) {
  id_ = id;
  return *this;
}

Builder& Builder::SetPortDisplayId(int64_t id) {
  port_display_id_ = id;
  return *this;
}

Builder& Builder::SetEdidDisplayId(int64_t id) {
  edid_display_id_ = id;
  return *this;
}

Builder& Builder::SetConnectorIndex(uint16_t index) {
  connector_index_ = index;
  return *this;
}

Builder& Builder::SetNativeMode(const gfx::Size& size) {
  native_mode_ = AddOrFindDisplayMode(size);
  return *this;
}

Builder& Builder::SetNativeMode(std::unique_ptr<DisplayMode> mode) {
  native_mode_ = AddOrFindDisplayMode(std::move(mode));
  return *this;
}

Builder& Builder::SetCurrentMode(const gfx::Size& size) {
  current_mode_ = AddOrFindDisplayMode(size);
  return *this;
}

Builder& Builder::SetCurrentMode(std::unique_ptr<DisplayMode> mode) {
  current_mode_ = AddOrFindDisplayMode(std::move(mode));
  return *this;
}

Builder& Builder::AddMode(const gfx::Size& size) {
  AddOrFindDisplayMode(size);
  return *this;
}

Builder& Builder::AddMode(std::unique_ptr<DisplayMode> mode) {
  AddOrFindDisplayMode(std::move(mode));
  return *this;
}

Builder& Builder::SetOrigin(const gfx::Point& origin) {
  origin_ = origin;
  return *this;
}

Builder& Builder::SetType(DisplayConnectionType type) {
  type_ = type;
  return *this;
}

Builder& Builder::SetBaseConnectorId(uint64_t base_connector_id) {
  base_connector_id_ = base_connector_id;
  return *this;
}

Builder& Builder::SetPathTopology(const std::vector<uint64_t>& path_topology) {
  path_topology_ = path_topology;
  return *this;
}

Builder& Builder::SetIsAspectPreservingScaling(bool val) {
  is_aspect_preserving_scaling_ = val;
  return *this;
}

Builder& Builder::SetHasOverscan(bool has_overscan) {
  has_overscan_ = has_overscan;
  return *this;
}

Builder& Builder::SetHasColorCorrectionMatrix(bool val) {
  has_color_correction_matrix_ = val;
  return *this;
}

Builder& Builder::SetColorCorrectionInLinearSpace(bool val) {
  color_correction_in_linear_space_ = val;
  return *this;
}

Builder& Builder::SetName(const std::string& name) {
  name_ = name;
  return *this;
}

Builder& Builder::SetProductCode(int64_t product_code) {
  product_code_ = product_code;
  return *this;
}

Builder& Builder::SetMaximumCursorSize(const gfx::Size& maximum_cursor_size) {
  maximum_cursor_size_ = maximum_cursor_size;
  return *this;
}

Builder& Builder::SetDPI(int dpi) {
  dpi_ = static_cast<float>(dpi);
  return *this;
}

Builder& Builder::SetLowDPI() {
  return SetDPI(96);
}

Builder& Builder::SetHighDPI() {
  return SetDPI(326);  // Retina-ish.
}

Builder& Builder::SetPrivacyScreen(PrivacyScreenState state) {
  privacy_screen_state_ = state;
  return *this;
}

Builder& Builder::SetHasContentProtectionKey(bool has_content_protection_key) {
  has_content_protection_key_ = has_content_protection_key;
  return *this;
}

Builder& Builder::SetColorSpace(const gfx::ColorSpace& color_space) {
  color_space_ = color_space;
  return *this;
}

Builder& Builder::SetBitsPerChannel(uint32_t bits_per_channel) {
  bits_per_channel_ = bits_per_channel;
  return *this;
}

Builder& Builder::SetHDRStaticMetadata(
    const gfx::HDRStaticMetadata& hdr_static_metadata) {
  hdr_static_metadata_ = hdr_static_metadata;
  return *this;
}

Builder& Builder::SetVariableRefreshRateState(
    VariableRefreshRateState variable_refresh_rate_state) {
  variable_refresh_rate_state_ = variable_refresh_rate_state;
  return *this;
}

Builder& Builder::SetVsyncRateMin(
    const absl::optional<uint16_t>& vsync_rate_min) {
  vsync_rate_min_ = vsync_rate_min;
  return *this;
}

const DisplayMode* Builder::AddOrFindDisplayMode(const gfx::Size& size) {
  for (auto& mode : modes_) {
    if (mode->size() == size)
      return mode.get();
  }

  // Not found, insert a mode with the size and return.
  modes_.push_back(std::make_unique<DisplayMode>(size, false, 60.0f));
  return modes_.back().get();
}

const DisplayMode* Builder::AddOrFindDisplayMode(
    std::unique_ptr<DisplayMode> mode) {
  for (auto& existing : modes_) {
    if (mode->size() == existing->size() &&
        mode->is_interlaced() == existing->is_interlaced() &&
        mode->refresh_rate() == existing->refresh_rate()) {
      return existing.get();
    }
  }

  // Not found, insert mode and return.
  modes_.push_back(std::move(mode));
  return modes_.back().get();
}

FakeDisplaySnapshot::FakeDisplaySnapshot(
    int64_t display_id,
    int64_t port_display_id,
    int64_t edid_display_id,
    uint16_t connector_index,
    const gfx::Point& origin,
    const gfx::Size& physical_size,
    DisplayConnectionType type,
    uint64_t base_connector_id,
    const std::vector<uint64_t>& path_topology,
    bool is_aspect_preserving_scaling,
    bool has_overscan,
    PrivacyScreenState privacy_screen_state,
    bool has_content_protection_key,
    bool has_color_correction_matrix,
    bool color_correction_in_linear_space,
    std::string display_name,
    DisplayModeList modes,
    const DisplayMode* current_mode,
    const DisplayMode* native_mode,
    int64_t product_code,
    const gfx::Size& maximum_cursor_size,
    const gfx::ColorSpace& color_space,
    uint32_t bits_per_channel,
    const gfx::HDRStaticMetadata& hdr_static_metadata,
    VariableRefreshRateState variable_refresh_rate_state,
    const absl::optional<uint16_t>& vsync_rate_min,
    const DrmFormatsAndModifiers& drm_formats_and_modifiers)
    : DisplaySnapshot(display_id,
                      port_display_id,
                      edid_display_id,
                      connector_index,
                      origin,
                      physical_size,
                      type,
                      base_connector_id,
                      path_topology,
                      is_aspect_preserving_scaling,
                      has_overscan,
                      privacy_screen_state,
                      has_content_protection_key,
                      has_color_correction_matrix,
                      color_correction_in_linear_space,
                      color_space,
                      bits_per_channel,
                      hdr_static_metadata,
                      display_name,
                      base::FilePath(),
                      std::move(modes),
                      display::PanelOrientation::kNormal,
                      std::vector<uint8_t>(),
                      current_mode,
                      native_mode,
                      product_code,
                      2018 /*year_of_manufacture */,
                      maximum_cursor_size,
                      variable_refresh_rate_state,
                      vsync_rate_min,
                      drm_formats_and_modifiers) {}

FakeDisplaySnapshot::~FakeDisplaySnapshot() {}

// static
std::unique_ptr<DisplaySnapshot> FakeDisplaySnapshot::CreateFromSpec(
    int64_t id,
    const std::string& spec) {
  StringPiece leftover(spec);

  // Cut off end of string at each delimiter to split.
  StringPiece options = ExtractSuffix(&leftover, "/");
  StringPiece dpi = ExtractSuffix(&leftover, "^");
  StringPiece resolutions = ExtractSuffix(&leftover, "#");

  // Leftovers should be just the native mode at this point.
  std::unique_ptr<DisplayMode> native_mode =
      ParseDisplayMode(std::string(leftover));

  // Fail without valid native mode.
  if (!native_mode)
    return nullptr;

  FakeDisplaySnapshot::Builder builder;
  builder.SetId(id).SetNativeMode(std::move(native_mode));

  if (!HandleModes(&builder, resolutions) || !HandleDPI(&builder, dpi) ||
      !HandleOptions(&builder, options)) {
    return nullptr;
  }

  return builder.Build();
}

}  // namespace display
