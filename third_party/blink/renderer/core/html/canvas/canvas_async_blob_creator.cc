// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/canvas/canvas_async_blob_creator.h"

#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metrics.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_rendering_context.h"
#include "third_party/blink/renderer/platform/graphics/image_data_buffer.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/image-encoders/image_encoder_utils.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

namespace {

// small slack period between deadline and current time for safety
constexpr base::TimeDelta kCreateBlobSlackBeforeDeadline =
    base::Milliseconds(1);
constexpr base::TimeDelta kEncodeRowSlackBeforeDeadline =
    base::Microseconds(100);

/* The value is based on user statistics on Nov 2017. */
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || \
     BUILDFLAG(IS_WIN))
const double kIdleTaskStartTimeoutDelayMs = 1000.0;
#else
const double kIdleTaskStartTimeoutDelayMs = 4000.0;  // For ChromeOS, Mobile
#endif

/* The value is based on user statistics on May 2018. */
// We should be more lenient on completion timeout delay to ensure that the
// switch from idle to main thread only happens to a minority of toBlob calls
#if !BUILDFLAG(IS_ANDROID)
// Png image encoding on 4k by 4k canvas on Mac HDD takes 5.7+ seconds
// We see that 99% users require less than 5 seconds.
const double kIdleTaskCompleteTimeoutDelayMs = 5700.0;
#else
// Png image encoding on 4k by 4k canvas on Android One takes 9.0+ seconds
// We see that 99% users require less than 9 seconds.
const double kIdleTaskCompleteTimeoutDelayMs = 9000.0;
#endif

bool IsCreateBlobDeadlineNearOrPassed(base::TimeTicks deadline) {
  return base::TimeTicks::Now() >= deadline - kCreateBlobSlackBeforeDeadline;
}

bool IsEncodeRowDeadlineNearOrPassed(base::TimeTicks deadline,
                                     size_t image_width) {
  // Rough estimate of the row encoding time in micro seconds. We will consider
  // a slack time later to not pass the idle task deadline.
  int row_encode_time_us = 1000 * (kIdleTaskCompleteTimeoutDelayMs / 4000.0) *
                           (image_width / 4000.0);
  base::TimeDelta row_encode_time_delta =
      base::Microseconds(row_encode_time_us);
  return base::TimeTicks::Now() >=
         deadline - row_encode_time_delta - kEncodeRowSlackBeforeDeadline;
}

void RecordIdleTaskStatusHistogram(
    CanvasAsyncBlobCreator::IdleTaskStatus status) {
  UMA_HISTOGRAM_ENUMERATION("Blink.Canvas.ToBlob.IdleTaskStatus", status);
}

void RecordInitiateEncodingTimeHistogram(ImageEncodingMimeType mime_type,
                                         base::TimeDelta elapsed_time) {
  if (mime_type == kMimeTypePng) {
    UmaHistogramMicrosecondsTimes(
        "Blink.Canvas.ToBlob.InitialEncodingDelay.PNG", elapsed_time);
  } else if (mime_type == kMimeTypeJpeg) {
    UmaHistogramMicrosecondsTimes(
        "Blink.Canvas.ToBlob.InitialEncodingDelay.JPEG", elapsed_time);
  }
}

void RecordCompleteEncodingTimeHistogram(ImageEncodingMimeType mime_type,
                                         base::TimeDelta elapsed_time) {
  if (mime_type == kMimeTypePng) {
    UmaHistogramMicrosecondsTimes("Blink.Canvas.ToBlob.TotalEncodingDelay.PNG",
                                  elapsed_time);
  } else if (mime_type == kMimeTypeJpeg) {
    UmaHistogramMicrosecondsTimes("Blink.Canvas.ToBlob.TotalEncodingDelay.JPEG",
                                  elapsed_time);
  }
}

