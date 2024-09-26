// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/url_loader/navigation_body_loader.h"

#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/record_ontransfersizeupdate_utils.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/referrer_utils.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom-blink.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom.h"
#include "third_party/blink/public/platform/resource_load_info_notifier_wrapper.h"
#include "third_party/blink/public/platform/web_code_cache_loader.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_navigation_params.h"
#include "third_party/blink/renderer/platform/loader/fetch/body_text_decoder.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"
#include "third_party/ced/src/compact_enc_det/compact_enc_det.h"

namespace blink {
namespace {

bool ShouldSendDirectlyToPreloadScanner() {
  static const base::FeatureParam<bool> kSendToScannerParam{
      &features::kThreadedBodyLoader, "send-to-scanner", true};
  return kSendToScannerParam.Get();
}

// Returns the maximum data size to process in TakeData(). Returning 0 means
// process all the data available.
size_t GetMaxDataToProcessPerTask() {
  static const base::FeatureParam<int> kMaxDataToProcessParam{
      &features::kThreadedBodyLoader, "max-data-to-process", 0};
  return kMaxDataToProcessParam.Get();
}

// A chunk of data read by the OffThreadBodyReader. This will be created on a
// background thread and processed on the main thread.
struct DataChunk {
  String decoded_data;
  bool has_seen_end_of_data = false;
  bool has_error = false;
  std::unique_ptr<char[]> encoded_data;
  size_t encoded_data_size = 0;
  WebEncodingData encoding_data;
};

// This interface abstracts out the logic for consuming the response body and
// allows calling ReadFromDataPipeImpl() on either the main thread or a
// background thread.
class BodyReader {
 public:
  virtual ~BodyReader() = default;
  virtual bool ShouldContinueReading() = 0;
  virtual void FinishedReading(bool has_error) = 0;
  virtual bool DataReceived(const char* data, size_t size) = 0;
};

void ReadFromDataPipeImpl(BodyReader& reader,
                          mojo::ScopedDataPipeConsumerHandle& handle,
                          mojo::SimpleWatcher& handle_watcher) {
  uint32_t num_bytes_consumed = 0;
  while (reader.ShouldContinueReading()) {
    const void* buffer = nullptr;
    uint32_t available = 0;
    MojoResult result =
        handle->BeginReadData(&buffer, &available, MOJO_READ_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      handle_watcher.ArmOrNotify();
      return;
    }
    if (result == MOJO_RESULT_FAILED_PRECONDITION) {
      reader.FinishedReading(/*has_error=*/false);
      return;
    }
    if (result != MOJO_RESULT_OK) {
      reader.FinishedReading(/*has_error=*/true);
      return;
    }
    const uint32_t chunk_size = network::features::GetLoaderChunkSize();
    DCHECK_LE(num_bytes_consumed, chunk_size);
    available = std::min(available, chunk_size - num_bytes_consumed);
    if (available == 0) {
      // We've already consumed many bytes in this task. Defer the remaining
      // to the next task.
      result = handle->EndReadData(0);
      DCHECK_EQ(result, MOJO_RESULT_OK);
      handle_watcher.ArmOrNotify();
      return;
    }
    num_bytes_consumed += available;
    if (!reader.DataReceived(static_cast<const char*>(buffer), available))
      return;
    result = handle->EndReadData(available);
    DCHECK_EQ(MOJO_RESULT_OK, result);
  }
}

}  // namespace

class NavigationBodyLoader::OffThreadBodyReader : public BodyReader {
 public:
  OffThreadBodyReader(
      mojo::ScopedDataPipeConsumerHandle response_body,
      std::unique_ptr<BodyTextDecoder> decoder,
      base::WeakPtr<NavigationBodyLoader> body_loader,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
      scoped_refptr<base::SequencedTaskRunner> reader_task_runner,
      bool should_keep_encoded_data)
      : response_body_(std::move(response_body)),
        decoder_(std::move(decoder)),
        should_keep_encoded_data_(should_keep_encoded_data),
        main_thread_task_runner_(std::move(main_thread_task_runner)),
        reader_task_runner_(std::move(reader_task_runner)),
        body_loader_(std::move(body_loader)) {
    DCHECK(IsMainThread());
    PostCrossThreadTask(
        *reader_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&OffThreadBodyReader::StartInBackground,
                            CrossThreadUnretained(this)));
  }

