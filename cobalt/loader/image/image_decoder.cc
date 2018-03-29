// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/loader/image/image_decoder.h"

#include <algorithm>

#include "base/debug/trace_event.h"
#include "cobalt/loader/image/dummy_gif_image_decoder.h"
#include "cobalt/loader/image/image_decoder_starboard.h"
#include "cobalt/loader/image/jpeg_image_decoder.h"
#include "cobalt/loader/image/png_image_decoder.h"
#include "cobalt/loader/image/stub_image_decoder.h"
#include "cobalt/loader/image/webp_image_decoder.h"
#include "net/base/mime_util.h"
#include "net/http/http_status_code.h"
#include "starboard/image.h"

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

// Determine the ImageType of an image from its signature.
ImageDecoder::ImageType DetermineImageType(const uint8* header) {
  if (!memcmp(header, "\xFF\xD8\xFF", 3)) {
    return ImageDecoder::kImageTypeJPEG;
  } else if (!memcmp(header, "GIF87a", 6) || !memcmp(header, "GIF89a", 6)) {
    return ImageDecoder::kImageTypeGIF;
  } else if (!memcmp(header, "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A", 8)) {
    return ImageDecoder::kImageTypePNG;
  } else if (!memcmp(header, "RIFF", 4) && !memcmp(header + 8, "WEBPVP", 6)) {
    return ImageDecoder::kImageTypeWebP;
  } else {
    return ImageDecoder::kImageTypeInvalid;
  }
}

}  // namespace

ImageDecoder::ImageDecoder(render_tree::ResourceProvider* resource_provider,
                           const SuccessCallback& success_callback,
                           const ErrorCallback& error_callback)
    : resource_provider_(resource_provider),
      success_callback_(success_callback),
      error_callback_(error_callback),
      image_type_(kImageTypeInvalid),
      state_(resource_provider_ ? kWaitingForHeader : kSuspended),
      is_deletion_pending_(false) {
  signature_cache_.position = 0;
}

ImageDecoder::ImageDecoder(render_tree::ResourceProvider* resource_provider,
                           const SuccessCallback& success_callback,
                           const ErrorCallback& error_callback,
                           ImageType image_type)
    : resource_provider_(resource_provider),
      success_callback_(success_callback),
      error_callback_(error_callback),
      image_type_(image_type),
      state_(resource_provider_ ? kWaitingForHeader : kSuspended),
      is_deletion_pending_(false) {
  signature_cache_.position = 0;
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
    CacheMessage(&error_message_, "No content returned, but expected some.");
  }

  if (headers->response_code() == net::HTTP_NO_CONTENT) {
    // The server successfully processed the request, but is not returning any
    // content.
    state_ = kNotApplicable;
    CacheMessage(&error_message_, "No content returned.");
  }

  bool success = headers->GetMimeType(&mime_type_);
  if (!success || !net::IsSupportedImageMimeType(mime_type_)) {
    state_ = kNotApplicable;
    CacheMessage(&error_message_, "Not an image mime type.");
  }

  return kLoadResponseContinue;
}

void ImageDecoder::DecodeChunk(const char* data, size_t size) {
  TRACE_EVENT1("cobalt::loader::image_decoder", "ImageDecoder::DecodeChunk",
               "size", size);
  // If there's a deletion pending, then just clear out the decoder and return.
  // There's no point in doing any additional processing that'll get thrown
  // away without ever being used.
  if (base::subtle::Acquire_Load(&is_deletion_pending_)) {
    decoder_.reset();
    return;
  }

  if (size == 0) {
    DLOG(WARNING) << "Decoder received 0 bytes.";
    return;
  }

  DecodeChunkInternal(reinterpret_cast<const uint8*>(data), size);
}

void ImageDecoder::Finish() {
  TRACE_EVENT0("cobalt::loader::image_decoder", "ImageDecoder::Finish");
  // If there's a deletion pending, then just clear out the decoder and return.
  // There's no point in doing any additional processing that'll get thrown
  // away without ever being used.
  if (base::subtle::Acquire_Load(&is_deletion_pending_)) {
    decoder_.reset();
    return;
  }

  switch (state_) {
    case kDecoding:
      DCHECK(decoder_);
      if (decoder_->FinishWithSuccess()) {
        if (!decoder_->has_animation()) {
#if SB_HAS(GRAPHICS)
          SbDecodeTarget target = decoder_->RetrieveSbDecodeTarget();
          if (SbDecodeTargetIsValid(target)) {
            success_callback_.Run(new StaticImage(
                resource_provider_->CreateImageFromSbDecodeTarget(target)));
          } else  // NOLINT
#endif
          {
            scoped_ptr<render_tree::ImageData> image_data =
                decoder_->RetrieveImageData();
            success_callback_.Run(
                image_data ? new StaticImage(resource_provider_->CreateImage(
                                 image_data.Pass()))
                           : NULL);
          }
        } else {
          success_callback_.Run(decoder_->animated_image());
        }
      } else {
        error_callback_.Run(decoder_->GetTypeString() +
                            " failed to decode image.");
      }
      break;
    case kWaitingForHeader:
      if (signature_cache_.position == 0) {
        // no image is available.
        error_callback_.Run(error_message_);
      } else {
        error_callback_.Run("No enough image data for header.");
      }
      break;
    case kUnsupportedImageFormat:
      error_callback_.Run("Unsupported image format.");
      break;
    case kSuspended:
      DLOG(WARNING) << __FUNCTION__ << "[" << this << "] while suspended.";
      break;
    case kNotApplicable:
      // no image is available.
      error_callback_.Run(error_message_);
      break;
  }
}