void RecordScaledDurationHistogram(ImageEncodingMimeType mime_type,
                                   base::TimeDelta elapsed_time,
                                   float width,
                                   float height) {
  float sqrt_pixels = std::sqrt(width) * std::sqrt(height);
  float scaled_time_float =
      elapsed_time.InMicrosecondsF() / (sqrt_pixels == 0 ? 1.0f : sqrt_pixels);

  // If scaled_time_float overflows as integer, CheckedNumeric will store it
  // as invalid, then ValueOrDefault will return the maximum int.
  base::CheckedNumeric<int> checked_scaled_time = scaled_time_float;
  int scaled_time_int =
      checked_scaled_time.ValueOrDefault(std::numeric_limits<int>::max());

  if (mime_type == kMimeTypePng) {
    UMA_HISTOGRAM_COUNTS_100000("Blink.Canvas.ToBlob.ScaledDuration.PNG",
                                scaled_time_int);
  } else if (mime_type == kMimeTypeJpeg) {
    UMA_HISTOGRAM_COUNTS_100000("Blink.Canvas.ToBlob.ScaledDuration.JPEG",
                                scaled_time_int);
  } else if (mime_type == kMimeTypeWebp) {
    UMA_HISTOGRAM_COUNTS_100000("Blink.Canvas.ToBlob.ScaledDuration.WEBP",
                                scaled_time_int);
  }
}

}  // anonymous namespace

CanvasAsyncBlobCreator::CanvasAsyncBlobCreator(
    scoped_refptr<StaticBitmapImage> image,
    const ImageEncodeOptions* options,
    ToBlobFunctionType function_type,
    base::TimeTicks start_time,
    ExecutionContext* context,
    const IdentifiableToken& input_digest,
    ScriptPromiseResolver* resolver)
    : CanvasAsyncBlobCreator(image,
                             options,
                             function_type,
                             nullptr,
                             start_time,
                             context,
                             input_digest,
                             resolver) {}

CanvasAsyncBlobCreator::CanvasAsyncBlobCreator(
    scoped_refptr<StaticBitmapImage> image,
    const ImageEncodeOptions* options,
    ToBlobFunctionType function_type,
    V8BlobCallback* callback,
    base::TimeTicks start_time,
    ExecutionContext* context,
    const IdentifiableToken& input_digest,
    ScriptPromiseResolver* resolver)
    : fail_encoder_initialization_for_test_(false),
      enforce_idle_encoding_for_test_(false),
      context_(context),
      encode_options_(options),
      function_type_(function_type),
      start_time_(start_time),
      static_bitmap_image_loaded_(false),
      input_digest_(input_digest),
      callback_(callback),
      script_promise_resolver_(resolver) {
  DCHECK(context);

  mime_type_ = ImageEncoderUtils::ToEncodingMimeType(
      encode_options_->type(),
      ImageEncoderUtils::kEncodeReasonConvertToBlobPromise);

  // We use pixmap to access the image pixels. Make the image unaccelerated if
  // necessary.
  DCHECK(image);
  image_ = image->MakeUnaccelerated();

  DCHECK(image_);
  DCHECK(!image_->IsTextureBacked());

  sk_sp<SkImage> skia_image =
      image_->PaintImageForCurrentFrame().GetSwSkImage();
  DCHECK(skia_image);
  DCHECK(!skia_image->isTextureBacked());

  // If image is lazy decoded, call readPixels() to trigger decoding.
  if (skia_image->isLazyGenerated()) {
    SkImageInfo info = SkImageInfo::MakeN32Premul(1, 1);
    uint8_t pixel[info.bytesPerPixel()];
    skia_image->readPixels(info, pixel, info.minRowBytes(), 0, 0);
  }

  if (skia_image->peekPixels(&src_data_)) {
    static_bitmap_image_loaded_ = true;

    // Ensure that the size of the to-be-encoded-image does not pass the maximum
    // size supported by the encoders.
    int max_dimension = ImageEncoder::MaxDimension(mime_type_);
    if (std::max(src_data_.width(), src_data_.height()) > max_dimension) {
      SkImageInfo info = src_data_.info();
      info = info.makeWH(std::min(info.width(), max_dimension),
                         std::min(info.height(), max_dimension));
      src_data_.reset(info, src_data_.addr(), src_data_.rowBytes());
    }
  }

  idle_task_status_ = kIdleTaskNotSupported;
  num_rows_completed_ = 0;
  if (context->IsWindow()) {
    parent_frame_task_runner_ =
        context->GetTaskRunner(TaskType::kCanvasBlobSerialization);
  }
}

CanvasAsyncBlobCreator::~CanvasAsyncBlobCreator() = default;

