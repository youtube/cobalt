// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_ozone.h"

#include <algorithm>
#include <limits>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_metrics.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint_serializer.h"
#include "ui/base/data_transfer_policy/data_transfer_policy_controller.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/platform_clipboard.h"

namespace ui {

namespace {

// The amount of time to wait for a request to complete before aborting it.
constexpr base::TimeDelta kRequestTimeout = base::Seconds(1);

// Checks if DLP rules allow the clipboard read.
bool IsReadAllowed(const DataTransferEndpoint* data_src,
                   const DataTransferEndpoint* data_dst,
                   const base::span<uint8_t> data) {
  DataTransferPolicyController* policy_controller =
      DataTransferPolicyController::Get();

  if (!policy_controller || !data_src || data.empty())
    return true;

  bool is_allowed = policy_controller->IsClipboardReadAllowed(
      data_src, data_dst, data.size());
  return is_allowed;
}

// Depending on the backend, the platform clipboard may or may not be
// available.  Should it be absent, we provide a dummy one.  It always calls
// back immediately with empty data. It starts without ownership of any buffers
// but will take and keep ownership after a call to OfferClipboardData(). By
// taking ownership, we allow ClipboardOzone to return existing data in
// ReadClipboardDataAndWait().
class StubPlatformClipboard : public PlatformClipboard {
 public:
  StubPlatformClipboard() = default;
  ~StubPlatformClipboard() override = default;

  // PlatformClipboard:
  void OfferClipboardData(
      ClipboardBuffer buffer,
      const PlatformClipboard::DataMap& data_map,
      PlatformClipboard::OfferDataClosure callback) override {
    is_owner_[buffer] = true;
    std::move(callback).Run();
  }
  void RequestClipboardData(
      ClipboardBuffer buffer,
      const std::string& mime_type,
      PlatformClipboard::RequestDataClosure callback) override {
    std::move(callback).Run({});
  }
  void GetAvailableMimeTypes(
      ClipboardBuffer buffer,
      PlatformClipboard::GetMimeTypesClosure callback) override {
    std::move(callback).Run({});
  }
  bool IsSelectionOwner(ClipboardBuffer buffer) override {
    return is_owner_[buffer];
  }
  void SetClipboardDataChangedCallback(
      PlatformClipboard::ClipboardDataChangedCallback cb) override {}
  bool IsSelectionBufferAvailable() const override { return false; }

 private:
  base::flat_map<ClipboardBuffer, bool> is_owner_;
};

}  // namespace

// A helper class that uses a request pattern to asynchronously communicate
// with the ozone::PlatformClipboard and fetch clipboard data with the
// specified MIME types.
class ClipboardOzone::AsyncClipboardOzone {
 public:
  explicit AsyncClipboardOzone(PlatformClipboard* platform_clipboard,
                               ClipboardOzone* clipboard_ozone)
      : platform_clipboard_(platform_clipboard),
        clipboard_ozone_(clipboard_ozone),
        weak_factory_(this) {
    DCHECK(platform_clipboard_);

    // Set a callback to listen to requests to increase the clipboard sequence
    // number.
    auto update_sequence_cb =
        base::BindRepeating(&AsyncClipboardOzone::OnClipboardDataChanged,
                            weak_factory_.GetWeakPtr());
    platform_clipboard_->SetClipboardDataChangedCallback(
        std::move(update_sequence_cb));
  }
  AsyncClipboardOzone(const AsyncClipboardOzone&) = delete;
  AsyncClipboardOzone& operator=(const AsyncClipboardOzone&) = delete;
  ~AsyncClipboardOzone() = default;

  bool IsSelectionBufferAvailable() const {
    return platform_clipboard_->IsSelectionBufferAvailable();
  }

  base::span<uint8_t> ReadClipboardDataSetSourceAndWait(
      ClipboardBuffer buffer,
      const std::string& mime_type) {
    const auto data = ReadClipboardDataAndWait(buffer, mime_type);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    if (!data.empty()) {
      const auto data_src =
          ReadClipboardDataAndWait(buffer, kMimeTypeDataTransferEndpoint);
      std::string data_src_json = std::string(
          reinterpret_cast<char*>(data_src.data()), data_src.size());
      std::unique_ptr<DataTransferEndpoint> dte =
          ui::ConvertJsonToDataTransferEndpoint(data_src_json);
      if (dte)
        clipboard_ozone_->SetSource(buffer, std::move(dte));
    }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

    return data;
  }