  ~OffThreadBodyReader() override {
    DCHECK(reader_task_runner_->RunsTasksInCurrentSequence());
  }

  Vector<DataChunk> TakeData(size_t max_data_to_process) {
    DCHECK(IsMainThread());
    base::AutoLock lock(lock_);
    if (max_data_to_process == 0)
      return std::move(data_chunks_);

    Vector<DataChunk> data;
    size_t data_processed = 0;
    while (!data_chunks_.empty() && data_processed < max_data_to_process) {
      data.emplace_back(std::move(data_chunks_.front()));
      data_processed += data.back().encoded_data_size;
      data_chunks_.erase(data_chunks_.begin());
    }
    if (!data_chunks_.empty()) {
      PostCrossThreadTask(
          *main_thread_task_runner_, FROM_HERE,
          CrossThreadBindOnce(&NavigationBodyLoader::ProcessOffThreadData,
                              body_loader_));
    }
    return data;
  }

  void StoreProcessBackgroundDataCallback(Client* client) {
    DCHECK(IsMainThread());
    if (background_callback_set_)
      return;

    auto callback = client->TakeProcessBackgroundDataCallback();
    if (!callback)
      return;

    background_callback_set_ = true;

    base::AutoLock lock(lock_);
    process_background_data_callback_ = std::move(callback);

    // Process any existing data to make sure we don't miss any.
    for (const auto& chunk : data_chunks_)
      process_background_data_callback_.Run(chunk.decoded_data);
  }

  void Delete() const {
    DCHECK(IsMainThread());
    reader_task_runner_->DeleteSoon(FROM_HERE, this);
  }