void CanvasAsyncBlobCreator::Dispose() {
  // Eagerly let go of references to prevent retention of these
  // resources while any remaining posted tasks are queued.
  context_.Clear();
  callback_.Clear();
  script_promise_resolver_.Clear();
  image_ = nullptr;
}

ImageEncodeOptions* CanvasAsyncBlobCreator::GetImageEncodeOptionsForMimeType(
    ImageEncodingMimeType mime_type) {
  ImageEncodeOptions* encode_options = ImageEncodeOptions::Create();
  encode_options->setType(ImageEncodingMimeTypeName(mime_type));
  return encode_options;
}

bool CanvasAsyncBlobCreator::EncodeImage(const double& quality) {
  std::unique_ptr<ImageDataBuffer> buffer = ImageDataBuffer::Create(src_data_);
  if (!buffer)
    return false;
  return buffer->EncodeImage(mime_type_, quality, &encoded_image_);
}

void CanvasAsyncBlobCreator::ScheduleAsyncBlobCreation(const double& quality) {
  if (!static_bitmap_image_loaded_) {
    context_->GetTaskRunner(TaskType::kCanvasBlobSerialization)
        ->PostTask(
            FROM_HERE,
            WTF::BindOnce(&CanvasAsyncBlobCreator::CreateNullAndReturnResult,
                          WrapPersistent(this)));
    return;
  }
  // Webp encoder does not support progressive encoding. We also don't use idle
  // encoding for web tests, since the idle task start and completition
  // deadlines (6.7s or 13s) bypass the web test running deadline (6s)
  // and result in timeouts on different tests. We use
  // enforce_idle_encoding_for_test_ to test idle encoding in unit tests.
  // We also don't use idle tasks in workers because the short idle periods are
  // not implemented, so the idle task can take a long time even when the thread
  // is not busy.
  bool use_idle_encoding =
      WTF::IsMainThread() && (mime_type_ != kMimeTypeWebp) &&
      (enforce_idle_encoding_for_test_ ||
       !RuntimeEnabledFeatures::NoIdleEncodingForWebTestsEnabled());

  if (!use_idle_encoding) {
    if (!IsMainThread()) {
      DCHECK(function_type_ == kOffscreenCanvasConvertToBlobPromise);
      // When OffscreenCanvas.convertToBlob() occurs on worker thread,
      // we do not need to use background task runner to reduce load on main.
      // So we just directly encode images on the worker thread.
      if (!EncodeImage(quality)) {
        context_->GetTaskRunner(TaskType::kCanvasBlobSerialization)
            ->PostTask(FROM_HERE,
                       WTF::BindOnce(
                           &CanvasAsyncBlobCreator::CreateNullAndReturnResult,
                           WrapPersistent(this)));

        return;
      }
      context_->GetTaskRunner(TaskType::kCanvasBlobSerialization)
          ->PostTask(
              FROM_HERE,
              WTF::BindOnce(&CanvasAsyncBlobCreator::CreateBlobAndReturnResult,
                            WrapPersistent(this)));

    } else {
      worker_pool::PostTask(
          FROM_HERE, CrossThreadBindOnce(
                         &CanvasAsyncBlobCreator::EncodeImageOnEncoderThread,
                         WrapCrossThreadPersistent(this), quality));
    }
  } else {
    idle_task_status_ = kIdleTaskNotStarted;
    ScheduleInitiateEncoding(quality);

    // We post the below task to check if the above idle task isn't late.
    // There's no risk of concurrency as both tasks are on the same thread.
    PostDelayedTaskToCurrentThread(
        FROM_HERE,
        WTF::BindOnce(&CanvasAsyncBlobCreator::IdleTaskStartTimeoutEvent,
                      WrapPersistent(this), quality),
        kIdleTaskStartTimeoutDelayMs);
  }
}

void CanvasAsyncBlobCreator::ScheduleInitiateEncoding(double quality) {
  schedule_idle_task_start_time_ = base::TimeTicks::Now();
  ThreadScheduler::Current()->PostIdleTask(
      FROM_HERE, WTF::BindOnce(&CanvasAsyncBlobCreator::InitiateEncoding,
                               WrapPersistent(this), quality));
}