  std::vector<std::string> RequestMimeTypes(ClipboardBuffer buffer) {
    if (buffer == ClipboardBuffer::kSelection && !IsSelectionBufferAvailable())
      return {};

    // We can use a fastpath if we are the owner of the selection.
    if (platform_clipboard_->IsSelectionOwner(buffer)) {
      std::vector<std::string> mime_types;
      for (const auto& item : offered_data_[buffer])
        mime_types.push_back(item.first);
      return mime_types;
    }

    return GetMimeTypes(buffer);
  }

  void PrepareForWriting() { data_to_offer_.clear(); }

  void OfferData(ClipboardBuffer buffer) {
    if (buffer == ClipboardBuffer::kSelection && !IsSelectionBufferAvailable())
      return;
    Offer(buffer, std::move(data_to_offer_));
  }

  void Clear(ClipboardBuffer buffer) {
    if (buffer == ClipboardBuffer::kSelection && !IsSelectionBufferAvailable())
      return;
    data_to_offer_.clear();
    OfferData(buffer);
  }

  void InsertData(std::vector<uint8_t> data,
                  const std::set<std::string>& mime_types) {
    auto wrapped_data = scoped_refptr<base::RefCountedBytes>(
        base::RefCountedBytes::TakeVector(&data));
    for (const auto& mime_type : mime_types) {
      DCHECK_EQ(data_to_offer_.count(mime_type), 0U);
      data_to_offer_[mime_type] = wrapped_data;
    }
  }

  const ClipboardSequenceNumberToken& GetSequenceNumber(
      ClipboardBuffer buffer) const {
    return buffer == ClipboardBuffer::kCopyPaste ? clipboard_sequence_number_
                                                 : selection_sequence_number_;
  }

 private:
  base::span<uint8_t> ReadClipboardDataAndWait(ClipboardBuffer buffer,
                                               const std::string& mime_type) {
    if (buffer == ClipboardBuffer::kSelection && !IsSelectionBufferAvailable())
      return {};

    // We can use a fastpath if we are the owner of the selection.
    if (platform_clipboard_->IsSelectionOwner(buffer)) {
      auto it = offered_data_[buffer].find(mime_type);
      if (it == offered_data_[buffer].end())
        return {};
      return base::make_span(it->second->front(), it->second->size());
    }

    if (auto data = Read(buffer, mime_type))
      return base::make_span(data->front(), data->size());

    return {};
  }

  // Request<Result> encapsulates a clipboard request and provides a sync-like
  // way to perform it, whereas Result is the operation return type. Request
  // instances are created by factory functions (e.g: Read, Offer, GetMimeTypes,
  // etc), which are supposed to know how to bind them to actual calls to the
  // underlying platform clipboard instance. Factory functions usually create
  // them as local vars (ie: stack memory), and bind them to weak references,
  // through GetWeakPtr(), plumbing them into Finish* functions, which prevents
  // use-after-free issues in case, for example, a platform clipboard callback
  // would run with an already destroyed request instance.
  template <typename Result>
  class Request {
   public:
    enum class State { kStarted, kDone, kAborted };

    // Blocks until the request is done or aborted. The |result_| is returned if
    // the request succeeds, otherwise an empty value is returned.
    Result TakeResultSync() {
      // For a variety of reasons, it might already be done at this point,
      // depending on the platform clipboard implementation and the specific
      // request (e.g: cached values, sync request, etc).
      if (state_ == State::kDone)
        return std::move(result_);

      DCHECK_EQ(state_, State::kStarted);

      // TODO(crbug.com/913422): this is known to be dangerous, and may cause
      // blocks in ui thread. But ui::Clipboard was designed with synchronous
      // APIs rather than asynchronous ones, which platform clipboards can
      // provide. E.g: X11 and Wayland.
      base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
      quit_closure_ = run_loop.QuitClosure();

      // Set a timeout timer after which the request will be aborted.
      base::OneShotTimer abort_timer;
      abort_timer.Start(FROM_HERE, kRequestTimeout,
                        base::BindOnce(&Request::Abort, GetWeakPtr()));

      run_loop.Run();
      return std::move(result_);
    }

