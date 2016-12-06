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
#include "cobalt/loader/image/image_decoder_starboard.h"
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
#if defined(STARBOARD)
#include "starboard/image.h"
#endif

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

// The various types of images we support decoding.
enum ImageType {
  kImageTypeInvalid,
  kImageTypeGIF,
  kImageTypeJPEG,
  kImageTypePNG,
  kImageTypeWebP,
};

// Determine the ImageType of an image from its signature.
ImageType DetermineImageType(const uint8* header) {
  if (!memcmp(header, "\xFF\xD8\xFF", 3)) {
    return kImageTypeJPEG;
  } else if (!memcmp(header, "GIF87a", 6) || !memcmp(header, "GIF89a", 6)) {
    return kImageTypeGIF;
  } else if (!memcmp(header, "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A", 8)) {
    return kImageTypePNG;
  } else if (!memcmp(header, "RIFF", 4) && !memcmp(header + 8, "WEBPVP", 6)) {
    return kImageTypeWebP;
  } else {
    return kImageTypeInvalid;
  }
}

#if defined(STARBOARD)
#if SB_VERSION(3) && SB_HAS(GRAPHICS)
// clang-format off
SbDecodeTargetFormat kPreferredFormats[] = {
    kSbDecodeTargetFormat1PlaneRGBA,
    kSbDecodeTargetFormat1PlaneBGRA,
};
// clang-format on
#endif
#endif

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
  TRACE_EVENT0("cobalt::loader::image", "ImageDecoder::ImageDecoder()");
  signature_cache_.position = 0;

  if (!resource_provider_) {
    state_ = kNoResourceProvider;
  }
}

LoadResponseType ImageDecoder::OnResponseStarted(
    Fetcher* fetcher, const scoped_refptr<net::HttpResponseHeaders>& headers) {
  UNREFERENCED_PARAMETER(fetcher);
  TRACE_EVENT0("cobalt::loader::image", "ImageDecoder::OnResponseStarted()");

  if (state_ == kSuspended) {
    DLOG(WARNING) << __FUNCTION__ << "[" << this << "] while suspended.";
    return kLoadResponseContinue;
  }

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

  bool success = headers->GetMimeType(&mime_type_);
  if (!success || !net::IsSupportedImageMimeType(mime_type_)) {
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
#if defined(STARBOARD)
#if SB_VERSION(3) && SB_HAS(GRAPHICS)
        SbDecodeTarget target = decoder_->RetrieveSbDecodeTarget();
        if (SbDecodeTargetIsValid(target)) {
          success_callback_.Run(
              resource_provider_->CreateImageFromSbDecodeTarget(target));
        } else  // NOLINT
#endif
#endif
        {
          scoped_ptr<render_tree::ImageData> image_data =
              decoder_->RetrieveImageData();
          success_callback_.Run(
              image_data ? resource_provider_->CreateImage(image_data.Pass())
                         : NULL);
        }
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
    case kSuspended:
      DLOG(WARNING) << __FUNCTION__ << "[" << this << "] while suspended.";
      break;
    case kNotApplicable:
      // no image is available.
      failure_callback_.Run(failure_message_);
      break;
  }
}

bool ImageDecoder::Suspend() {
  TRACE_EVENT0("cobalt::loader::image", "ImageDecoder::Suspend()");
  if (state_ == kDecoding) {
    DCHECK(decoder_);
    decoder_.reset();
  }
  state_ = kSuspended;
  signature_cache_.position = 0;
  resource_provider_ = NULL;
  return true;
}

void ImageDecoder::Resume(render_tree::ResourceProvider* resource_provider) {
  TRACE_EVENT0("cobalt::loader::image", "ImageDecoder::Resume()");
  if (state_ != kSuspended) {
    DCHECK_EQ(resource_provider_, resource_provider);
    return;
  }

  state_ = kWaitingForHeader;
  resource_provider_ = resource_provider;

  if (!resource_provider_) {
    state_ = kNoResourceProvider;
  }
}

void ImageDecoder::DecodeChunkInternal(const uint8* input_bytes, size_t size) {
  TRACE_EVENT0("cobalt::loader::image", "ImageDecoder::DecodeChunkInternal()");
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
    case kSuspended:
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
  TRACE_EVENT0("cobalt::loader::image",
               "ImageDecoder::InitializeInternalDecoder()");
  const size_t index = signature_cache_.position;
  size_t fill_size = std::min(kLengthOfLongestSignature - index, size);
  memcpy(signature_cache_.data + index, input_bytes, fill_size);
  signature_cache_.position += fill_size;
  *consumed_size = fill_size;
  if (signature_cache_.position < kLengthOfLongestSignature) {
    // Data is not enough for matching signature.
    return false;
  }

  const ImageType image_type = DetermineImageType(signature_cache_.data);

#if defined(STARBOARD)
#if SB_VERSION(3) && SB_HAS(GRAPHICS)
  const char* mime_type_c_string = mime_type_.c_str();

  // Find out if any of our preferred formats are supported for this mime
  // type.
  SbDecodeTargetFormat format = kSbDecodeTargetFormatInvalid;
  for (size_t i = 0; i < SB_ARRAY_SIZE(kPreferredFormats); ++i) {
    if (SbImageIsDecodeSupported(mime_type_c_string, kPreferredFormats[i])) {
      format = kPreferredFormats[i];
      break;
    }
  }

  if (SbDecodeTargetIsFormatValid(format) &&
      resource_provider_->SupportsSbDecodeTarget()) {
    decoder_ = make_scoped_ptr<ImageDataDecoder>(new ImageDecoderStarboard(
        resource_provider_, mime_type_c_string, format));
    return true;
  }
#endif  // SB_VERSION(3) && SB_HAS(GRAPHICS)
#endif  // defined(STARBOARD)

  // Call different types of decoders by matching the image signature.
  if (s_use_stub_image_decoder) {
    decoder_ = make_scoped_ptr<ImageDataDecoder>(
        new StubImageDecoder(resource_provider_));
  } else if (image_type == kImageTypeJPEG) {
#if defined(__LB_PS3__)
    decoder_ = make_scoped_ptr<ImageDataDecoder>(
        new JPEGImageDecoderPS3(resource_provider_));
#else   // defined(__LB_PS3__)
    decoder_ = make_scoped_ptr<ImageDataDecoder>(
        new JPEGImageDecoder(resource_provider_));
#endif  // defined(__LB_PS3__)
  } else if (image_type == kImageTypePNG) {
    decoder_ = make_scoped_ptr<ImageDataDecoder>(
        new PNGImageDecoder(resource_provider_));
  } else if (image_type == kImageTypeWebP) {
    decoder_ = make_scoped_ptr<ImageDataDecoder>(
        new WEBPImageDecoder(resource_provider_));
  } else if (image_type == kImageTypeGIF) {
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