void CanvasAsyncBlobCreator::InitiateEncoding(double quality,
                                              base::TimeTicks deadline) {
  if (idle_task_status_ == kIdleTaskSwitchedToImmediateTask) {
    return;
  }
  RecordInitiateEncodingTimeHistogram(
      mime_type_, base::TimeTicks::Now() - schedule_idle_task_start_time_);

  DCHECK(idle_task_status_ == kIdleTaskNotStarted);
  idle_task_status_ = kIdleTaskStarted;

  if (!InitializeEncoder(quality)) {
    idle_task_status_ = kIdleTaskFailed;
    return;
  }

  // Re-use this time variable to collect data on complete encoding delay
  schedule_idle_task_start_time_ = base::TimeTicks::Now();
  IdleEncodeRows(deadline);
}

void CanvasAsyncBlobCreator::IdleEncodeRows(base::TimeTicks deadline) {
  if (idle_task_status_ == kIdleTaskSwitchedToImmediateTask) {
    return;
  }

  for (int y = num_rows_completed_; y < src_data_.height(); ++y) {
    if (IsEncodeRowDeadlineNearOrPassed(deadline, src_data_.width())) {
      num_rows_completed_ = y;
      ThreadScheduler::Current()->PostIdleTask(
          FROM_HERE, WTF::BindOnce(&CanvasAsyncBlobCreator::IdleEncodeRows,
                                   WrapPersistent(this)));
      return;
    }

    if (!encoder_->encodeRows(1)) {
      idle_task_status_ = kIdleTaskFailed;
      CreateNullAndReturnResult();
      return;
    }
  }
  num_rows_completed_ = src_data_.height();

  idle_task_status_ = kIdleTaskCompleted;
  base::TimeDelta elapsed_time =
      base::TimeTicks::Now() - schedule_idle_task_start_time_;
  RecordCompleteEncodingTimeHistogram(mime_type_, elapsed_time);
  if (IsCreateBlobDeadlineNearOrPassed(deadline)) {
    context_->GetTaskRunner(TaskType::kCanvasBlobSerialization)
        ->PostTask(
            FROM_HERE,
            WTF::BindOnce(&CanvasAsyncBlobCreator::CreateBlobAndReturnResult,
                          WrapPersistent(this)));
  } else {
    CreateBlobAndReturnResult();
  }
}

void CanvasAsyncBlobCreator::ForceEncodeRowsOnCurrentThread() {
  DCHECK(idle_task_status_ == kIdleTaskSwitchedToImmediateTask);

  // Continue encoding from the last completed row
  for (int y = num_rows_completed_; y < src_data_.height(); ++y) {
    if (!encoder_->encodeRows(1)) {
      idle_task_status_ = kIdleTaskFailed;
      CreateNullAndReturnResult();
      return;
    }
  }
  num_rows_completed_ = src_data_.height();

  if (IsMainThread()) {
    CreateBlobAndReturnResult();
  } else {
    PostCrossThreadTask(
        *context_->GetTaskRunner(TaskType::kCanvasBlobSerialization), FROM_HERE,
        CrossThreadBindOnce(&CanvasAsyncBlobCreator::CreateBlobAndReturnResult,
                            WrapCrossThreadPersistent(this)));
  }

  SignalAlternativeCodePathFinishedForTesting();
}

void CanvasAsyncBlobCreator::CreateBlobAndReturnResult() {
  RecordIdleTaskStatusHistogram(idle_task_status_);

  Blob* result_blob = Blob::Create(encoded_image_.data(), encoded_image_.size(),
                                   ImageEncodingMimeTypeName(mime_type_));
  if (function_type_ == kHTMLCanvasToBlobCallback) {
    context_->GetTaskRunner(TaskType::kCanvasBlobSerialization)
        ->PostTask(FROM_HERE,
                   WTF::BindOnce(&V8BlobCallback::InvokeAndReportException,
                                 WrapPersistent(callback_.Get()), nullptr,
                                 WrapPersistent(result_blob)));
  } else {
    context_->GetTaskRunner(TaskType::kCanvasBlobSerialization)
        ->PostTask(FROM_HERE,
                   WTF::BindOnce(&ScriptPromiseResolver::Resolve<Blob*>,
                                 WrapPersistent(script_promise_resolver_.Get()),
                                 WrapPersistent(result_blob)));
  }

  RecordScaledDurationHistogram(mime_type_,
                                base::TimeTicks::Now() - start_time_,
                                image_->width(), image_->height());

  if (IdentifiabilityStudySettings::Get()->ShouldSampleType(
          blink::IdentifiableSurface::Type::kCanvasReadback)) {
    // Creating this ImageDataBuffer has some overhead, namely getting the
    // SkImage and computing the pixmap. We need the StaticBitmapImage to be
    // deleted on the same thread on which it was created, so we use the same
    // TaskType here in order to get the same TaskRunner.

    // TODO(crbug.com/1143737) WrapPersistent(this) stores more data than is
    // needed by the function. It would be good to find a way to wrap only the
    // objects needed (image_, ukm_source_id_, input_digest_, context_)
    context_->GetTaskRunner(TaskType::kCanvasBlobSerialization)
        ->PostTask(
            FROM_HERE,
            WTF::BindOnce(&CanvasAsyncBlobCreator::RecordIdentifiabilityMetric,
                          WrapPersistent(this)));
  } else {
    // RecordIdentifiabilityMetric needs a reference to image_, and will run
    // dispose itself. So here we only call dispose if not recording the metric.
    Dispose();
  }
}