    void Finish(const Result& result) {
      DCHECK_EQ(state_, State::kStarted);
      state_ = State::kDone;
      result_ = result;
      if (!quit_closure_.is_null())
        std::move(quit_closure_).Run();
    }

    base::WeakPtr<Request<Result>> GetWeakPtr() {
      return weak_factory_.GetWeakPtr();
    }

   private:
    void Abort() {
      DCHECK_EQ(state_, State::kStarted);
      Finish(Result{});
      state_ = State::kAborted;
    }

    // Keeps track of the request state.
    State state_ = State::kStarted;

    // Holds the sync loop quit closure.
    base::OnceClosure quit_closure_;

    // Stores the request result.
    Result result_ = {};

    base::WeakPtrFactory<Request<Result>> weak_factory_{this};
  };

  std::vector<std::string> GetMimeTypes(ClipboardBuffer buffer) {
    using MimeTypesRequest = Request<std::vector<std::string>>;
    MimeTypesRequest request;
    platform_clipboard_->GetAvailableMimeTypes(
        buffer,
        base::BindOnce(&MimeTypesRequest::Finish, request.GetWeakPtr()));
    return request.TakeResultSync();
  }

  PlatformClipboard::Data Read(ClipboardBuffer buffer,
                               const std::string& mime_type) {
    using ReadRequest = Request<PlatformClipboard::Data>;
    ReadRequest request;
    platform_clipboard_->RequestClipboardData(
        buffer, mime_type,
        base::BindOnce(&ReadRequest::Finish, request.GetWeakPtr()));
    return request.TakeResultSync().release();
  }

  void Offer(ClipboardBuffer buffer, PlatformClipboard::DataMap data_map) {
    using OfferRequest = Request<bool>;
    OfferRequest request;
    offered_data_[buffer] = data_map;
    platform_clipboard_->OfferClipboardData(
        buffer, data_map,
        base::BindOnce(&OfferRequest::Finish, request.GetWeakPtr(), true));
    request.TakeResultSync();
  }

  void OnClipboardDataChanged(ClipboardBuffer buffer) {
    DCHECK(buffer == ClipboardBuffer::kCopyPaste ||
           platform_clipboard_->IsSelectionBufferAvailable());
    if (buffer == ClipboardBuffer::kCopyPaste)
      clipboard_sequence_number_ = ClipboardSequenceNumberToken();
    else
      selection_sequence_number_ = ClipboardSequenceNumberToken();
    ClipboardMonitor::GetInstance()->NotifyClipboardDataChanged();
  }

  bool IsBufferSupported(ClipboardBuffer buffer) const {
    return buffer == ClipboardBuffer::kCopyPaste ||
           platform_clipboard_->IsSelectionBufferAvailable();
  }

  // Clipboard data accumulated for writing.
  PlatformClipboard::DataMap data_to_offer_;

  // Clipboard data that had been offered most recently.  Used as a cache to
  // read data if we still own it.
  base::flat_map<ClipboardBuffer, PlatformClipboard::DataMap> offered_data_;

  // Provides communication to a system clipboard under ozone level.
  const raw_ptr<PlatformClipboard> platform_clipboard_ = nullptr;

  // Reference to the ClipboardOzone object instantiating this
  // ClipboardOzone::AsyncClipboardOzone object. It is used to set
  // the correct source when some text is copied from Ash and pasted to Lacros.
  const raw_ptr<ClipboardOzone> clipboard_ozone_;

  ClipboardSequenceNumberToken clipboard_sequence_number_;
  ClipboardSequenceNumberToken selection_sequence_number_;

