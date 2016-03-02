/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/loader/image/image_decoder.h"

#include <algorithm>

#include "base/debug/trace_event.h"
#include "cobalt/loader/image/dummy_gif_image_decoder.h"
#if defined(__LB_PS3__)
#include "cobalt/loader/image/jpeg_image_decoder_ps3.h"
#else  // defined(__LB_PS3__)
#include "cobalt/loader/image/jpeg_image_decoder.h"
#endif  // defined(__LB_PS3__)
#include "cobalt/loader/image/png_image_decoder.h"
#include "cobalt/loader/image/stub_image_decoder.h"
#include "cobalt/loader/image/webp_image_decoder.h"
#include "net/http/http_status_code.h"

namespace cobalt {
namespace loader {
namespace image {

namespace {

bool s_use_stub_image_decoder = false;

}  // namespace

ImageDecoder::ImageDecoder(render_tree::ResourceProvider* resource_provider,
                           const SuccessCallback& success_callback,
                           const ErrorCallback& error_callback)
    : resource_provider_(resource_provider),
      success_callback_(success_callback),
      error_callback_(error_callback),
      error_state_(kNoError) {
  signature_cache_.position = 0;

  DCHECK(resource_provider_);
}

LoadResponseType ImageDecoder::OnResponseStarted(
    Fetcher* fetcher, const scoped_refptr<net::HttpResponseHeaders>& headers) {
  UNREFERENCED_PARAMETER(fetcher);
  if (headers->response_code() == net::HTTP_NO_CONTENT) {
    // The server successfully processed the request, but is not returning any
    // content.
    error_state_ = kNoContent;
  }

  return kLoadResponseContinue;
}

void ImageDecoder::DecodeChunk(const char* data, size_t size) {
  TRACE_EVENT1("cobalt::loader::image_decoder", "ImageDecoder::DecodeChunk",
               "size", size);
  if (error_state_ != kNoError) {
    // Do not attempt to continue processing data if we are in an error state.
    DCHECK(!decoder_);
    return;
  }

  if (size == 0) {
    DLOG(WARNING) << "Decoder received 0 bytes.";
    return;
  }

  const uint8* input_bytes = reinterpret_cast<const uint8*>(data);
  // Call different types of decoders by matching the image signature. If it is
  // the first time decoding a chunk, create an internal specific image decoder,
  // otherwise just call DecodeChunk.
  if (decoder_) {
    decoder_->DecodeChunk(input_bytes, size);
    return;
  }

  const size_t index = signature_cache_.position;
  size_t data_offset = kLengthOfLongestSignature - index;
  size_t fill_size = std::min(data_offset, size);
  memcpy(signature_cache_.data + index, input_bytes, fill_size);
  signature_cache_.position += fill_size;
  if (size < data_offset) {
    // Data is not enough for matching signature.
    return;
  }

  if (s_use_stub_image_decoder) {
    decoder_ = make_scoped_ptr<ImageDataDecoder>(
        new StubImageDecoder(resource_provider_));
#if defined(__LB_PS3__)
  } else if (JPEGImageDecoderPS3::IsValidSignature(signature_cache_.data)) {
    decoder_ = make_scoped_ptr<ImageDataDecoder>(
        new JPEGImageDecoderPS3(resource_provider_));
#else   // defined(__LB_PS3__)
  } else if (JPEGImageDecoder::IsValidSignature(signature_cache_.data)) {
    decoder_ = make_scoped_ptr<ImageDataDecoder>(
        new JPEGImageDecoder(resource_provider_));
#endif  // defined(__LB_PS3__)
  } else if (PNGImageDecoder::IsValidSignature(signature_cache_.data)) {
    decoder_ = make_scoped_ptr<ImageDataDecoder>(
        new PNGImageDecoder(resource_provider_));
  } else if (WEBPImageDecoder::IsValidSignature(signature_cache_.data)) {
    decoder_ = make_scoped_ptr<ImageDataDecoder>(
        new WEBPImageDecoder(resource_provider_));
  } else if (DummyGIFImageDecoder::IsValidSignature(signature_cache_.data)) {
    decoder_ = make_scoped_ptr<ImageDataDecoder>(
        new DummyGIFImageDecoder(resource_provider_));
  } else {
    error_state_ = kUnsupportedImageFormat;
    return;
  }

  if (index == 0) {
    // This case means the first chunk is large enough for matching signature.
    decoder_->DecodeChunk(input_bytes, size);
  } else {
    decoder_->DecodeChunk(signature_cache_.data, kLengthOfLongestSignature);
    input_bytes += data_offset;
    decoder_->DecodeChunk(input_bytes, size - data_offset);
  }
}

void ImageDecoder::Finish() {
  TRACE_EVENT0("cobalt::loader::image_decoder", "ImageDecoder::Finish");
  switch (error_state_) {
    case kNoError:
      if (decoder_ && decoder_->FinishWithSuccess()) {
        scoped_ptr<render_tree::ImageData> image_data =
            decoder_->RetrieveImageData();
        success_callback_.Run(
            image_data ? resource_provider_->CreateImage(image_data.Pass())
                       : NULL);
      } else {
        error_callback_.Run("Decode image failed.");
      }
      break;
    case kNoContent:
      // Load successful and no image is available if no content.
      success_callback_.Run(NULL);
      break;
    case kUnsupportedImageFormat:
      error_callback_.Run("Unsupported image format");
      break;
  }
}

// static
void ImageDecoder::UseStubImageDecoder() { s_use_stub_image_decoder = true; }

}  // namespace image
}  // namespace loader
}  // namespace cobalt
