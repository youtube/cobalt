// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <cstdint>
#include <optional>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/process/memory.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "cc/paint/paint_cache.h"
#include "cc/paint/paint_op_buffer.h"
#include "cc/paint/paint_op_reader.h"
#include "cc/paint/paint_op_writer.h"
#include "cc/test/transfer_cache_test_helper.h"
#include "gpu/command_buffer/common/buffer.h"
#include "gpu/command_buffer/service/service_font_manager.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"
#include "testing/libfuzzer/libfuzzer_exports.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GpuTypes.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/SkSurfaceGanesh.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLInterface.h"
#include "third_party/skia/include/gpu/ganesh/mock/GrMockTypes.h"

struct Environment {
  Environment() {
    static constexpr char kDumpSKPSwitch[] = "dump-skp";
    base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
    if (cl->HasSwitch(kDumpSKPSwitch)) {
      dump_skp = cl->GetSwitchValuePath(kDumpSKPSwitch);
    } else {
      // Disable noisy logging as per "libFuzzer in Chrome" documentation:
      // testing/libfuzzer/getting_started.md#Disable-noisy-error-message-logging.
      logging::SetMinLogLevel(logging::LOGGING_FATAL);
    }

    base::EnableTerminationOnOutOfMemory();
    base::DiscardableMemoryAllocator::SetInstance(
        &discardable_memory_allocator);
  }

  ~Environment() { base::DiscardableMemoryAllocator::SetInstance(nullptr); }

  const std::optional<base::FilePath>& DumpSKP() const { return dump_skp; }

 private:
  base::TestDiscardableMemoryAllocator discardable_memory_allocator;
  std::optional<base::FilePath> dump_skp;
};

class FontSupport : public gpu::ServiceFontManager::Client {
 public:
  FontSupport() = default;
  ~FontSupport() override = default;

  // gpu::ServiceFontManager::Client implementation.
  scoped_refptr<gpu::Buffer> GetShmBuffer(uint32_t shm_id) override {
    auto it = buffers_.find(shm_id);
    if (it != buffers_.end()) {
      return it->second;
    }
    return CreateBuffer(shm_id);
  }
  void ReportProgress() override {}

 private:
  scoped_refptr<gpu::Buffer> CreateBuffer(uint32_t shm_id) {
    static const size_t kBufferSize = 2048u;
    base::UnsafeSharedMemoryRegion shared_memory =
        base::UnsafeSharedMemoryRegion::Create(kBufferSize);
    base::WritableSharedMemoryMapping mapping = shared_memory.Map();
    auto buffer = gpu::MakeBufferFromSharedMemory(std::move(shared_memory),
                                                  std::move(mapping));
    buffers_[shm_id] = buffer;
    return buffer;
  }

  base::flat_map<uint32_t, scoped_refptr<gpu::Buffer>> buffers_;
};

void Raster(SkCanvas* canvas,
            SkStrikeClient* strike_client,
            cc::ServicePaintCache* paint_cache,
            base::span<const uint8_t> input) {
  cc::PlaybackParams params(nullptr, canvas->getLocalToDevice());
  cc::TransferCacheTestHelper transfer_cache_helper;
  std::vector<uint8_t> scratch_buffer;
  cc::PaintOp::DeserializeOptions deserialize_options{
      .transfer_cache = &transfer_cache_helper,
      .paint_cache = paint_cache,
      .strike_client = strike_client,
      .scratch_buffer = scratch_buffer,
      .is_privileged = true};

  // Need kHeaderBytes bytes to be able to read the header.
  while (input.size() >= cc::PaintOpWriter::kHeaderBytes) {
    std::unique_ptr<char, base::AlignedFreeDeleter> deserialized(
        static_cast<char*>(base::AlignedAlloc(
            sizeof(cc::LargestPaintOp), cc::PaintOpBuffer::kPaintOpAlign)));
    size_t bytes_read = 0;
    cc::PaintOp* deserialized_op = cc::PaintOp::Deserialize(
        input, deserialized.get(), sizeof(cc::LargestPaintOp), &bytes_read,
        deserialize_options);

    if (!deserialized_op) {
      break;
    }

    deserialized_op->Raster(canvas, params);

    deserialized_op->DestroyThis();

    input = input.subspan(bytes_read);
  }
}