void CanvasAsyncBlobCreator::RecordIdentifiabilityMetric() {
  std::unique_ptr<ImageDataBuffer> data_buffer =
      ImageDataBuffer::Create(image_);

  if (data_buffer) {
    blink::IdentifiabilityMetricBuilder(context_->UkmSourceID())
        .Add(blink::IdentifiableSurface::FromTypeAndToken(
                 blink::IdentifiableSurface::Type::kCanvasReadback,
                 input_digest_),
             blink::IdentifiabilityDigestOfBytes(base::make_span(
                 data_buffer->Pixels(), data_buffer->ComputeByteSize())))
        .Record(context_->UkmRecorder());
  }

  // Avoid unwanted retention, see dispose().
  Dispose();
}

void CanvasAsyncBlobCreator::CreateNullAndReturnResult() {
  RecordIdleTaskStatusHistogram(idle_task_status_);
  if (function_type_ == kHTMLCanvasToBlobCallback) {
    DCHECK(IsMainThread());
    RecordIdleTaskStatusHistogram(idle_task_status_);
    context_->GetTaskRunner(TaskType::kCanvasBlobSerialization)
        ->PostTask(
            FROM_HERE,
            WTF::BindOnce(&V8BlobCallback::InvokeAndReportException,
                          WrapPersistent(callback_.Get()), nullptr, nullptr));
  } else {
    context_->GetTaskRunner(TaskType::kCanvasBlobSerialization)
        ->PostTask(
            FROM_HERE,
            WTF::BindOnce(&ScriptPromiseResolver::Reject<DOMException*>,
                          WrapPersistent(script_promise_resolver_.Get()),
                          WrapPersistent(MakeGarbageCollected<DOMException>(
                              DOMExceptionCode::kEncodingError,
                              "Encoding of the source image has failed."))));
  }
  // Avoid unwanted retention, see dispose().
  Dispose();
}

void CanvasAsyncBlobCreator::EncodeImageOnEncoderThread(double quality) {
  DCHECK(!IsMainThread());
  if (!EncodeImage(quality)) {
    PostCrossThreadTask(
        *parent_frame_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&CanvasAsyncBlobCreator::CreateNullAndReturnResult,
                            WrapCrossThreadPersistent(this)));
    return;
  }

  PostCrossThreadTask(
      *parent_frame_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&CanvasAsyncBlobCreator::CreateBlobAndReturnResult,
                          WrapCrossThreadPersistent(this)));
}

bool CanvasAsyncBlobCreator::InitializeEncoder(double quality) {
  // This is solely used for unit tests.
  if (fail_encoder_initialization_for_test_)
    return false;
  if (mime_type_ == kMimeTypeJpeg) {
    SkJpegEncoder::Options options;
    options.fQuality = ImageEncoder::ComputeJpegQuality(quality);
    options.fAlphaOption = SkJpegEncoder::AlphaOption::kBlendOnBlack;
    if (options.fQuality == 100) {
      options.fDownsample = SkJpegEncoder::Downsample::k444;
    }
    encoder_ = ImageEncoder::Create(&encoded_image_, src_data_, options);
  } else {
    // Progressive encoding is only applicable to png and jpeg image format,
    // and thus idle tasks scheduling can only be applied to these image
    // formats.
    // TODO(zakerinasab): Progressive encoding on webp image formats
    // (crbug.com/571399)
    DCHECK_EQ(kMimeTypePng, mime_type_);
    SkPngEncoder::Options options;
    options.fFilterFlags = SkPngEncoder::FilterFlag::kSub;
    options.fZLibLevel = 3;
    encoder_ = ImageEncoder::Create(&encoded_image_, src_data_, options);
  }

  return encoder_.get();
}

