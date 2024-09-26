/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "third_party/blink/renderer/core/loader/resource/image_resource.h"

#include <stdint.h>
#include <algorithm>
#include <memory>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/common/loader/referrer_utils.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_info.h"
#include "third_party/blink/renderer/platform/instrumentation/instance_counters.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_client.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_timing.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loading_log.h"
#include "third_party/blink/renderer/platform/loader/fetch/unique_identifier.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/reporting_disposition.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

#include "v8/include/v8.h"

namespace blink {

namespace {

// The amount of time to wait before informing the clients that the image has
// been updated (in seconds). This effectively throttles invalidations that
// result from new data arriving for this image.
constexpr auto kFlushDelay = base::Seconds(1);

}  // namespace

class ImageResource::ImageResourceInfoImpl final
    : public GarbageCollected<ImageResourceInfoImpl>,
      public ImageResourceInfo {
 public:
  explicit ImageResourceInfoImpl(ImageResource* resource)
      : resource_(resource) {
    DCHECK(resource_);
  }
  void Trace(Visitor* visitor) const override {
    visitor->Trace(resource_);
    ImageResourceInfo::Trace(visitor);
  }

 private:
  const KURL& Url() const override { return resource_->Url(); }
  base::TimeTicks LoadResponseEnd() const override {
    return resource_->LoadResponseEnd();
  }
  base::TimeTicks LoadStart() const override {
    if (ResourceLoadTiming* load_timing =
            resource_->GetResponse().GetResourceLoadTiming()) {
      return load_timing->SendStart();
    }
    return base::TimeTicks();
  }
  const ResourceResponse& GetResponse() const override {
    return resource_->GetResponse();
  }
  bool IsCacheValidator() const override {
    return resource_->IsCacheValidator();
  }
  bool IsAccessAllowed(
      DoesCurrentFrameHaveSingleSecurityOrigin
          does_current_frame_has_single_security_origin) const override {
    return resource_->IsAccessAllowed(
        does_current_frame_has_single_security_origin);
  }
  bool HasCacheControlNoStoreHeader() const override {
    return resource_->HasCacheControlNoStoreHeader();
  }
  absl::optional<ResourceError> GetResourceError() const override {
    if (resource_->LoadFailedOrCanceled())
      return resource_->GetResourceError();
    return absl::nullopt;
  }

  void SetDecodedSize(size_t size) override { resource_->SetDecodedSize(size); }
  void WillAddClientOrObserver() override {
    resource_->WillAddClientOrObserver();
  }
  void DidRemoveClientOrObserver() override {
    resource_->DidRemoveClientOrObserver();
  }
  void EmulateLoadStartedForInspector(
      ResourceFetcher* fetcher,
      const KURL& url,
      const AtomicString& initiator_name) override {
    fetcher->EmulateLoadStartedForInspector(
        resource_.Get(), url, mojom::blink::RequestContextType::IMAGE,
        network::mojom::RequestDestination::kImage, initiator_name);
  }

  void LoadDeferredImage(ResourceFetcher* fetcher) override {
    if (resource_->GetType() == ResourceType::kImage &&
        resource_->StillNeedsLoad() &&
        !fetcher->ShouldDeferImageLoad(resource_->Url())) {
      fetcher->StartLoad(resource_);
    }
  }

  bool IsAdResource() const override {
    return resource_->GetResourceRequest().IsAdResource();
  }

  const HashSet<String>* GetUnsupportedImageMimeTypes() const override {
    if (!resource_->Options().unsupported_image_mime_types)
      return nullptr;
    return &resource_->Options().unsupported_image_mime_types->data;
  }

  absl::optional<WebURLRequest::Priority> RequestPriority() const override {
    auto priority = resource_->GetResourceRequest().Priority();
    if (priority == WebURLRequest::Priority::kUnresolved) {
      // This can happen for image documents (e.g. when `<iframe
      // src="title.png">` is the LCP), because the `ImageResource` isn't
      // associated with `ResourceLoader` in such cases. For now, consider the
      // priority not available for such cases by returning nullopt.
      return absl::nullopt;
    }
    return priority;
  }

  const Member<ImageResource> resource_;
};

class ImageResource::ImageResourceFactory : public NonTextResourceFactory {
  STACK_ALLOCATED();

