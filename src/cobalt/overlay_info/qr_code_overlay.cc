// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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
#include <string>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/overlay_info/overlay_info_registry.h"
#include "cobalt/render_tree/animations/animate_node.h"
#include "starboard/memory.h"
#include "third_party/QR-Code-generator/cpp/QrCode.hpp"

namespace cobalt {
namespace overlay_info {

namespace {

using qrcodegen::QrCode;
using render_tree::Image;
using render_tree::ImageNode;
using render_tree::ResourceProvider;

const int kMinimumQrCodeVersion = 3;
const int kPixelSizeInBytes = 4;
#if SB_IS_BIG_ENDIAN
const uint32_t kBlack = 0x000000FF;
#else   // SB_IS_BIG_ENDIAN
const uint32_t kBlack = 0xFF000000;
#endif  // SB_IS_BIG_ENDIAN
const uint32_t kWhite = 0xFFFFFFFF;
const uint32_t kBorderColor = kWhite;
const int kScreenMarginInPixels = 64;

uint32_t s_frame_index_ = 0;

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

void DrawQrCode(const QrCode& qr_code, int module_dimension_in_pixels,
                int pitch_in_bytes, uint8_t* target_buffer) {
  uint8_t* row_data = target_buffer;
  for (int row = 0; row < qr_code.getSize(); ++row) {
    uint8_t* column_data = row_data;
    for (int column = 0; column < qr_code.getSize(); ++column) {
      DrawRect(module_dimension_in_pixels, module_dimension_in_pixels,
               pitch_in_bytes, qr_code.getModule(row, column) ? kBlack : kWhite,
               column_data);
      column_data += kPixelSizeInBytes * module_dimension_in_pixels;
    }

    row_data += pitch_in_bytes * module_dimension_in_pixels;
  }
}

scoped_refptr<Image> CreateImageForQrCode(const QrCode& qr_code,
                                          const math::Size& screen_size,
                                          int slots,
                                          ResourceProvider* resource_provider) {
  TRACE_EVENT0("cobalt::overlay_info", "CreateImageForQrCode()");

  const int module_dimension_in_pixels = screen_size.height() > 1080 ? 16 : 8;
  const int code_border_in_pixels = module_dimension_in_pixels * 2;

  int qr_code_size_in_blocks = qr_code.getSize();

  int column = (screen_size.width() - kScreenMarginInPixels * 2 -
                code_border_in_pixels) /
               (qr_code_size_in_blocks * module_dimension_in_pixels +
                code_border_in_pixels);
  column = std::min(column, slots);
  int row = (slots + column - 1) / column;

  int image_width =
      column * qr_code_size_in_blocks * module_dimension_in_pixels +
      code_border_in_pixels * (column + 1);
  int image_height = row * qr_code_size_in_blocks * module_dimension_in_pixels +
                     code_border_in_pixels * (row + 1);

  auto image_data = resource_provider->AllocateImageData(
      math::Size(image_width, image_height), render_tree::kPixelFormatRGBA8,
      render_tree::kAlphaFormatOpaque);
  DCHECK(image_data);
  auto image_desc = image_data->GetDescriptor();

  uint32_t slot_index = 0;
  auto row_data = image_data->GetMemory();
  for (int i = 0; i < row; ++i) {
    // Draw the top border of all qr code blocks in the row.
    DrawRect(image_width, code_border_in_pixels, image_desc.pitch_in_bytes,
             kBorderColor, row_data);
    row_data += code_border_in_pixels * image_desc.pitch_in_bytes;
    auto column_data = row_data;

    for (int j = 0; j < column; ++j) {
      // Draw the left border.
      DrawRect(code_border_in_pixels,
               qr_code_size_in_blocks * module_dimension_in_pixels,
               image_desc.pitch_in_bytes, kBorderColor, column_data);
      column_data += code_border_in_pixels * kPixelSizeInBytes;
      if (slot_index == s_frame_index_ % slots) {
        // Draw qr code.
        DrawQrCode(qr_code, module_dimension_in_pixels,
                   image_desc.pitch_in_bytes, column_data);
      } else {
        DrawRect(qr_code_size_in_blocks * module_dimension_in_pixels,
                 qr_code_size_in_blocks * module_dimension_in_pixels,
                 image_desc.pitch_in_bytes, kBlack, column_data);
      }
      ++slot_index;
      column_data += qr_code_size_in_blocks * module_dimension_in_pixels *
                     kPixelSizeInBytes;
    }

    // Draw the right border of the row.
    DrawRect(code_border_in_pixels,
             qr_code_size_in_blocks * module_dimension_in_pixels,
             image_desc.pitch_in_bytes, kBorderColor, column_data);

    row_data += qr_code_size_in_blocks * module_dimension_in_pixels *
                image_desc.pitch_in_bytes;
  }

  // Draw the bottom border of all qr code.
  DrawRect(image_width, code_border_in_pixels, image_desc.pitch_in_bytes,
           kBorderColor, row_data);

  return resource_provider->CreateImage(std::move(image_data));
}

void AnimateCB(math::Size screen_size, int slots,
               ResourceProvider* resource_provider,
               ImageNode::Builder* image_node, base::TimeDelta time) {
  DCHECK(image_node);

  TRACE_EVENT0("cobalt::overlay_info", "AnimateCB()");

  OverlayInfoRegistry::Register("frame", s_frame_index_);
  ++s_frame_index_;

  std::string infos;
  OverlayInfoRegistry::RetrieveAndClear(&infos);

  if (infos.empty()) {
    image_node->source = NULL;
    return;
  }

  auto qrcode = QrCode::encodeText(infos.c_str(), QrCode::Ecc::LOW,
                                   kMinimumQrCodeVersion);
  image_node->source =
      CreateImageForQrCode(qrcode, screen_size, slots, resource_provider);
  auto image_size = image_node->source->GetSize();
  image_node->destination_rect = math::RectF(
      screen_size.width() - image_size.width() - kScreenMarginInPixels,
      screen_size.height() - image_size.height() - kScreenMarginInPixels,
      image_size.width(), image_size.height());
}

}  // namespace

QrCodeOverlay::QrCodeOverlay(
    const math::Size& screen_size, int slots,
    ResourceProvider* resource_provider,
    const RenderTreeProducedCB& render_tree_produced_cb)
    : render_tree_produced_cb_(render_tree_produced_cb),
      slots_(slots),
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

void QrCodeOverlay::SetResourceProvider(ResourceProvider* resource_provider) {
  resource_provider_ = resource_provider;
  UpdateRenderTree();
}

void QrCodeOverlay::UpdateRenderTree() {
  TRACE_EVENT0("cobalt::overlay_info", "QrCodeOverlay::UpdateRenderTree()");

  if (resource_provider_ == NULL) {
    return;
  }

  scoped_refptr<ImageNode> image_node = new ImageNode(nullptr);
  render_tree::animations::AnimateNode::Builder animate_node_builder;

  animate_node_builder.Add(image_node, base::Bind(AnimateCB, screen_size_,
                                                  slots_, resource_provider_));

  render_tree_produced_cb_.Run(new render_tree::animations::AnimateNode(
      animate_node_builder, image_node));
}

}  // namespace overlay_info
}  // namespace cobalt