  void FlushForTesting() {
    base::RunLoop run_loop;
    reader_task_runner_->PostTask(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  // BodyReader:
  bool ShouldContinueReading() override {
    // It's fine to keep reading unconditionally here because the main thread
    // will wait to process the data if loading is deferred.
    return true;
  }

  void FinishedReading(bool has_error) override {
    has_seen_end_of_data_ = true;
    AddChunk(decoder_->Flush(), nullptr, 0, has_error);
  }

  bool DataReceived(const char* data, size_t size) override {
    AddChunk(decoder_->Decode(data, size), data, size, /*has_error=*/false);
    return true;
  }

  void StartInBackground() {
    TRACE_EVENT0("loading", "OffThreadBodyReader::StartInBackground");
    DCHECK(reader_task_runner_->RunsTasksInCurrentSequence());
    response_body_watcher_ = std::make_unique<mojo::SimpleWatcher>(
        FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL);
    response_body_watcher_->Watch(
        response_body_.get(), MOJO_HANDLE_SIGNAL_READABLE,
        base::BindRepeating(&OffThreadBodyReader::ReadFromDataPipe,
                            base::Unretained(this)));
    ReadFromDataPipe(MOJO_RESULT_OK);
  }

  void ReadFromDataPipe(MojoResult unused) {
    TRACE_EVENT0("loading", "OffThreadBodyReader::ReadFromDataPipe");
    ReadFromDataPipeImpl(*this, response_body_, *response_body_watcher_);
  }

  void AddChunk(const String& decoded_data,
                const char* encoded_data,
                size_t size,
                bool has_error) {
    DCHECK(reader_task_runner_->RunsTasksInCurrentSequence());
    std::unique_ptr<char[]> encoded_data_copy;
    // Avoid copying the encoded data unless the caller needs it.
    if (should_keep_encoded_data_) {
      encoded_data_copy = std::make_unique<char[]>(size);
      memcpy(encoded_data_copy.get(), encoded_data, size);
    }

    bool post_task;
    {
      base::AutoLock lock(lock_);
      if (decoded_data && process_background_data_callback_)
        process_background_data_callback_.Run(decoded_data);

      // If |data_chunks_| is not empty, there is already a task posted which
      // will consume the data, so no need to post another one.
      post_task = data_chunks_.empty();
      data_chunks_.push_back(
          DataChunk{.decoded_data = decoded_data,
                    .has_seen_end_of_data = has_seen_end_of_data_,
                    .has_error = has_error,
                    .encoded_data = std::move(encoded_data_copy),
                    .encoded_data_size = size,
                    .encoding_data = decoder_->GetEncodingData()});
    }
    if (post_task) {
      PostCrossThreadTask(
          *main_thread_task_runner_, FROM_HERE,
          CrossThreadBindOnce(&NavigationBodyLoader::ProcessOffThreadData,
                              body_loader_));
    }
  }

  mojo::ScopedDataPipeConsumerHandle response_body_;
  std::unique_ptr<mojo::SimpleWatcher> response_body_watcher_;
  std::unique_ptr<BodyTextDecoder> decoder_;
  bool should_keep_encoded_data_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> reader_task_runner_;
  base::WeakPtr<NavigationBodyLoader> body_loader_;
  bool has_seen_end_of_data_ = false;

  base::Lock lock_;
  // This bool is used on the main thread to avoid locking when the callback has
  // already been set.
  bool background_callback_set_ = false;
  Client::ProcessBackgroundDataCallback process_background_data_callback_
      GUARDED_BY(lock_);
  Vector<DataChunk> data_chunks_ GUARDED_BY(lock_);
};

void NavigationBodyLoader::OffThreadBodyReaderDeleter::operator()(
    const OffThreadBodyReader* ptr) {
  if (ptr)
    ptr->Delete();
}

class NavigationBodyLoader::MainThreadBodyReader : public BodyReader {
 public:
  explicit MainThreadBodyReader(NavigationBodyLoader* loader)
      : loader_(loader) {}

  bool ShouldContinueReading() override {
    return loader_->freeze_mode_ == WebLoaderFreezeMode::kNone;
  }

  void FinishedReading(bool has_error) override {
    loader_->has_seen_end_of_data_ = true;
    if (has_error) {
      loader_->status_.error_code = net::ERR_FAILED;
      loader_->has_received_completion_ = true;
    }
    loader_->NotifyCompletionIfAppropriate();
  }

  bool DataReceived(const char* data, size_t size) override {
    base::WeakPtr<NavigationBodyLoader> weak_self =
        loader_->weak_factory_.GetWeakPtr();
    loader_->client_->BodyDataReceived(base::make_span(data, size));
    return weak_self.get();
  }

 private:
  NavigationBodyLoader* loader_;
};

NavigationBodyLoader::NavigationBodyLoader(
    const KURL& original_url,
    network::mojom::URLResponseHeadPtr response_head,
    mojo::ScopedDataPipeConsumerHandle response_body,
    network::mojom::URLLoaderClientEndpointsPtr endpoints,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    std::unique_ptr<ResourceLoadInfoNotifierWrapper>
        resource_load_info_notifier_wrapper)
    : response_head_(std::move(response_head)),
      response_body_(std::move(response_body)),
      endpoints_(std::move(endpoints)),
      task_runner_(std::move(task_runner)),
      handle_watcher_(FROM_HERE,
                      mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                      task_runner_),
      resource_load_info_notifier_wrapper_(
          std::move(resource_load_info_notifier_wrapper)),
      original_url_(original_url),
      should_send_directly_to_preload_scanner_(
          ShouldSendDirectlyToPreloadScanner()),
      max_data_to_process_per_task_(GetMaxDataToProcessPerTask()) {}

NavigationBodyLoader::~NavigationBodyLoader() {
  if (!has_received_completion_ || !has_seen_end_of_data_) {
    resource_load_info_notifier_wrapper_->NotifyResourceLoadCanceled(
        net::ERR_ABORTED);
  }
}

void NavigationBodyLoader::OnReceiveEarlyHints(
    network::mojom::EarlyHintsPtr early_hints) {
  // This has already happened in the browser process.
  NOTREACHED();
}

void NavigationBodyLoader::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr head,
    mojo::ScopedDataPipeConsumerHandle body,
    absl::optional<mojo_base::BigBuffer> cached_metadata) {
  // This has already happened in the browser process.
  NOTREACHED();
}

void NavigationBodyLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr head) {
  // This has already happened in the browser process.
  NOTREACHED();
}