bool ImageDecoder::Suspend() {
  TRACE_EVENT0("cobalt::loader::image", "ImageDecoder::Suspend()");
  DCHECK_NE(state_, kSuspended);
  DCHECK(resource_provider_);

  if (state_ == kDecoding) {
    DCHECK(decoder_ || base::subtle::Acquire_Load(&is_deletion_pending_));
    decoder_.reset();
  }
  state_ = kSuspended;
  signature_cache_.position = 0;
  resource_provider_ = NULL;
  return true;
}

void ImageDecoder::Resume(render_tree::ResourceProvider* resource_provider) {
  TRACE_EVENT0("cobalt::loader::image", "ImageDecoder::Resume()");
  DCHECK_EQ(state_, kSuspended);
  DCHECK(!resource_provider_);
  DCHECK(resource_provider);

  state_ = kWaitingForHeader;
  resource_provider_ = resource_provider;
}

void ImageDecoder::SetDeletionPending() {
  base::subtle::Acquire_Store(&is_deletion_pending_, true);
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
    case kSuspended: {
      // Do not attempt to continue processing data.
      DCHECK(!decoder_);
    } break;
  }
}

namespace {
#if SB_HAS(GRAPHICS)
const char* GetMimeTypeFromImageType(ImageDecoder::ImageType image_type) {
  switch (image_type) {
    case ImageDecoder::kImageTypeJPEG:
      return "image/jpeg";
    case ImageDecoder::kImageTypePNG:
      return "image/png";
    case ImageDecoder::kImageTypeGIF:
      return "image/gif";
    case ImageDecoder::kImageTypeWebP:
      return "image/webp";
    case ImageDecoder::kImageTypeInvalid:
      return NULL;
  }
  NOTREACHED();
  return NULL;
}

// If |mime_type| is empty, |image_type| will be used to deduce the mime type.
scoped_ptr<ImageDataDecoder> MaybeCreateStarboardDecoder(
    const std::string& mime_type, ImageDecoder::ImageType image_type,
    render_tree::ResourceProvider* resource_provider) {
  // clang-format off
  const SbDecodeTargetFormat kPreferredFormats[] = {
      kSbDecodeTargetFormat1PlaneRGBA,
      kSbDecodeTargetFormat1PlaneBGRA,
  };
  // clang-format on

  const char* mime_type_c_string = NULL;

  // If we weren't explicitly given a mime type (this might happen if the
  // resource did not get fetched via HTTP), then deduce it from the image's
  // header, if we were able to deduce that.
  if (mime_type.empty()) {
    mime_type_c_string = GetMimeTypeFromImageType(image_type);
  } else {
    mime_type_c_string = mime_type.c_str();
  }

  if (mime_type_c_string) {
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
        resource_provider->SupportsSbDecodeTarget()) {
      return make_scoped_ptr<ImageDataDecoder>(new ImageDecoderStarboard(
          resource_provider, mime_type_c_string, format));
    }
  }
  return scoped_ptr<ImageDataDecoder>();
}
#endif  // SB_HAS(GRAPHICS)

scoped_ptr<ImageDataDecoder> CreateImageDecoderFromImageType(
    ImageDecoder::ImageType image_type,
    render_tree::ResourceProvider* resource_provider) {
  // Call different types of decoders by matching the image signature.
  if (s_use_stub_image_decoder) {
    return make_scoped_ptr<ImageDataDecoder>(
        new StubImageDecoder(resource_provider));
  } else if (image_type == ImageDecoder::kImageTypeJPEG) {
    return make_scoped_ptr<ImageDataDecoder>(
        new JPEGImageDecoder(resource_provider));
  } else if (image_type == ImageDecoder::kImageTypePNG) {
    return make_scoped_ptr<ImageDataDecoder>(
        new PNGImageDecoder(resource_provider));
  } else if (image_type == ImageDecoder::kImageTypeWebP) {
    return make_scoped_ptr<ImageDataDecoder>(
        new WEBPImageDecoder(resource_provider));
  } else if (image_type == ImageDecoder::kImageTypeGIF) {
    return make_scoped_ptr<ImageDataDecoder>(
        new DummyGIFImageDecoder(resource_provider));
  } else {
    return scoped_ptr<ImageDataDecoder>();
  }
}
}  // namespace

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

  if (image_type_ == kImageTypeInvalid) {
    image_type_ = DetermineImageType(signature_cache_.data);
  }

#if SB_HAS(GRAPHICS)
  decoder_ =
      MaybeCreateStarboardDecoder(mime_type_, image_type_, resource_provider_);
#endif  // SB_HAS(GRAPHICS)

  if (!decoder_) {
    decoder_ = CreateImageDecoderFromImageType(image_type_, resource_provider_);
  }

  if (!decoder_) {
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