void CanvasAsyncBlobCreator::IdleTaskStartTimeoutEvent(double quality) {
  if (idle_task_status_ == kIdleTaskStarted) {
    // Even if the task started quickly, we still want to ensure completion
    PostDelayedTaskToCurrentThread(
        FROM_HERE,
        WTF::BindOnce(&CanvasAsyncBlobCreator::IdleTaskCompleteTimeoutEvent,
                      WrapPersistent(this)),
        kIdleTaskCompleteTimeoutDelayMs);
  } else if (idle_task_status_ == kIdleTaskNotStarted) {
    // If the idle task does not start after a delay threshold, we will
    // force it to happen on main thread (even though it may cause more
    // janks) to prevent toBlob being postponed forever in extreme cases.
    idle_task_status_ = kIdleTaskSwitchedToImmediateTask;
    SignalTaskSwitchInStartTimeoutEventForTesting();

    DCHECK(mime_type_ == kMimeTypePng || mime_type_ == kMimeTypeJpeg);
    if (InitializeEncoder(quality)) {
      context_->GetTaskRunner(TaskType::kCanvasBlobSerialization)
          ->PostTask(
              FROM_HERE,
              WTF::BindOnce(
                  &CanvasAsyncBlobCreator::ForceEncodeRowsOnCurrentThread,
                  WrapPersistent(this)));
    } else {
      // Failing in initialization of encoder
      SignalAlternativeCodePathFinishedForTesting();
    }
  } else {
    DCHECK(idle_task_status_ == kIdleTaskFailed ||
           idle_task_status_ == kIdleTaskCompleted);
    SignalAlternativeCodePathFinishedForTesting();
  }
}

void CanvasAsyncBlobCreator::IdleTaskCompleteTimeoutEvent() {
  DCHECK(idle_task_status_ != kIdleTaskNotStarted);

  if (idle_task_status_ == kIdleTaskStarted) {
    // It has taken too long to complete for the idle task.
    idle_task_status_ = kIdleTaskSwitchedToImmediateTask;
    SignalTaskSwitchInCompleteTimeoutEventForTesting();

    DCHECK(mime_type_ == kMimeTypePng || mime_type_ == kMimeTypeJpeg);
    context_->GetTaskRunner(TaskType::kCanvasBlobSerialization)
        ->PostTask(FROM_HERE,
                   WTF::BindOnce(
                       &CanvasAsyncBlobCreator::ForceEncodeRowsOnCurrentThread,
                       WrapPersistent(this)));
  } else {
    DCHECK(idle_task_status_ == kIdleTaskFailed ||
           idle_task_status_ == kIdleTaskCompleted);
    SignalAlternativeCodePathFinishedForTesting();
  }
}

void CanvasAsyncBlobCreator::PostDelayedTaskToCurrentThread(
    const base::Location& location,
    base::OnceClosure task,
    double delay_ms) {
  context_->GetTaskRunner(TaskType::kCanvasBlobSerialization)
      ->PostDelayedTask(location, std::move(task),
                        base::Milliseconds(delay_ms));
}

void CanvasAsyncBlobCreator::Trace(Visitor* visitor) const {
  visitor->Trace(context_);
  visitor->Trace(encode_options_);
  visitor->Trace(callback_);
  visitor->Trace(script_promise_resolver_);
}

bool CanvasAsyncBlobCreator::EncodeImageForConvertToBlobTest() {
  if (!static_bitmap_image_loaded_)
    return false;
  std::unique_ptr<ImageDataBuffer> buffer = ImageDataBuffer::Create(src_data_);
  if (!buffer)
    return false;
  return buffer->EncodeImage(mime_type_, encode_options_->quality(),
                             &encoded_image_);
}

}  // namespace blink