void NavigationBodyLoader::OnUploadProgress(int64_t current_position,
                                            int64_t total_size,
                                            OnUploadProgressCallback callback) {
  // This has already happened in the browser process.
  NOTREACHED();
}

void NavigationBodyLoader::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  network::RecordOnTransferSizeUpdatedUMA(
      network::OnTransferSizeUpdatedFrom::kNavigationBodyLoader);
  resource_load_info_notifier_wrapper_->NotifyResourceTransferSizeUpdated(
      transfer_size_diff);
}

void NavigationBodyLoader::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  // Except for errors, there must always be a response's body.
  DCHECK(has_received_body_handle_ || status.error_code != net::OK);
  has_received_completion_ = true;
  status_ = status;
  NotifyCompletionIfAppropriate();
}

void NavigationBodyLoader::SetDefersLoading(WebLoaderFreezeMode mode) {
  if (freeze_mode_ == mode)
    return;
  freeze_mode_ = mode;
  if (handle_.is_valid())
    OnReadable(MOJO_RESULT_OK);
  else if (off_thread_body_reader_)
    ProcessOffThreadData();
}

void NavigationBodyLoader::StartLoadingBody(
    WebNavigationBodyLoader::Client* client) {
  TRACE_EVENT1("loading", "NavigationBodyLoader::StartLoadingBody", "url",
               original_url_.GetString().Utf8());
  client_ = client;

  resource_load_info_notifier_wrapper_->NotifyResourceResponseReceived(
      std::move(response_head_));
  base::WeakPtr<NavigationBodyLoader> weak_self = weak_factory_.GetWeakPtr();
  NotifyCompletionIfAppropriate();
  if (!weak_self)
    return;

  // TODO(dgozman): we should explore retrieveing code cache in parallel with
  // receiving response or reading the first data chunk.
  BindURLLoaderAndStartLoadingResponseBodyIfPossible();
}

void NavigationBodyLoader::StartLoadingBodyInBackground(
    std::unique_ptr<BodyTextDecoder> decoder,
    bool should_keep_encoded_data) {
  if (!response_body_)
    return;

  // Initializing the map used when detecting encodings is not thread safe.
  // Initialize on the main thread here to avoid races.
  // TODO(crbug.com/1384221): Consider making the map thread safe in
  // third_party/ced/src/util/encodings/encodings.cc.
  EncodingNameAliasToEncoding("");

  off_thread_body_reader_.reset(new OffThreadBodyReader(
      std::move(response_body_), std::move(decoder), weak_factory_.GetWeakPtr(),
      task_runner_, worker_pool::CreateSequencedTaskRunner({}),
      should_keep_encoded_data));
}

void NavigationBodyLoader::FlushOffThreadBodyReaderForTesting() {
  if (!off_thread_body_reader_)
    return;
  off_thread_body_reader_->FlushForTesting();
}

void NavigationBodyLoader::BindURLLoaderAndContinue() {
  url_loader_.Bind(std::move(endpoints_->url_loader), task_runner_);
  url_loader_client_receiver_.Bind(std::move(endpoints_->url_loader_client),
                                   task_runner_);
  url_loader_client_receiver_.set_disconnect_handler(base::BindOnce(
      &NavigationBodyLoader::OnConnectionClosed, base::Unretained(this)));
}

void NavigationBodyLoader::OnConnectionClosed() {
  // If the connection aborts before the load completes, mark it as failed.
  if (!has_received_completion_)
    OnComplete(network::URLLoaderCompletionStatus(net::ERR_FAILED));
}