void Raster(GrDirectContext* gr_context,
            SkStrikeClient* strike_client,
            cc::ServicePaintCache* paint_cache,
            base::span<const uint8_t> input) {
  const size_t kRasterDimension = 32;

  SkImageInfo image_info = SkImageInfo::MakeN32(
      kRasterDimension, kRasterDimension, kOpaque_SkAlphaType);
  sk_sp<SkSurface> surface =
      SkSurfaces::RenderTarget(gr_context, skgpu::Budgeted::kYes, image_info);

  Raster(surface->getCanvas(), strike_client, paint_cache, input);
}

bool DumpSKP(SkStrikeClient* strike_client,
             cc::ServicePaintCache* paint_cache,
             base::span<const uint8_t> input,
             const Environment& env) {
  if (!env.DumpSKP()) {
    return false;
  }

  SkFILEWStream wstream(env.DumpSKP()->AsUTF8Unsafe().c_str());
  if (!wstream.isValid()) {
    LOG(ERROR) << "Invalid --dump-skp output path.";
    return true;
  }

  SkPictureRecorder recorder;
  SkCanvas* canvas = recorder.beginRecording(32, 32);
  Raster(canvas, strike_client, paint_cache, input);
  recorder.finishRecordingAsPicture()->serialize(&wstream);

  return true;
}

struct FontsAndRasterData {
  // These spans have the same lifetime as the input from libFuzzer.
  // There's no need to make them `base::raw_span`.
  RAW_PTR_EXCLUSION base::span<const uint8_t> fonts;
  RAW_PTR_EXCLUSION base::span<const uint8_t> raster_data;
};

std::optional<FontsAndRasterData> PartitionInputData(
    base::span<const uint8_t> data) {
  if (data.size() <= sizeof(size_t)) {
    return std::nullopt;
  }

  size_t bytes_for_fonts = data[0];
  if (bytes_for_fonts > data.size()) {
    bytes_for_fonts = data.size() / 2;
  }

  // This can result in 0 bytes being partitioned for fonts.
  const size_t raster_data_offset =
      base::bits::AlignDown(bytes_for_fonts, cc::PaintOpWriter::kMaxAlignment);
  const auto [fonts, raster_data] = data.split_at(raster_data_offset);
  return FontsAndRasterData{
      .fonts = fonts,
      .raster_data = raster_data,
  };
}

// Deserialize an arbitrary number of cc::PaintOps and raster them
// using gpu raster into an SkCanvas.
DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(base::span<const uint8_t> data) {
  std::optional<FontsAndRasterData> partitioned = PartitionInputData(data);
  if (!partitioned.has_value()) {
    return 0;
  }

  static Environment env;

  FontSupport font_support;
  scoped_refptr<gpu::ServiceFontManager> font_manager(
      new gpu::ServiceFontManager(&font_support,
                                  false /* disable_oopr_debug_crash_dump */));
  cc::ServicePaintCache paint_cache;
  std::vector<SkDiscardableHandleId> locked_handles;
  if (partitioned->fonts.size() > 0u) {
    font_manager->Deserialize(partitioned->fonts, &locked_handles);
  }

  if (DumpSKP(font_manager->strike_client(), &paint_cache,
              partitioned->raster_data, env)) {
    return 0;
  }

  GrMockOptions options_no_support;
  options_no_support.fShaderDerivativeSupport = false;
  auto gr_context_no_support = GrDirectContext::MakeMock(&options_no_support);

  CHECK(!!gr_context_no_support);
  CHECK(!gr_context_no_support->supportsDistanceFieldText());
  Raster(gr_context_no_support.get(), font_manager->strike_client(),
         &paint_cache, partitioned->raster_data);

  GrMockOptions options_with_support;
  options_with_support.fShaderDerivativeSupport = true;
  auto gr_context_with_support =
      GrDirectContext::MakeMock(&options_with_support);

  CHECK(!!gr_context_with_support);
  CHECK(gr_context_with_support->supportsDistanceFieldText());
  Raster(gr_context_with_support.get(), font_manager->strike_client(),
         &paint_cache, partitioned->raster_data);

  font_manager->Unlock(locked_handles);
  font_manager->Destroy();
  return 0;
}

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv) {
  base::CommandLine::Init(*argc, *argv);
  return 0;
}
