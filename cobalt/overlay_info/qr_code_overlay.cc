// Copyright 2018 Google Inc. All Rights Reserved.
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

#include "cobalt/overlay_info/qr_code_overlay.h"

#include <algorithm>
#include <vector>

#include "base/compiler_specific.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "cobalt/overlay_info/overlay_info_registry.h"
#include "cobalt/render_tree/animations/animate_node.h"
#include "third_party/QR-Code-generator/cpp/QrCode.hpp"

namespace cobalt {
namespace overlay_info {

namespace {

using qrcodegen::QrCode;
using render_tree::Image;
using render_tree::ImageNode;

const int kModuleDimensionInPixels = 4;
const int kPixelSizeInBytes = 4;
const uint32_t kBlack = 0x00000000;
const uint32_t kWhite = 0xFFFFFFFF;
const uint32_t kBorderColor = kWhite;
const int kCodeBorderInPixels = 16;
const int kScreenMarginInPixels = 128;

int64_t s_frame_count_ = 0;

void DrawRect(int width, int height, int pitch_in_bytes, uint32_t color,
              uint8_t* target_buffer) {
  while (height > 0) {
    uint32_t* pixels = reinterpret_cast<uint32_t*>(target_buffer);
    for (int i = 0; i < width; ++i) {
      pixels[i] = color;
    }
    target_buffer += pitch_in_bytes;
    --height;
  }
}

void DrawQrCode(const QrCode& qr_code, int pitch_in_bytes,
                uint8_t* target_buffer) {
  uint8_t* row_data = target_buffer;
  for (int row = 0; row < qr_code.getSize(); ++row) {
    uint8_t* column_data = row_data;
    for (int column = 0; column < qr_code.getSize(); ++column) {
      DrawRect(kModuleDimensionInPixels, kModuleDimensionInPixels,
               pitch_in_bytes, qr_code.getModule(row, column) ? kBlack : kWhite,
               column_data);
      column_data += kPixelSizeInBytes * kModuleDimensionInPixels;
    }

    row_data += pitch_in_bytes * kModuleDimensionInPixels;
  }
}

scoped_refptr<Image> CreateImageForQrCodes(
    const std::vector<QrCode>& qr_codes, const math::Size& screen_size,
    render_tree::ResourceProvider* resource_provider) {
  TRACE_EVENT0("cobalt::overlay_info", "CreateImageForQrCodes()");

  int max_code_size = 0;

  for (auto& qr_code : qr_codes) {
    max_code_size = std::max(max_code_size, qr_code.getSize());
  }

  int column =
      (screen_size.width() - kScreenMarginInPixels * 2 - kCodeBorderInPixels) /
      (max_code_size * kModuleDimensionInPixels + kCodeBorderInPixels);
  column = std::min(column, static_cast<int>(qr_codes.size()));
  int row = (static_cast<int>(qr_codes.size()) + column - 1) / column;

  int image_width = column * max_code_size * kModuleDimensionInPixels +
                    kCodeBorderInPixels * (column + 1);
  int image_height = row * max_code_size * kModuleDimensionInPixels +
                     kCodeBorderInPixels * (row + 1);

  auto image_data = resource_provider->AllocateImageData(
      math::Size(image_width, image_height), render_tree::kPixelFormatRGBA8,
      render_tree::kAlphaFormatOpaque);
  DCHECK(image_data);
  auto image_desc = image_data->GetDescriptor();

  size_t qr_code_index = 0;
  auto row_data = image_data->GetMemory();
  for (int i = 0; i < row; ++i) {
    // Draw the top border of all qr codes in the row.
    DrawRect(image_width, kCodeBorderInPixels, image_desc.pitch_in_bytes,
             kBorderColor, row_data);
    row_data += kCodeBorderInPixels * image_desc.pitch_in_bytes;
    auto column_data = row_data;

    for (int j = 0; j < column; ++j) {
      // Draw the left border.
      DrawRect(kCodeBorderInPixels, max_code_size * kModuleDimensionInPixels,
               image_desc.pitch_in_bytes, kBorderColor, column_data);
      column_data += kCodeBorderInPixels * kPixelSizeInBytes;
      if (qr_code_index < qr_codes.size()) {
        // Draw qr code.
        DrawQrCode(qr_codes[qr_code_index], image_desc.pitch_in_bytes,
                   column_data);
        ++qr_code_index;
      }
      column_data +=
          max_code_size * kModuleDimensionInPixels * kPixelSizeInBytes;
    }

    // Draw the right border of the row.
    DrawRect(kCodeBorderInPixels, max_code_size * kModuleDimensionInPixels,
             image_desc.pitch_in_bytes, kBorderColor, column_data);

    row_data +=
        max_code_size * kModuleDimensionInPixels * image_desc.pitch_in_bytes;
  }

  // Draw the bottom border of all qr code.
  DrawRect(image_width, kCodeBorderInPixels, image_desc.pitch_in_bytes,
           kBorderColor, row_data);

  return resource_provider->CreateImage(image_data.Pass());
}

void AnimateCB(math::Size screen_size,
               render_tree::ResourceProvider* resource_provider,
               render_tree::ImageNode::Builder* image_node,
               base::TimeDelta time) {
  UNREFERENCED_PARAMETER(time);
  DCHECK(image_node);

  TRACE_EVENT0("cobalt::overlay_info", "AnimateCB()");

  OverlayInfoRegistry::Register("overlay_info:frame_count", &s_frame_count_,
                                sizeof(s_frame_count_));
  ++s_frame_count_;

  std::vector<uint8_t> infos;
  OverlayInfoRegistry::RetrieveAndClear(&infos);

  if (infos.empty()) {
    image_node->source = NULL;
    return;
  }

  // Use a vector in case we decide to switch back to multiple qr codes.
  std::vector<QrCode> qr_codes;
  qr_codes.emplace_back(QrCode::encodeBinary(infos, QrCode::Ecc::LOW));

  image_node->source =
      CreateImageForQrCodes(qr_codes, screen_size, resource_provider);
  auto image_size = image_node->source->GetSize();
  // TODO: Move the QR code between draws to avoid tearing.
  image_node->destination_rect =
      math::RectF(kScreenMarginInPixels, kScreenMarginInPixels,
                  image_size.width(), image_size.height());
}

}  // namespace

QrCodeOverlay::QrCodeOverlay(
    const math::Size& screen_size,
    render_tree::ResourceProvider* resource_provider,
    const RenderTreeProducedCB& render_tree_produced_cb)
    : render_tree_produced_cb_(render_tree_produced_cb),
      screen_size_(screen_size),
      resource_provider_(resource_provider) {
  DCHECK_GT(screen_size.width(), 0);
  DCHECK_GT(screen_size.height(), 0);
  DCHECK(!render_tree_produced_cb_.is_null());

  UpdateRenderTree();
}

void QrCodeOverlay::SetSize(const math::Size& size) {
  DCHECK_GT(size.width(), 0);
  DCHECK_GT(size.height(), 0);

  screen_size_ = size;
  UpdateRenderTree();
}

void QrCodeOverlay::SetResourceProvider(
    render_tree::ResourceProvider* resource_provider) {
  resource_provider_ = resource_provider;
  UpdateRenderTree();
}

void QrCodeOverlay::UpdateRenderTree() {
  TRACE_EVENT0("cobalt::overlay_info", "QrCodeOverlay::UpdateRenderTree()");

  if (resource_provider_ == NULL) {
    return;
  }

  scoped_refptr<ImageNode> image_node = new ImageNode(NULL);
  render_tree::animations::AnimateNode::Builder animate_node_builder;

  animate_node_builder.Add(
      image_node, base::Bind(AnimateCB, screen_size_, resource_provider_));

  render_tree_produced_cb_.Run(new render_tree::animations::AnimateNode(
      animate_node_builder, image_node));
}

}  // namespace overlay_info
}  // namespace cobalt