void NavigationBodyLoader::OnReadable(MojoResult unused) {
  TRACE_EVENT1("loading", "NavigationBodyLoader::OnReadable", "url",
               original_url_.GetString().Utf8());
  if (has_seen_end_of_data_ || freeze_mode_ != WebLoaderFreezeMode::kNone ||
      is_in_on_readable_)
    return;
  // Protect against reentrancy:
  // - when the client calls SetDefersLoading;
  // - when nested message loop starts from BodyDataReceived
  //   and we get notified by the watcher.
  // Note: we cannot use AutoReset here since |this| may be deleted
  // before reset.
  is_in_on_readable_ = true;
  base::WeakPtr<NavigationBodyLoader> weak_self = weak_factory_.GetWeakPtr();
  ReadFromDataPipe();
  if (!weak_self)
    return;
  is_in_on_readable_ = false;
}

void NavigationBodyLoader::ProcessOffThreadData() {
  if (has_seen_end_of_data_ || freeze_mode_ != WebLoaderFreezeMode::kNone ||
      !client_) {
    return;
  }

  auto chunks =
      off_thread_body_reader_->TakeData(max_data_to_process_per_task_);
  auto weak_self = weak_factory_.GetWeakPtr();
  for (const auto& chunk : chunks) {
    client_->DecodedBodyDataReceived(
        chunk.decoded_data, chunk.encoding_data,
        base::make_span(chunk.encoded_data.get(), chunk.encoded_data_size));
    if (!weak_self)
      return;

    if (chunk.has_seen_end_of_data)
      has_seen_end_of_data_ = true;

    if (chunk.has_error) {
      status_.error_code = net::ERR_FAILED;
      has_received_completion_ = true;
      break;
    }
  }
  if (weak_self && should_send_directly_to_preload_scanner_)
    off_thread_body_reader_->StoreProcessBackgroundDataCallback(client_);

  NotifyCompletionIfAppropriate();
}

void NavigationBodyLoader::ReadFromDataPipe() {
  TRACE_EVENT1("loading", "NavigationBodyLoader::ReadFromDataPipe", "url",
               original_url_.GetString().Utf8());
  DCHECK(!off_thread_body_reader_);
  MainThreadBodyReader reader(this);
  ReadFromDataPipeImpl(reader, handle_, handle_watcher_);
}

void NavigationBodyLoader::NotifyCompletionIfAppropriate() {
  if (!has_received_completion_ || !has_seen_end_of_data_)
    return;

  handle_watcher_.Cancel();

  absl::optional<WebURLError> error;
  if (status_.error_code != net::OK) {
    error = WebURLError::Create(status_, original_url_);
  }

  resource_load_info_notifier_wrapper_->NotifyResourceLoadCompleted(status_);

  if (!client_)
    return;

  // |this| may be deleted after calling into client_, so clear it in advance.
  WebNavigationBodyLoader::Client* client = client_;
  client_ = nullptr;
  client->BodyLoadingFinished(
      status_.completion_time, status_.encoded_data_length,
      status_.encoded_body_length, status_.decoded_body_length,
      status_.should_report_corb_blocking, error);
}

void NavigationBodyLoader::
    BindURLLoaderAndStartLoadingResponseBodyIfPossible() {
  if (!response_body_ && !off_thread_body_reader_)
    return;
  // Bind the mojo::URLLoaderClient interface in advance, because we will start
  // to read from the data pipe immediately which may potentially postpone the
  // method calls from the remote. That causes the flakiness of some layout
  // tests.
  // TODO(minggang): The binding was executed after OnReceiveResponse
  // originally (prior to passing the response body from the browser process
  // during navigation), we should try to put it back if all the
  // webkit_layout_tests can pass in that way.
  BindURLLoaderAndContinue();

  DCHECK(!has_received_body_handle_);
  has_received_body_handle_ = true;

  if (off_thread_body_reader_) {
    ProcessOffThreadData();
    return;
  }

  DCHECK(response_body_.is_valid());

  DCHECK(!has_received_completion_);
  handle_ = std::move(response_body_);
  DCHECK(handle_.is_valid());
  handle_watcher_.Watch(handle_.get(), MOJO_HANDLE_SIGNAL_READABLE,
                        base::BindRepeating(&NavigationBodyLoader::OnReadable,
                                            base::Unretained(this)));
  OnReadable(MOJO_RESULT_OK);
  // Don't use |this| here as it might have been destroyed.
}