  base::WeakPtrFactory<AsyncClipboardOzone> weak_factory_;
};

// ClipboardOzone implementation.
ClipboardOzone::ClipboardOzone() {
  auto* platform_clipboard =
      OzonePlatform::GetInstance()->GetPlatformClipboard();

  if (platform_clipboard) {
    async_clipboard_ozone_ =
        std::make_unique<ClipboardOzone::AsyncClipboardOzone>(
            platform_clipboard, this);
  } else {
    static base::NoDestructor<StubPlatformClipboard> stub_platform_clipboard;
    async_clipboard_ozone_ =
        std::make_unique<ClipboardOzone::AsyncClipboardOzone>(
            stub_platform_clipboard.get(), this);
  }
}

ClipboardOzone::~ClipboardOzone() = default;

void ClipboardOzone::OnPreShutdown() {}

DataTransferEndpoint* ClipboardOzone::GetSource(ClipboardBuffer buffer) const {
  auto it = data_src_.find(buffer);
  return it == data_src_.end() ? nullptr : it->second.get();
}

const ClipboardSequenceNumberToken& ClipboardOzone::GetSequenceNumber(
    ClipboardBuffer buffer) const {
  return async_clipboard_ozone_->GetSequenceNumber(buffer);
}

bool ClipboardOzone::IsFormatAvailable(
    const ClipboardFormatType& format,
    ClipboardBuffer buffer,
    const DataTransferEndpoint* data_dst) const {
  DCHECK(CalledOnValidThread());

  if (!IsReadAllowed(GetSource(buffer), data_dst, base::span<uint8_t>()))
    return false;

  auto available_types = async_clipboard_ozone_->RequestMimeTypes(buffer);
  return base::Contains(available_types, format.GetName());
}

void ClipboardOzone::Clear(ClipboardBuffer buffer) {
  async_clipboard_ozone_->Clear(buffer);
  data_src_[buffer].reset();
}

std::vector<std::u16string> ClipboardOzone::GetStandardFormats(
    ClipboardBuffer buffer,
    const DataTransferEndpoint* data_dst) const {
  std::vector<std::u16string> types;
  auto available_types = async_clipboard_ozone_->RequestMimeTypes(buffer);
  for (const auto& mime_type : available_types) {
    if (mime_type == ClipboardFormatType::HtmlType().GetName() ||
        mime_type == ClipboardFormatType::SvgType().GetName() ||
        mime_type == ClipboardFormatType::RtfType().GetName() ||
        mime_type == ClipboardFormatType::BitmapType().GetName() ||
        mime_type == ClipboardFormatType::FilenamesType().GetName()) {
      types.push_back(base::UTF8ToUTF16(mime_type));
      continue;
    }
    // `WriteText` uses the following mime types for text, so if those types are
    // available, we add kMimeTypeText to the list.
    if ((mime_type == ClipboardFormatType::PlainTextType().GetName() ||
         mime_type == kMimeTypeLinuxText || mime_type == kMimeTypeLinuxString ||
         mime_type == kMimeTypeTextUtf8 ||
         mime_type == kMimeTypeLinuxUtf8String) &&
        !base::Contains(types, base::UTF8ToUTF16(kMimeTypeText))) {
      types.push_back(base::UTF8ToUTF16(kMimeTypeText));
      continue;
    }
  }
  return types;
}

void ClipboardOzone::ReadAvailableTypes(
    ClipboardBuffer buffer,
    const DataTransferEndpoint* data_dst,
    std::vector<std::u16string>* types) const {
  DCHECK(CalledOnValidThread());
  DCHECK(types);

  if (!IsReadAllowed(GetSource(buffer), data_dst, base::span<uint8_t>()))
    return;

  types->clear();

  for (const auto& mime_type : GetStandardFormats(buffer, data_dst)) {
    types->push_back(mime_type);
  }
  // Special handling for chromium/x-web-custom-data.
  // We must read the data and deserialize it to find the list
  // of mime types to report.
  if (IsFormatAvailable(ClipboardFormatType::WebCustomDataType(), buffer,
                        data_dst)) {
    auto data = async_clipboard_ozone_->ReadClipboardDataSetSourceAndWait(
        buffer, ClipboardFormatType::WebCustomDataType().GetName());
    ReadCustomDataTypes(data.data(), data.size(), types);
  }
}

void ClipboardOzone::ReadText(ClipboardBuffer buffer,
                              const DataTransferEndpoint* data_dst,
                              std::u16string* result) const {
  DCHECK(CalledOnValidThread());
  auto clipboard_data =
      async_clipboard_ozone_->ReadClipboardDataSetSourceAndWait(buffer,
                                                                kMimeTypeText);

  if (!IsReadAllowed(GetSource(buffer), data_dst, clipboard_data))
    return;

  RecordRead(ClipboardFormatMetric::kText);
  *result = base::UTF8ToUTF16(base::StringPiece(
      reinterpret_cast<char*>(clipboard_data.data()), clipboard_data.size()));
}

void ClipboardOzone::ReadAsciiText(ClipboardBuffer buffer,
                                   const DataTransferEndpoint* data_dst,
                                   std::string* result) const {
  DCHECK(CalledOnValidThread());

  auto clipboard_data =
      async_clipboard_ozone_->ReadClipboardDataSetSourceAndWait(buffer,
                                                                kMimeTypeText);

  if (!IsReadAllowed(GetSource(buffer), data_dst, clipboard_data))
    return;

  RecordRead(ClipboardFormatMetric::kText);
  result->assign(clipboard_data.begin(), clipboard_data.end());
}

void ClipboardOzone::ReadHTML(ClipboardBuffer buffer,
                              const DataTransferEndpoint* data_dst,
                              std::u16string* markup,
                              std::string* src_url,
                              uint32_t* fragment_start,
                              uint32_t* fragment_end) const {
  DCHECK(CalledOnValidThread());

  auto clipboard_data =
      async_clipboard_ozone_->ReadClipboardDataSetSourceAndWait(buffer,
                                                                kMimeTypeHTML);

  if (!IsReadAllowed(GetSource(buffer), data_dst, clipboard_data))
    return;

  RecordRead(ClipboardFormatMetric::kHtml);

  markup->clear();
  if (src_url)
    src_url->clear();
  *fragment_start = 0;
  *fragment_end = 0;

  *markup = base::UTF8ToUTF16(base::StringPiece(
      reinterpret_cast<char*>(clipboard_data.data()), clipboard_data.size()));
  DCHECK_LE(markup->length(), std::numeric_limits<uint32_t>::max());
  *fragment_end = static_cast<uint32_t>(markup->length());
}

void ClipboardOzone::ReadSvg(ClipboardBuffer buffer,
                             const DataTransferEndpoint* data_dst,
                             std::u16string* result) const {
  DCHECK(CalledOnValidThread());

  auto clipboard_data =
      async_clipboard_ozone_->ReadClipboardDataSetSourceAndWait(buffer,
                                                                kMimeTypeSvg);

  if (!IsReadAllowed(GetSource(buffer), data_dst, clipboard_data))
    return;

  RecordRead(ClipboardFormatMetric::kSvg);
  *result = base::UTF8ToUTF16(base::StringPiece(
      reinterpret_cast<char*>(clipboard_data.data()), clipboard_data.size()));
}

void ClipboardOzone::ReadRTF(ClipboardBuffer buffer,
                             const DataTransferEndpoint* data_dst,
                             std::string* result) const {
  DCHECK(CalledOnValidThread());

  auto clipboard_data =
      async_clipboard_ozone_->ReadClipboardDataSetSourceAndWait(buffer,
                                                                kMimeTypeRTF);

  if (!IsReadAllowed(GetSource(buffer), data_dst, clipboard_data))
    return;

  RecordRead(ClipboardFormatMetric::kRtf);
  result->assign(clipboard_data.begin(), clipboard_data.end());
}

void ClipboardOzone::ReadPng(ClipboardBuffer buffer,
                             const DataTransferEndpoint* data_dst,
                             ReadPngCallback callback) const {
  auto clipboard_data = ReadPngInternal(buffer);

  if (!IsReadAllowed(GetSource(buffer), data_dst, clipboard_data)) {
    std::move(callback).Run(std::vector<uint8_t>());
    return;
  }

  RecordRead(ClipboardFormatMetric::kPng);
  std::vector<uint8_t> png_data =
      std::vector<uint8_t>(clipboard_data.begin(), clipboard_data.end());
  std::move(callback).Run(png_data);
}

void ClipboardOzone::ReadCustomData(ClipboardBuffer buffer,
                                    const std::u16string& type,
                                    const DataTransferEndpoint* data_dst,
                                    std::u16string* result) const {
  DCHECK(CalledOnValidThread());

  auto custom_data = async_clipboard_ozone_->ReadClipboardDataSetSourceAndWait(
      buffer, kMimeTypeWebCustomData);

  if (!IsReadAllowed(GetSource(buffer), data_dst, custom_data))
    return;

  RecordRead(ClipboardFormatMetric::kCustomData);
  ReadCustomDataForType(custom_data.data(), custom_data.size(), type, result);
}

void ClipboardOzone::ReadFilenames(ClipboardBuffer buffer,
                                   const DataTransferEndpoint* data_dst,
                                   std::vector<ui::FileInfo>* result) const {
  DCHECK(CalledOnValidThread());

  auto clipboard_data =
      async_clipboard_ozone_->ReadClipboardDataSetSourceAndWait(
          buffer, kMimeTypeURIList);

  if (!IsReadAllowed(GetSource(buffer), data_dst, clipboard_data))
    return;

  RecordRead(ClipboardFormatMetric::kFilenames);
  std::string uri_list(clipboard_data.begin(), clipboard_data.end());
  *result = ui::URIListToFileInfos(uri_list);
}

void ClipboardOzone::ReadBookmark(const DataTransferEndpoint* data_dst,
                                  std::u16string* title,
                                  std::string* url) const {
  DCHECK(CalledOnValidThread());
  // TODO(msisov): This was left NOTIMPLEMENTED() in all the Linux platforms.
  // |data_dst| should be supported for DLP when ReadBookmark() is implemented.
  NOTIMPLEMENTED();
}

void ClipboardOzone::ReadData(const ClipboardFormatType& format,
                              const DataTransferEndpoint* data_dst,
                              std::string* result) const {
  DCHECK(CalledOnValidThread());

  auto clipboard_data =
      async_clipboard_ozone_->ReadClipboardDataSetSourceAndWait(
          ClipboardBuffer::kCopyPaste, format.GetName());

  if (!IsReadAllowed(GetSource(ClipboardBuffer::kCopyPaste), data_dst,
                     clipboard_data))
    return;

  RecordRead(ClipboardFormatMetric::kData);
  result->assign(clipboard_data.begin(), clipboard_data.end());
}

bool ClipboardOzone::IsSelectionBufferAvailable() const {
  return async_clipboard_ozone_->IsSelectionBufferAvailable();
}

void ClipboardOzone::WritePortableTextRepresentation(ClipboardBuffer buffer,
                                                     const ObjectMap& objects) {
  // Just like Non-Backed/X11 implementation does, copy text data from the
  // copy/paste selection to the primary selection.
  if (buffer == ClipboardBuffer::kCopyPaste && IsSelectionBufferAvailable()) {
    auto text_iter = objects.find(PortableFormat::kText);
    if (text_iter != objects.end() && !text_iter->second.data.empty()) {
      const auto& char_vector = text_iter->second.data[0];
      async_clipboard_ozone_->PrepareForWriting();
      if (!char_vector.empty())
        WriteText(&char_vector.front(), char_vector.size());
      async_clipboard_ozone_->OfferData(ClipboardBuffer::kSelection);
    }
  }
}

void ClipboardOzone::WritePortableAndPlatformRepresentations(
    ClipboardBuffer buffer,
    const ObjectMap& objects,
    std::vector<Clipboard::PlatformRepresentation> platform_representations,
    std::unique_ptr<DataTransferEndpoint> data_src) {
  DCHECK(CalledOnValidThread());

  async_clipboard_ozone_->PrepareForWriting();
  DispatchPlatformRepresentations(std::move(platform_representations));

  data_src_[buffer] = std::move(data_src);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  AddClipboardSourceToDataOffer(buffer);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  for (const auto& object : objects)
    DispatchPortableRepresentation(object.first, object.second);
  async_clipboard_ozone_->OfferData(buffer);

  WritePortableTextRepresentation(buffer, objects);
}

void ClipboardOzone::WriteText(const char* text_data, size_t text_len) {
  std::vector<uint8_t> data(text_data, text_data + text_len);
  async_clipboard_ozone_->InsertData(
      std::move(data), {kMimeTypeText, kMimeTypeLinuxText, kMimeTypeLinuxString,
                        kMimeTypeTextUtf8, kMimeTypeLinuxUtf8String});
}

void ClipboardOzone::WriteHTML(const char* markup_data,
                               size_t markup_len,
                               const char* url_data,
                               size_t url_len) {
  // `url_data` and `url_len` are not used in this platform.
  std::vector<uint8_t> data(markup_data, markup_data + markup_len);
  async_clipboard_ozone_->InsertData(std::move(data), {kMimeTypeHTML});
}

void ClipboardOzone::WriteUnsanitizedHTML(const char* markup_data,
                                          size_t markup_len,
                                          const char* url_data,
                                          size_t url_len) {
  WriteHTML(markup_data, markup_len, url_data, url_len);
}

void ClipboardOzone::WriteSvg(const char* markup_data, size_t markup_len) {
  std::vector<uint8_t> data(markup_data, markup_data + markup_len);
  async_clipboard_ozone_->InsertData(std::move(data), {kMimeTypeSvg});
}

void ClipboardOzone::WriteRTF(const char* rtf_data, size_t data_len) {
  std::vector<uint8_t> data(rtf_data, rtf_data + data_len);
  async_clipboard_ozone_->InsertData(std::move(data), {kMimeTypeRTF});
}

void ClipboardOzone::WriteFilenames(std::vector<ui::FileInfo> filenames) {
  std::string uri_list = ui::FileInfosToURIList(filenames);
  std::vector<uint8_t> data(uri_list.begin(), uri_list.end());
  async_clipboard_ozone_->InsertData(std::move(data), {kMimeTypeURIList});
}

void ClipboardOzone::WriteBookmark(const char* title_data,
                                   size_t title_len,
                                   const char* url_data,
                                   size_t url_len) {
  // Writes a Mozilla url (UTF16: URL, newline, title)
  std::u16string bookmark =
      base::UTF8ToUTF16(base::StringPiece(url_data, url_len)) + u"\n" +
      base::UTF8ToUTF16(base::StringPiece(title_data, title_len));

  std::vector<uint8_t> data(
      reinterpret_cast<const uint8_t*>(bookmark.data()),
      reinterpret_cast<const uint8_t*>(bookmark.data() + bookmark.size()));
  async_clipboard_ozone_->InsertData(std::move(data), {kMimeTypeMozillaURL});
}

void ClipboardOzone::WriteWebSmartPaste() {
  async_clipboard_ozone_->InsertData(std::vector<uint8_t>(),
                                     {kMimeTypeWebkitSmartPaste});
}

void ClipboardOzone::WriteBitmap(const SkBitmap& bitmap) {
  std::vector<unsigned char> output;
  if (gfx::PNGCodec::FastEncodeBGRASkBitmap(bitmap, false, &output))
    async_clipboard_ozone_->InsertData(std::move(output), {kMimeTypePNG});
}

void ClipboardOzone::WriteData(const ClipboardFormatType& format,
                               const char* data_data,
                               size_t data_len) {
  std::vector<uint8_t> data(data_data, data_data + data_len);
  async_clipboard_ozone_->InsertData(std::move(data), {format.GetName()});
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void ClipboardOzone::AddClipboardSourceToDataOffer(
    const ClipboardBuffer buffer) {
  DataTransferEndpoint* data_src = GetSource(buffer);

  if (!data_src)
    return;

  std::string dte_json = ConvertDataTransferEndpointToJson(*data_src);
  const char* dte_json_c_string = dte_json.c_str();
  std::vector<uint8_t> data(dte_json_c_string,
                            dte_json_c_string + dte_json.size());

  async_clipboard_ozone_->InsertData(std::move(data),
                                     {kMimeTypeDataTransferEndpoint});
}

void ClipboardOzone::SetSource(ClipboardBuffer buffer,
                               std::unique_ptr<DataTransferEndpoint> data_src) {
  data_src_[buffer] = std::move(data_src);
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

base::span<uint8_t> ClipboardOzone::ReadPngInternal(
    const ClipboardBuffer buffer) const {
  DCHECK(CalledOnValidThread());

  return async_clipboard_ozone_->ReadClipboardDataSetSourceAndWait(
      buffer, kMimeTypePNG);
}

}  // namespace ui