 public:
  explicit ImageResourceFactory()
      : NonTextResourceFactory(ResourceType::kImage) {}

  Resource* Create(const ResourceRequest& request,
                   const ResourceLoaderOptions& options) const override {
    return MakeGarbageCollected<ImageResource>(
        request, options, ImageResourceContent::CreateNotStarted());
  }
};

ImageResource* ImageResource::Fetch(FetchParameters& params,
                                    ResourceFetcher* fetcher) {
  if (params.GetResourceRequest().GetRequestContext() ==
      mojom::blink::RequestContextType::UNSPECIFIED) {
    params.SetRequestContext(mojom::blink::RequestContextType::IMAGE);
    params.SetRequestDestination(network::mojom::RequestDestination::kImage);
  }

  // If the fetch originated from user agent CSS we do not need to check CSP.
  bool is_user_agent_resource = params.Options().initiator_info.name ==
                                fetch_initiator_type_names::kUacss;
  if (is_user_agent_resource) {
    params.SetContentSecurityCheck(
        network::mojom::CSPDisposition::DO_NOT_CHECK);
  }

  auto* resource = To<ImageResource>(
      fetcher->RequestResource(params, ImageResourceFactory(), nullptr));

  resource->GetContent()->SetDiscoveryTime(params.DiscoveryTime());

  // If the fetch originated from user agent CSS we should mark it as a user
  // agent resource.
  if (is_user_agent_resource) {
    resource->FlagAsUserAgentResource();
  }

  return resource;
}

bool ImageResource::CanUseCacheValidator() const {
  // Disable revalidation while ImageResourceContent is still waiting for
  // SVG load completion.
  // TODO(hiroshige): Clean up revalidation-related dependencies.
  if (!GetContent()->IsLoaded())
    return false;

  return Resource::CanUseCacheValidator();
}

ImageResource* ImageResource::Create(
    const ResourceRequest& request,
    scoped_refptr<const DOMWrapperWorld> world) {
  ResourceLoaderOptions options(std::move(world));
  return MakeGarbageCollected<ImageResource>(
      request, options, ImageResourceContent::CreateNotStarted());
}

ImageResource* ImageResource::CreateForTest(const KURL& url) {
  ResourceRequest request(url);
  request.SetInspectorId(CreateUniqueIdentifier());
  // These are needed because some unittests don't go through the usual
  // request setting path in ResourceFetcher.
  request.SetRequestorOrigin(SecurityOrigin::CreateUniqueOpaque());
  request.SetReferrerPolicy(ReferrerUtils::MojoReferrerPolicyResolveDefault(
      request.GetReferrerPolicy()));
  request.SetPriority(WebURLRequest::Priority::kLow);

  return Create(request, nullptr);
}

ImageResource::ImageResource(const ResourceRequest& resource_request,
                             const ResourceLoaderOptions& options,
                             ImageResourceContent* content)
    : Resource(resource_request, ResourceType::kImage, options),
      content_(content) {
  DCHECK(GetContent());
  RESOURCE_LOADING_DVLOG(1)
      << "MakeGarbageCollected<ImageResource>(ResourceRequest) " << this;
  GetContent()->SetImageResourceInfo(
      MakeGarbageCollected<ImageResourceInfoImpl>(this));
}

ImageResource::~ImageResource() {
  RESOURCE_LOADING_DVLOG(1) << "~ImageResource " << this;

  if (is_referenced_from_ua_stylesheet_)
    InstanceCounters::DecrementCounter(InstanceCounters::kUACSSResourceCounter);
}

void ImageResource::OnMemoryDump(WebMemoryDumpLevelOfDetail level_of_detail,
                                 WebProcessMemoryDump* memory_dump) const {
  Resource::OnMemoryDump(level_of_detail, memory_dump);
  const String name = GetMemoryDumpName() + "/image_content";
  auto* dump = memory_dump->CreateMemoryAllocatorDump(name);
  if (content_->HasImage() && content_->GetImage()->HasData())
    dump->AddScalar("size", "bytes", content_->GetImage()->DataSize());
}

void ImageResource::Trace(Visitor* visitor) const {
  visitor->Trace(multipart_parser_);
  visitor->Trace(content_);
  Resource::Trace(visitor);
  MultipartImageResourceParser::Client::Trace(visitor);
}

bool ImageResource::HasClientsOrObservers() const {
  return Resource::HasClientsOrObservers() || GetContent()->HasObservers();
}

void ImageResource::DidAddClient(ResourceClient* client) {
  DCHECK((multipart_parser_ && IsLoading()) || !Data() ||
         GetContent()->HasImage());

  Resource::DidAddClient(client);
}

void ImageResource::DestroyDecodedDataForFailedRevalidation() {
  // Clears the image, as we must create a new image for the failed
  // revalidation response.
  UpdateImage(nullptr, ImageResourceContent::kClearAndUpdateImage, false);
  SetDecodedSize(0);
}

void ImageResource::DestroyDecodedDataIfPossible() {
  GetContent()->DestroyDecodedData();
  if (GetContent()->HasImage() && !IsUnusedPreload() &&
      GetContent()->IsRefetchableDataFromDiskCache()) {
    UMA_HISTOGRAM_MEMORY_KB(
        "Memory.Renderer.EstimatedDroppableEncodedSize",
        base::saturated_cast<base::Histogram::Sample>(EncodedSize() / 1024));
  }
}

void ImageResource::AllClientsAndObserversRemoved() {
  // After ErrorOccurred() is set true in Resource::FinishAsError() before
  // the subsequent UpdateImage() in ImageResource::FinishAsError(),
  // HasImage() is true and ErrorOccurred() is true.
  // |is_during_finish_as_error_| is introduced to allow such cases.
  // crbug.com/701723
  // TODO(hiroshige): Make the CHECK condition cleaner.
  CHECK(is_during_finish_as_error_ || !GetContent()->HasImage() ||
        !ErrorOccurred());
  GetContent()->DoResetAnimation();
  if (multipart_parser_)
    multipart_parser_->Cancel();
  Resource::AllClientsAndObserversRemoved();
}

scoped_refptr<const SharedBuffer> ImageResource::ResourceBuffer() const {
  if (Data())
    return Data();
  return GetContent()->ResourceBuffer();
}

void ImageResource::AppendData(const char* data, size_t length) {
  v8::Isolate::GetCurrent()->AdjustAmountOfExternalAllocatedMemory(length);
  if (multipart_parser_) {
    multipart_parser_->AppendData(data, base::checked_cast<wtf_size_t>(length));
  } else {
    Resource::AppendData(data, length);

    // Update the image immediately if needed.
    //
    // ImageLoader is not available when this image is loaded via ImageDocument.
    // In this case, as the task runner is not available, update the image
    // immediately.
    //
    // TODO(hajimehoshi): updating/flushing image should be throttled when
    // necessary, so such tasks should be done on a throttleable task runner.
    if (GetContent()->ShouldUpdateImageImmediately() || !Loader()) {
      UpdateImage(Data(), ImageResourceContent::kUpdateImage, false);
      return;
    }

    // For other cases, only update at |kFlushDelay| intervals. This
    // throttles how frequently we update |m_image| and how frequently we
    // inform the clients which causes an invalidation of this image. In other
    // words, we only invalidate this image every |kFlushDelay| seconds
    // while loading.
    if (!is_pending_flushing_) {
      scoped_refptr<base::SingleThreadTaskRunner> task_runner =
          Loader()->GetLoadingTaskRunner();
      base::TimeTicks now = base::TimeTicks::Now();
      if (last_flush_time_.is_null())
        last_flush_time_ = now;

      DCHECK_LE(last_flush_time_, now);
      base::TimeDelta flush_delay =
          std::max(base::TimeDelta(), last_flush_time_ - now + kFlushDelay);
      task_runner->PostDelayedTask(
          FROM_HERE,
          WTF::BindOnce(&ImageResource::FlushImageIfNeeded,
                        WrapWeakPersistent(this)),
          flush_delay);
      is_pending_flushing_ = true;
    }
  }
}

void ImageResource::FlushImageIfNeeded() {
  // We might have already loaded the image fully, in which case we don't need
  // to call |updateImage()|.
  if (IsLoading()) {
    last_flush_time_ = base::TimeTicks::Now();
    UpdateImage(Data(), ImageResourceContent::kUpdateImage, false);
  }
  is_pending_flushing_ = false;
}

void ImageResource::DecodeError(bool all_data_received) {
  size_t size = EncodedSize();

  ClearData();
  SetEncodedSize(0);
  if (!ErrorOccurred())
    SetStatus(ResourceStatus::kDecodeError);

  if (multipart_parser_)
    multipart_parser_->Cancel();

  bool is_multipart = !!multipart_parser_;
  // Finishes loading if needed, and notifies observers.
  if (!all_data_received && Loader()) {
    // Observers are notified via ImageResource::finish().
    // TODO(hiroshige): Do not call didFinishLoading() directly.
    Loader()->AbortResponseBodyLoading();
    Loader()->DidFinishLoading(base::TimeTicks::Now(), size, size, size, false);
  } else {
    auto result = GetContent()->UpdateImage(
        nullptr, GetStatus(),
        ImageResourceContent::kClearImageAndNotifyObservers, all_data_received,
        is_multipart);
    DCHECK_EQ(result, ImageResourceContent::UpdateImageResult::kNoDecodeError);
  }

  MemoryCache::Get()->Remove(this);
}

void ImageResource::UpdateImageAndClearBuffer() {
  UpdateImage(Data(), ImageResourceContent::kClearAndUpdateImage, true);
  ClearData();
}

void ImageResource::NotifyStartLoad() {
  Resource::NotifyStartLoad();
  CHECK_EQ(GetStatus(), ResourceStatus::kPending);
  GetContent()->NotifyStartLoad();
}

void ImageResource::Finish(base::TimeTicks load_finish_time,
                           base::SingleThreadTaskRunner* task_runner) {
  if (multipart_parser_) {
    if (!ErrorOccurred())
      multipart_parser_->Finish();
    if (Data())
      UpdateImageAndClearBuffer();
  } else {
    UpdateImage(Data(), ImageResourceContent::kUpdateImage, true);
    // As encoded image data can be obtained from Image::Data() via `content_`
    // (see ResourceBuffer()), we don't have to keep `data_`. Let's
    // clear it. As for the lifetimes of `content_` and `data_`, see this
    // document:
    // https://docs.google.com/document/d/1v0yTAZ6wkqX2U_M6BNIGUJpM1s0TIw1VsqpxoL7aciY/edit?usp=sharing
    ClearData();
  }
  Resource::Finish(load_finish_time, task_runner);
}

void ImageResource::FinishAsError(const ResourceError& error,
                                  base::SingleThreadTaskRunner* task_runner) {
  if (multipart_parser_)
    multipart_parser_->Cancel();
  // TODO(hiroshige): Move setEncodedSize() call to Resource::error() if it
  // is really needed, or remove it otherwise.
  SetEncodedSize(0);
  is_during_finish_as_error_ = true;
  Resource::FinishAsError(error, task_runner);
  is_during_finish_as_error_ = false;
  UpdateImage(nullptr, ImageResourceContent::kClearImageAndNotifyObservers,
              true);
}

void ImageResource::ResponseReceived(const ResourceResponse& response) {
  DCHECK(!multipart_parser_);
  if (response.MimeType() == "multipart/x-mixed-replace") {
    Vector<char> boundary = network_utils::ParseMultipartBoundary(
        response.HttpHeaderField(http_names::kContentType));
    // If there's no boundary, just handle the request normally.
    if (!boundary.empty()) {
      multipart_parser_ = MakeGarbageCollected<MultipartImageResourceParser>(
          response, boundary, this);
    }
  }

  // Notify the base class that a response has been received. Note that after
  // this call, |GetResponse()| will represent the full effective
  // ResourceResponse, while |response| might just be a revalidation response
  // (e.g. a 304) with a partial set of updated headers that were folded into
  // the cached response.
  Resource::ResponseReceived(response);
}

void ImageResource::OnePartInMultipartReceived(
    const ResourceResponse& response) {
  DCHECK(multipart_parser_);

  if (!GetResponse().IsNull()) {
    CHECK_EQ(GetResponse().WasFetchedViaServiceWorker(),
             response.WasFetchedViaServiceWorker());
    CHECK_EQ(GetResponse().GetType(), response.GetType());
  }

  SetResponse(response);
  if (multipart_parsing_state_ == MultipartParsingState::kWaitingForFirstPart) {
    // We have nothing to do because we don't have any data.
    multipart_parsing_state_ = MultipartParsingState::kParsingFirstPart;
    return;
  }
  UpdateImageAndClearBuffer();

  if (multipart_parsing_state_ == MultipartParsingState::kParsingFirstPart) {
    multipart_parsing_state_ = MultipartParsingState::kFinishedParsingFirstPart;
    // Notify finished when the first part ends.
    if (!ErrorOccurred())
      SetStatus(ResourceStatus::kCached);
    // We notify clients and observers of finish in checkNotify() and
    // updateImageAndClearBuffer(), respectively, and they will not be
    // notified again in Resource::finish()/error().
    NotifyFinished();
    if (Loader())
      Loader()->DidFinishLoadingFirstPartInMultipart();
  }
}

void ImageResource::MultipartDataReceived(const char* bytes, size_t size) {
  DCHECK(multipart_parser_);
  Resource::AppendData(bytes, size);
}

bool ImageResource::IsAccessAllowed(
    ImageResourceInfo::DoesCurrentFrameHaveSingleSecurityOrigin
        does_current_frame_has_single_security_origin) const {
  if (does_current_frame_has_single_security_origin !=
      ImageResourceInfo::kHasSingleSecurityOrigin)
    return false;

  return GetResponse().IsCorsSameOrigin();
}

ImageResourceContent* ImageResource::GetContent() {
  return content_;
}

const ImageResourceContent* ImageResource::GetContent() const {
  return content_;
}

std::pair<ResourcePriority, ResourcePriority>
ImageResource::PriorityFromObservers() {
  return GetContent()->PriorityFromObservers();
}

void ImageResource::UpdateImage(
    scoped_refptr<SharedBuffer> shared_buffer,
    ImageResourceContent::UpdateImageOption update_image_option,
    bool all_data_received) {
  bool is_multipart = !!multipart_parser_;
  auto result = GetContent()->UpdateImage(std::move(shared_buffer), GetStatus(),
                                          update_image_option,
                                          all_data_received, is_multipart);
  if (result == ImageResourceContent::UpdateImageResult::kShouldDecodeError) {
    // In case of decode error, we call imageNotifyFinished() iff we don't
    // initiate reloading:
    // [(a): when this is in the middle of loading, or (b): otherwise]
    // 1. The updateImage() call above doesn't call notifyObservers().
    // 2. notifyObservers(ShouldNotifyFinish) is called
    //    (a) via updateImage() called in ImageResource::finish()
    //        called via didFinishLoading() called in decodeError(), or
    //    (b) via updateImage() called in decodeError().
    //    imageNotifyFinished() is called here iff we will not initiate
    //    reloading in Step 3 due to notifyObservers()'s
    //    schedulingReloadOrShouldReloadBrokenPlaceholder() check.
    // 3. reloadIfLoFiOrPlaceholderImage() is called via ResourceFetcher
    //    (a) via didFinishLoading() called in decodeError(), or
    //    (b) after returning ImageResource::updateImage().
    DecodeError(all_data_received);
  }
}

void ImageResource::FlagAsUserAgentResource() {
  if (is_referenced_from_ua_stylesheet_)
    return;

  InstanceCounters::IncrementCounter(InstanceCounters::kUACSSResourceCounter);
  is_referenced_from_ua_stylesheet_ = true;
}

}  // namespace blink