// static
void WebNavigationBodyLoader::FillNavigationParamsResponseAndBodyLoader(
    mojom::CommonNavigationParamsPtr common_params,
    mojom::CommitNavigationParamsPtr commit_params,
    int request_id,
    network::mojom::URLResponseHeadPtr response_head,
    mojo::ScopedDataPipeConsumerHandle response_body,
    network::mojom::URLLoaderClientEndpointsPtr url_loader_client_endpoints,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    std::unique_ptr<ResourceLoadInfoNotifierWrapper>
        resource_load_info_notifier_wrapper,
    bool is_main_frame,
    WebNavigationParams* navigation_params) {
  // Use the original navigation url to start with. We'll replay the
  // redirects afterwards and will eventually arrive to the final url.
  const KURL original_url = !commit_params->original_url.is_empty()
                                ? KURL(commit_params->original_url)
                                : KURL(common_params->url);
  KURL url = original_url;
  resource_load_info_notifier_wrapper->NotifyResourceLoadInitiated(
      request_id, GURL(url),
      !commit_params->original_method.empty() ? commit_params->original_method
                                              : common_params->method,
      common_params->referrer->url, common_params->request_destination,
      is_main_frame ? net::HIGHEST : net::LOWEST);
  size_t redirect_count = commit_params->redirect_response.size();

  if (redirect_count != commit_params->redirects.size()) {
    // We currently incorrectly send empty redirect_response and redirect_infos
    // on frame reloads and some cases involving throttles.
    // TODO(https://crbug.com/1171225): Fix this.
    DCHECK_EQ(0u, redirect_count);
    DCHECK_EQ(0u, commit_params->redirect_infos.size());
    DCHECK_NE(0u, commit_params->redirects.size());
  }
  navigation_params->redirects.reserve(redirect_count);
  navigation_params->redirects.resize(redirect_count);
  for (size_t i = 0; i < redirect_count; ++i) {
    WebNavigationParams::RedirectInfo& redirect =
        navigation_params->redirects[i];
    auto& redirect_info = commit_params->redirect_infos[i];
    auto& redirect_response = commit_params->redirect_response[i];
    redirect.redirect_response =
        WebURLResponse::Create(url, *redirect_response,
                               response_head->ssl_info.has_value(), request_id);
    resource_load_info_notifier_wrapper->NotifyResourceRedirectReceived(
        redirect_info, std::move(redirect_response));
    if (url.ProtocolIsData())
      redirect.redirect_response.SetHttpStatusCode(200);
    redirect.new_url = KURL(redirect_info.new_url);
    // WebString treats default and empty strings differently while std::string
    // does not. A default value is expected for new_referrer rather than empty.
    if (!redirect_info.new_referrer.empty())
      redirect.new_referrer = WebString::FromUTF8(redirect_info.new_referrer);
    redirect.new_referrer_policy = ReferrerUtils::NetToMojoReferrerPolicy(
        redirect_info.new_referrer_policy);
    redirect.new_http_method = WebString::FromLatin1(redirect_info.new_method);
    url = KURL(redirect_info.new_url);
  }

  navigation_params->response = WebURLResponse::Create(
      url, *response_head, response_head->ssl_info.has_value(), request_id);
  if (url.ProtocolIsData())
    navigation_params->response.SetHttpStatusCode(200);

  if (url_loader_client_endpoints) {
    navigation_params->body_loader.reset(new NavigationBodyLoader(
        original_url, std::move(response_head), std::move(response_body),
        std::move(url_loader_client_endpoints), task_runner,
        std::move(resource_load_info_notifier_wrapper)));
  }
}
}  // namespace blink
