// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_MOCK_CLIPBOARD_HOST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_MOCK_CLIPBOARD_HOST_H_

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/clipboard/clipboard.mojom-blink.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace blink {

class MockClipboardHost : public mojom::blink::ClipboardHost {
 public:
  MockClipboardHost();
  MockClipboardHost(const MockClipboardHost&) = delete;
  MockClipboardHost& operator=(const MockClipboardHost&) = delete;
  ~MockClipboardHost() override;

  void Bind(mojo::PendingReceiver<mojom::blink::ClipboardHost> receiver);
  // Clears all clipboard data.
  void ResetForTesting();

  // These write methods exist only in the mock class because
  // mojom::ClipboardHost does not provide equivalent methods.  These are here
  // to simplify testing of the system clipboard.
  void WriteRtfForTesting(const String& rtf_text);
  void WriteFilesForTesting(mojom::blink::ClipboardFilesPtr files);

  // Method to simulate clipboard data change only for testing.
  void OnClipboardDataChangedForTesting();

  // Force a format to be advertised in ReadStandardFormatNames() even when
  // the corresponding data is empty. Used to test reader behavior when the
  // OS clipboard advertises a format but returns no data.
  void AddFormatWithoutDataForTesting(const String& mime_type) {
    if (!std::ranges::contains(forced_formats_for_testing_, mime_type)) {
      forced_formats_for_testing_.push_back(mime_type);
    }
  }

#if BUILDFLAG(IS_MAC)
  // Test helper to configure the permission state returned by the mock
  void SetPlatformPermissionStateForTesting(
      mojom::blink::PlatformClipboardPermissionState state) {
    platform_permission_state_for_testing_ = state;
  }
#endif

  // Installs a hook fired during ReadAvailableCustomAndStandardFormats(),
  // after format enumeration but before the Mojo reply, so tests can simulate
  // a clipboard change inside the async race window. See crbug.com/498411773.
  void SetReadAvailableFormatsHookForTesting(
      base::RepeatingCallback<void(MockClipboardHost*)> hook) {
    read_available_formats_hook_for_testing_ = std::move(hook);
  }

  // Method call tracking for testing
  int ReadTextCallCountForTesting() const {
    return read_text_call_count_for_testing_;
  }
  int ReadHtmlCallCountForTesting() const {
    return read_html_call_count_for_testing_;
  }
  int ReadAvailableFormatsCallCountForTesting() const {
    return read_available_formats_call_count_for_testing_;
  }
  mojom::ClipboardBuffer LastReadPngBuffer() const {
    return last_read_png_buffer_;
  }

  // Test helpers used to simulate a slow OS clipboard read so callers can
  // verify that renderer-side Async Clipboard read paths are truly
  // non-blocking. When deferred mode is enabled, the next ReadText() call
  // stashes its reply callback instead of invoking it immediately. The
  // stashed callback can be invoked later via
  // RunDeferredReadTextCallbackForTesting().
  // See crbug.com/474131935.
  void SetReadTextCallbackDeferredForTesting(bool deferred) {
    defer_read_text_callback_for_testing_ = deferred;
  }
  bool HasDeferredReadTextCallbackForTesting() const {
    return !deferred_read_text_callback_for_testing_.is_null();
  }
  void RunDeferredReadTextCallbackForTesting();

 private:
  FRIEND_TEST_ALL_PREFIXES(ClipboardTest,
                           ClipboardChangeDuringReadRejectsGetType);

  // mojom::ClipboardHost
  void GetSequenceNumber(mojom::ClipboardBuffer clipboard_buffer,
                         GetSequenceNumberCallback callback) override;
  void IsFormatAvailable(mojom::ClipboardFormat format,
                         mojom::ClipboardBuffer clipboard_buffer,
                         IsFormatAvailableCallback callback) override;
  void ReadAvailableTypes(mojom::ClipboardBuffer clipboard_buffer,
                          ReadAvailableTypesCallback callback) override;
  void ReadText(mojom::ClipboardBuffer clipboard_buffer,
                ReadTextCallback callback) override;
  void ReadHtml(mojom::ClipboardBuffer clipboard_buffer,
                ReadHtmlCallback callback) override;
  void ReadSvg(mojom::ClipboardBuffer clipboard_buffer,
               ReadSvgCallback callback) override;
  void ReadRtf(mojom::ClipboardBuffer clipboard_buffer,
               ReadRtfCallback callback) override;
  void ReadPng(mojom::ClipboardBuffer clipboard_buffer,
               ReadPngCallback callback) override;
  void ReadFiles(mojom::ClipboardBuffer clipboard_buffer,
                 ReadFilesCallback callback) override;
  void ReadDataTransferCustomData(
      mojom::ClipboardBuffer clipboard_buffer,
      const String& type,
      ReadDataTransferCustomDataCallback callback) override;
  void WriteText(const String& text) override;
  void CommitWrite() override;
  void WriteHtml(const String& markup, const KURL& url) override;
  void WriteSvg(const String& markup) override;
  void WriteSmartPasteMarker() override;
  void WriteDataTransferCustomData(
      const HashMap<String, String>& data) override;
  void WriteBookmark(const String& url, const String& title) override;
  void WriteImage(const SkBitmap& bitmap) override;
  void ReadAvailableCustomAndStandardFormats(
      ReadAvailableCustomAndStandardFormatsCallback callback) override;
  void ReadUnsanitizedCustomFormat(
      const String& format,
      ReadUnsanitizedCustomFormatCallback callback) override;
  void WriteUnsanitizedCustomFormat(const String& format,
                                    mojo_base::BigBuffer data) override;
  void RegisterClipboardListener(
      mojo::PendingRemote<mojom::blink::ClipboardListener> listener) override;
#if BUILDFLAG(IS_MAC)
  void WriteStringToFindPboard(const String& text) override;
  void GetPlatformPermissionState(
      GetPlatformPermissionStateCallback callback) override;
#endif
  Vector<String> ReadStandardFormatNames();

  mojo::ReceiverSet<mojom::blink::ClipboardHost> receivers_;
  mojo::Remote<mojom::blink::ClipboardListener> clipboard_listener_;
  ClipboardSequenceNumberToken sequence_number_;
  String plain_text_ = g_empty_string;
  String html_text_ = g_empty_string;
  String svg_text_ = g_empty_string;
  String rtf_text_ = g_empty_string;
  mojom::blink::ClipboardFilesPtr files_ = mojom::blink::ClipboardFiles::New();
  KURL url_;
  Vector<uint8_t> png_;
  // TODO(asully): Remove `image_` once ReadImage() path is removed.
  SkBitmap image_;
  HashMap<String, String> custom_data_;
  bool write_smart_paste_ = false;
  bool needs_reset_ = false;
  Vector<String> forced_formats_for_testing_;
  HashMap<String, Vector<uint8_t>> unsanitized_custom_data_map_;
#if BUILDFLAG(IS_MAC)
  mojom::blink::PlatformClipboardPermissionState
      platform_permission_state_for_testing_ =
          mojom::blink::PlatformClipboardPermissionState::kAsk;
#endif

  // Method call tracking
  int read_text_call_count_for_testing_ = 0;
  int read_html_call_count_for_testing_ = 0;
  int read_available_formats_call_count_for_testing_ = 0;

  // Hook fired during ReadAvailableCustomAndStandardFormats() for TOCTOU
  // race regression tests.
  base::RepeatingCallback<void(MockClipboardHost*)>
      read_available_formats_hook_for_testing_;

  // Deferred-callback machinery for the truly-async-read regression test.
  bool defer_read_text_callback_for_testing_ = false;
  ReadTextCallback deferred_read_text_callback_for_testing_;

  mojom::ClipboardBuffer last_read_png_buffer_ =
      mojom::ClipboardBuffer::kStandard;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_MOCK_CLIPBOARD_HOST_H_
