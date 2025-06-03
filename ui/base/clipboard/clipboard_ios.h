// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CLIPBOARD_CLIPBOARD_IOS_H_
#define UI_BASE_CLIPBOARD_CLIPBOARD_IOS_H_

#include "base/component_export.h"
#include "ui/base/clipboard/clipboard.h"

@class UIPasteboard;

namespace ui {

class COMPONENT_EXPORT(UI_BASE_CLIPBOARD) ClipboardIOS : public Clipboard {
 public:
  ClipboardIOS(const ClipboardIOS&) = delete;
  ClipboardIOS& operator=(const ClipboardIOS&) = delete;

 private:
  friend class Clipboard;

  ClipboardIOS();
  ~ClipboardIOS() override;

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

  void WritePortableAndPlatformRepresentations(
      ClipboardBuffer buffer,
      const ObjectMap& objects,
      std::vector<Clipboard::PlatformRepresentation> platform_representations,
      std::unique_ptr<DataTransferEndpoint> data_src) override;
  void WriteText(base::StringPiece text) override;
  void WriteHTML(base::StringPiece markup,
                 absl::optional<base::StringPiece> source_url) override;
  void WriteUnsanitizedHTML(
      base::StringPiece markup,
      absl::optional<base::StringPiece> source_url) override;
  void WriteSvg(base::StringPiece markup) override;
  void WriteRTF(base::StringPiece rtf) override;
  void WriteFilenames(std::vector<ui::FileInfo> filenames) override;
  void WriteBookmark(base::StringPiece title, base::StringPiece url) override;
  void WriteWebSmartPaste() override;
  void WriteBitmap(const SkBitmap& bitmap) override;
  void WriteData(const ClipboardFormatType& format,
                 base::span<const uint8_t> data) override;

  std::vector<uint8_t> ReadPngInternal(ClipboardBuffer buffer,
                                       UIPasteboard* pasteboard) const;

  // Mapping of OS-provided sequence number to a unique token.
  mutable struct {
    NSInteger sequence_number;
    ClipboardSequenceNumberToken token;
  } clipboard_sequence_;
};

}  // namespace ui

#endif  // UI_BASE_CLIPBOARD_CLIPBOARD_IOS_H_
