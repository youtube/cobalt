// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "base/debug/trace_event.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/time.h"
#include "cobalt/base/wrap_main.h"
#include "cobalt/loader/image/image_decoder.h"
#include "cobalt/math/size.h"
#include "cobalt/renderer/renderer_module.h"
#include "cobalt/system_window/system_window.h"
#include "cobalt/trace_event/scoped_trace_to_file.h"

namespace cobalt {
namespace loader {
namespace image {
namespace sandbox {
namespace {
const int kViewportWidth = 1920;
const int kViewportHeight = 1080;

using renderer::RendererModule;
using render_tree::ResourceProvider;
using system_window::SystemWindow;
using file_util::FileEnumerator;

struct ImageDecoderCallback {
  void SuccessCallback(const scoped_refptr<loader::image::Image>& value) {
    image = value;
  }

  void ErrorCallback(const std::string& error_message) {
    LOG(ERROR) << error_message;
  }

  scoped_refptr<loader::image::Image> image;
};

std::vector<FilePath> GetImagePaths(const char* extention) {
  FilePath image_path;
  CHECK(PathService::Get(base::DIR_TEST_DATA, &image_path));
  image_path = image_path.Append(FILE_PATH_LITERAL("cobalt"))
                   .Append(FILE_PATH_LITERAL("loader"))
                   .Append(FILE_PATH_LITERAL("testdata"));

  std::vector<FilePath> result;
  FileEnumerator file_enumerator(image_path, false /* Not recursive */,
                                 FileEnumerator::FILES);
  for (FilePath next = file_enumerator.Next(); !next.empty();
       next = file_enumerator.Next()) {
    if (next.Extension() == extention) {
      result.push_back(next);
    }
  }
  return result;
}

std::vector<uint8> GetFileContent(const FilePath& file_path) {
  int64 size;
  std::vector<uint8> data;

  bool success = file_util::GetFileSize(file_path, &size);

  CHECK(success) << "Could not get file size.";
  CHECK_GT(size, 0);

  data.resize(static_cast<size_t>(size));

  int num_of_bytes = file_util::ReadFile(
      file_path, reinterpret_cast<char*>(&data[0]), static_cast<int>(size));

  CHECK_EQ(num_of_bytes, data.size()) << "Could not read '" << file_path.value()
                                      << "'.";
  return data;
}

void DecodeImages(ResourceProvider* resource_provider, const char* extension) {
  std::vector<FilePath> paths = GetImagePaths(extension);
  base::TimeDelta total_time;
  size_t total_size = 0;

  for (size_t i = 0; i < paths.size(); ++i) {
    ImageDecoderCallback image_decoder_result;
    std::vector<uint8> image_data = GetFileContent(paths[i]);

    base::Time start = base::Time::Now();
    scoped_ptr<Decoder> image_decoder(new ImageDecoder(
        resource_provider, base::Bind(&ImageDecoderCallback::SuccessCallback,
                                      base::Unretained(&image_decoder_result)),
        base::Bind(&ImageDecoderCallback::ErrorCallback,
                   base::Unretained(&image_decoder_result))));

    image_decoder->DecodeChunk(reinterpret_cast<char*>(&image_data[0]),
                               image_data.size());
    image_decoder->Finish();
    if (image_decoder_result.image) {
      base::TimeDelta decoding_time = base::Time::Now() - start;
      total_time += decoding_time;
      total_size += image_decoder_result.image->GetEstimatedSizeInBytes();
      LOG(INFO) << "Decoding " << paths[i].BaseName().value() << " takes "
                << decoding_time.InMicroseconds() << " microseconds";
    }
  }
  if (total_time.InMicroseconds() != 0) {
    LOG(INFO) << "Average decoding speed for type \"" << extension << "\" is "
              << total_size * base::Time::kMicrosecondsPerSecond /
                     total_time.InMicroseconds()
              << " byte per seconds";
  }
}

int SandboxMain(int argc, char** argv) {
  cobalt::trace_event::ScopedTraceToFile trace_to_file(
      FilePath(FILE_PATH_LITERAL("image_decoder_sandbox_trace.json")));

  base::EventDispatcher event_dispatcher;
  // Create a system window to use as a render target.
  scoped_ptr<SystemWindow> system_window(
      new cobalt::system_window::SystemWindow(
          &event_dispatcher,
          cobalt::math::Size(kViewportWidth, kViewportHeight)));

  // Construct a renderer module using default options.
  RendererModule::Options renderer_module_options;
  RendererModule renderer_module(system_window.get(), renderer_module_options);

  // Try to decode all images in the given folder.
  ResourceProvider* resource_provider =
      renderer_module.pipeline()->GetResourceProvider();
  DecodeImages(resource_provider, ".jpg");
  DecodeImages(resource_provider, ".png");
  DecodeImages(resource_provider, ".webp");

  return 0;
}

}  // namespace
}  // namespace sandbox
}  // namespace image
}  // namespace loader
}  // namespace cobalt

COBALT_WRAP_SIMPLE_MAIN(cobalt::loader::image::sandbox::SandboxMain);
