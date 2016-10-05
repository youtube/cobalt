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
#include "net/base/mime_util.h"
#include "net/http/http_status_code.h"

namespace cobalt {
namespace loader {
namespace image {

namespace {

bool s_use_stub_image_decoder = false;

void CacheMessage(std::string* result, const std::string& message) {
  DCHECK(result);

  if (!result->empty()) {
    result->append(" ");
  }

  result->append(message);
}

}  // namespace

ImageDecoder::ImageDecoder(render_tree::ResourceProvider* resource_provider,
                           const SuccessCallback& success_callback,
                           const FailureCallback& failure_callback,
                           const ErrorCallback& error_callback)
    : resource_provider_(resource_provider),
      success_callback_(success_callback),
      failure_callback_(failure_callback),
      error_callback_(error_callback),
      state_(kWaitingForHeader) {
  signature_cache_.position = 0;

  if (!resource_provider_) {
    state_ = kNoResourceProvider;
  }
}

LoadResponseType ImageDecoder::OnResponseStarted(
    Fetcher* fetcher, const scoped_refptr<net::HttpResponseHeaders>& headers) {
  UNREFERENCED_PARAMETER(fetcher);

  if (headers->response_code() == net::HTTP_OK &&
      headers->GetContentLength() == 0) {
    // The server successfully processed the request and expected some contents,
    // but it is not returning any content.
    state_ = kNotApplicable;
    CacheMessage(&failure_message_, "No content returned, but expected some.");
  }

  if (headers->response_code() == net::HTTP_NO_CONTENT) {
    // The server successfully processed the request, but is not returning any
    // content.
    state_ = kNotApplicable;
    CacheMessage(&failure_message_, "No content returned.");
  }

  std::string mime_style;
  bool success = headers->GetMimeType(&mime_style);
  if (!success || !net::IsSupportedImageMimeType(mime_style)) {
    state_ = kNotApplicable;
    CacheMessage(&failure_message_, "Not an image mime type.");
  }

  return kLoadResponseContinue;
}

void ImageDecoder::DecodeChunk(const char* data, size_t size) {
  TRACE_EVENT1("cobalt::loader::image_decoder", "ImageDecoder::DecodeChunk",
               "size", size);
  if (size == 0) {
    DLOG(WARNING) << "Decoder received 0 bytes.";
    return;
  }

  DecodeChunkInternal(reinterpret_cast<const uint8*>(data), size);
}

void ImageDecoder::Finish() {
  TRACE_EVENT0("cobalt::loader::image_decoder", "ImageDecoder::Finish");
  switch (state_) {
    case kDecoding:
      DCHECK(decoder_);
      if (decoder_->FinishWithSuccess()) {
        scoped_ptr<render_tree::ImageData> image_data =
            decoder_->RetrieveImageData();
        success_callback_.Run(
            image_data ? resource_provider_->CreateImage(image_data.Pass())
                       : NULL);
      } else {
        error_callback_.Run(decoder_->GetTypeString() +
                            " failed to decode image.");
      }
      break;
    case kWaitingForHeader:
      if (signature_cache_.position == 0) {
        // no image is available.
        failure_callback_.Run(failure_message_);
      } else {
        error_callback_.Run("No enough image data for header.");
      }
      break;
    case kUnsupportedImageFormat:
      error_callback_.Run("Unsupported image format.");
      break;
    case kNoResourceProvider:
      failure_callback_.Run("No resource provider was passed to the decoder.");
      break;
    case kAborted:
      error_callback_.Run("ImageDecoder received an external signal to abort.");
      break;
    case kNotApplicable:
      // no image is available.
      failure_callback_.Run(failure_message_);
      break;
  }
}

void ImageDecoder::Abort() {
  if (state_ == kDecoding) {
    DCHECK(decoder_);
    decoder_.reset();
    state_ = kAborted;
  } else if (state_ == kWaitingForHeader) {
    state_ = kAborted;
  }
}

void ImageDecoder::DecodeChunkInternal(const uint8* input_bytes, size_t size) {
  switch (state_) {
    case kWaitingForHeader: {
      size_t consumed_size = 0;
      if (InitializeInternalDecoder(input_bytes, size, &consumed_size)) {
        state_ = kDecoding;
        DCHECK(decoder_);
        if (consumed_size == kLengthOfLongestSignature) {
          // This case means the first chunk is large enough for matching
          // signature.
          decoder_->DecodeChunk(input_bytes, size);
        } else {
          decoder_->DecodeChunk(signature_cache_.data,
                                kLengthOfLongestSignature);
          input_bytes += consumed_size;
          decoder_->DecodeChunk(input_bytes, size - consumed_size);
        }
      }
    } break;
    case kDecoding: {
      DCHECK(decoder_);
      decoder_->DecodeChunk(input_bytes, size);
    } break;
    case kNotApplicable:
    case kUnsupportedImageFormat:
    case kAborted:
    case kNoResourceProvider:
    default: {
      // Do not attempt to continue processing data.
      DCHECK(!decoder_);
    } break;
  }
}

bool ImageDecoder::InitializeInternalDecoder(const uint8* input_bytes,
                                             size_t size,
                                             size_t* consumed_size) {
  const size_t index = signature_cache_.position;
  size_t fill_size = std::min(kLengthOfLongestSignature - index, size);
  memcpy(signature_cache_.data + index, input_bytes, fill_size);
  signature_cache_.position += fill_size;
  *consumed_size = fill_size;
  if (signature_cache_.position < kLengthOfLongestSignature) {
    // Data is not enough for matching signature.
    return false;
  }

  // Call different types of decoders by matching the image signature.
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
    state_ = kUnsupportedImageFormat;
    return false;
  }

  return true;
}

// static
void ImageDecoder::UseStubImageDecoder() { s_use_stub_image_decoder = true; }

}  // namespace image
}  // namespace loader
}  // namespace cobalt
