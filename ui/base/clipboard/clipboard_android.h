// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_ANDROID_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_ANDROID_H_

#include "ui/base/clipboard/clipboard.h"

#include <jni.h>
#include <stddef.h>
#include <stdint.h>

#include "base/android/scoped_java_ref.h"
#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"

namespace ui {

// Documentation on the underlying Android API this ultimately abstracts is
// available at https://developer.android.com/guide/topics/text/copy-paste.
class ClipboardAndroid : public Clipboard {
 public:
  // Callback called whenever the clipboard is modified.  The parameter
  // represents the time of the modification.
  using ModifiedCallback = base::RepeatingCallback<void(base::Time)>;

  ClipboardAndroid(const ClipboardAndroid&) = delete;
  ClipboardAndroid& operator=(const ClipboardAndroid&) = delete;

  // Called by Java when the Java Clipboard is notified that the clipboard has
  // changed.
  void OnPrimaryClipChanged(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& obj);

  // Called by Java when the Java Clipboard is notified that the window focus
  // has changed. Since Chrome will not receive OnPrimaryClipChanged call from
  // Android if Chrome is in background,Clipboard handler needs to check the
  // content of clipboard didn't change, when Chrome is back in foreground.
  void OnPrimaryClipTimestampInvalidated(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const jlong j_timestamp_ms);

  // Called by Java side.
  int64_t GetLastModifiedTimeToJavaTime(JNIEnv* env);

  // Sets the callback called whenever the clipboard is modified.
  COMPONENT_EXPORT(UI_BASE_CLIPBOARD)
  void SetModifiedCallback(ModifiedCallback cb);

  // Sets the last modified time without calling the above callback.
  COMPONENT_EXPORT(UI_BASE_CLIPBOARD)
  void SetLastModifiedTimeWithoutRunningCallback(base::Time time);

 private:
  friend class Clipboard;

  ClipboardAndroid();
  ~ClipboardAndroid() override;

  // Clipboard overrides:
  void OnPreShutdown() override;
  DataTransferEndpoint* GetSource(ClipboardBuffer buffer) const override;
  const ClipboardSequenceNumberToken& GetSequenceNumber(
      ClipboardBuffer buffer) const override;
  std::vector<std::u16string> GetStandardFormats(
      ClipboardBuffer buffer,
      const DataTransferEndpoint* data_dst) const override;
  bool IsFormatAvailable(const ClipboardFormatType& format,
                         ClipboardBuffer buffer,
                         const DataTransferEndpoint* data_dst) const override;
  void Clear(ClipboardBuffer buffer) override;
  void ReadAvailableTypes(ClipboardBuffer buffer,
                          const DataTransferEndpoint* data_dst,
                          std::vector<std::u16string>* types) const override;
  void ReadText(ClipboardBuffer buffer,
                const DataTransferEndpoint* data_dst,
                std::u16string* result) const override;
  void ReadAsciiText(ClipboardBuffer buffer,
                     const DataTransferEndpoint* data_dst,
                     std::string* result) const override;
  void ReadHTML(ClipboardBuffer buffer,
                const DataTransferEndpoint* data_dst,
                std::u16string* markup,
                std::string* src_url,
                uint32_t* fragment_start,
                uint32_t* fragment_end) const override;
  void ReadSvg(ClipboardBuffer buffer,
               const DataTransferEndpoint* data_dst,
               std::u16string* result) const override;
  void ReadRTF(ClipboardBuffer buffer,
               const DataTransferEndpoint* data_dst,
               std::string* result) const override;
  void ReadPng(ClipboardBuffer buffer,
               const DataTransferEndpoint* data_dst,
               ReadPngCallback callback) const override;
  void ReadCustomData(ClipboardBuffer buffer,
                      const std::u16string& type,
                      const DataTransferEndpoint* data_dst,
                      std::u16string* result) const override;
  void ReadFilenames(ClipboardBuffer buffer,
                     const DataTransferEndpoint* data_dst,
                     std::vector<ui::FileInfo>* result) const override;
  void ReadBookmark(const DataTransferEndpoint* data_dst,
                    std::u16string* title,
                    std::string* url) const override;
  void ReadData(const ClipboardFormatType& format,
                const DataTransferEndpoint* data_dst,
                std::string* result) const override;
  base::Time GetLastModifiedTime() const override;
  void ClearLastModifiedTime() override;
  void WritePortableAndPlatformRepresentations(
      ClipboardBuffer buffer,
      const ObjectMap& objects,
      std::vector<Clipboard::PlatformRepresentation> platform_representations,
      std::unique_ptr<DataTransferEndpoint> data_src) override;
  void WriteText(const char* text_data, size_t text_len) override;
  void WriteHTML(const char* markup_data,
                 size_t markup_len,
                 const char* url_data,
                 size_t url_len) override;
  void WriteUnsanitizedHTML(const char* markup_data,
                            size_t markup_len,
                            const char* url_data,
                            size_t url_len) override;
  void WriteSvg(const char* markup_data, size_t markup_len) override;
  void WriteRTF(const char* rtf_data, size_t data_len) override;
  void WriteFilenames(std::vector<ui::FileInfo> filenames) override;
  void WriteBookmark(const char* title_data,
                     size_t title_len,
                     const char* url_data,
                     size_t url_len) override;
  void WriteWebSmartPaste() override;
  void WriteBitmap(const SkBitmap& bitmap) override;
  void WriteData(const ClipboardFormatType& format,
                 const char* data_data,
                 size_t data_len) override;
};

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_ANDROID_H_
