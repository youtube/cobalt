// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_exchange_data_provider.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/logging.h"
#include "base/pickle.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider.h"
#include "ui/base/dragdrop/os_exchange_data_provider_non_backed.h"
#include "ui/ozone/public/platform_clipboard.h"
#include "url/gurl.h"
#include "url/url_canon.h"
#include "url/url_util.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "ui/base/data_transfer_policy/data_transfer_endpoint_serializer.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace ui {

namespace {

constexpr FilenameToURLPolicy kFilenameToURLPolicy =
    FilenameToURLPolicy::CONVERT_FILENAMES;

// Converts mime type string to OSExchangeData::Format, if supported, otherwise
// 0 is returned.
int MimeTypeToFormat(const std::string& mime_type) {
  if (mime_type == ui::kMimeTypeText || mime_type == ui::kMimeTypeTextUtf8)
    return OSExchangeData::STRING;
  if (mime_type == ui::kMimeTypeURIList)
    return OSExchangeData::FILE_NAME;
  if (mime_type == ui::kMimeTypeMozillaURL)
    return OSExchangeData::URL;
  if (mime_type == ui::kMimeTypeHTML)
    return OSExchangeData::HTML;
  if (base::StartsWith(mime_type, ui::kMimeTypeOctetStream))
    return OSExchangeData::FILE_CONTENTS;
  if (mime_type == ui::kMimeTypeWebCustomData)
    return OSExchangeData::PICKLED_DATA;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (mime_type == ui::kMimeTypeDataTransferEndpoint)
    return OSExchangeData::DATA_TRANSFER_ENDPOINT;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  return 0;
}

// Converts raw data to either narrow or wide string.
template <typename StringType>
StringType BytesTo(PlatformClipboard::Data bytes) {
  using ValueType = typename StringType::value_type;
  if (bytes->size() % sizeof(ValueType) != 0U) {
    // This is suspicious.
    LOG(WARNING)
        << "Data is possibly truncated, or a wrong conversion is requested.";
  }

  StringType result(bytes->front_as<ValueType>(),
                    bytes->size() / sizeof(ValueType));
  return result;
}

void AddString(PlatformClipboard::Data data, OSExchangeDataProvider* provider) {
  DCHECK(provider);

  if (data->data().empty())
    return;

  provider->SetString(base::UTF8ToUTF16(BytesTo<std::string>(data)));
}

void AddHtml(PlatformClipboard::Data data, OSExchangeDataProvider* provider) {
  DCHECK(provider);

  if (data->data().empty())
    return;

  provider->SetHtml(base::UTF8ToUTF16(BytesTo<std::string>(data)), GURL());
}

// Parses |data| as if it had text/uri-list format.  Its brief spec is:
// 1.  Any lines beginning with the '#' character are comment lines.
// 2.  Non-comment lines shall be URIs (URNs or URLs).
// 3.  Lines are terminated with a CRLF pair.
// 4.  URL encoding is used.
void AddFiles(PlatformClipboard::Data data, OSExchangeDataProvider* provider) {
  DCHECK(provider);

  std::string data_as_string = BytesTo<std::string>(data);

  const auto lines = base::SplitString(
      data_as_string, "\r\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  std::vector<FileInfo> filenames;
  for (const auto& line : lines) {
    if (line.empty() || line[0] == '#')
      continue;
    GURL url(line);
    if (!url.is_valid() || !url.SchemeIsFile()) {
      LOG(WARNING) << "Invalid URI found: " << line;
      continue;
    }

    std::string url_path = url.path();
    url::RawCanonOutputT<char16_t> unescaped;
    url::DecodeURLEscapeSequences(url_path.data(), url_path.size(),
                                  url::DecodeURLMode::kUTF8OrIsomorphic,
                                  &unescaped);

    std::string path8;
    base::UTF16ToUTF8(unescaped.data(), unescaped.length(), &path8);
    const base::FilePath path(path8);
    filenames.emplace_back(path, path.BaseName());
  }
  if (filenames.empty())
    return;

  provider->SetFilenames(filenames);
}

// Parses |data| as if it had text/x-moz-url format, which is basically
// two lines separated with newline, where the first line is the URL and
// the second one is page title.  The unpleasant feature of text/x-moz-url is
// that the URL has UTF-16 encoding.
void AddUrl(PlatformClipboard::Data data, OSExchangeDataProvider* provider) {
  DCHECK(provider);

  if (data->data().empty())
    return;

  std::u16string data_as_string16 = BytesTo<std::u16string>(data);

  const auto lines =
      base::SplitString(data_as_string16, u"\r\n", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  if (lines.size() != 2U) {
    LOG(WARNING) << "Invalid data passed as text/x-moz-url; it must contain "
                 << "exactly 2 lines but has " << lines.size() << " instead.";
    return;
  }
  GURL url(lines[0]);
  if (!url.is_valid()) {
    LOG(WARNING) << "Invalid data passed as text/x-moz-url; the first line "
                 << "must contain a valid URL but it doesn't.";
    return;
  }

  provider->SetURL(url, lines[1]);
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// Parses |data| as if it was an encoded custom mime type DataTransferEndpoint.
// Used to synchronize the drag source metadata between Ash and Lacros.
void AddSource(PlatformClipboard::Data data, OSExchangeDataProvider* provider) {
  DCHECK(provider);

  if (data->data().empty())
    return;

  std::string source_dte = BytesTo<std::string>(data);
  provider->SetSource(ConvertJsonToDataTransferEndpoint(source_dte));
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace

WaylandExchangeDataProvider::WaylandExchangeDataProvider() = default;

WaylandExchangeDataProvider::~WaylandExchangeDataProvider() = default;

std::unique_ptr<OSExchangeDataProvider> WaylandExchangeDataProvider::Clone()
    const {
  auto clone = std::make_unique<WaylandExchangeDataProvider>();
  CopyData(clone.get());
  return clone;
}

std::vector<std::string> WaylandExchangeDataProvider::BuildMimeTypesList()
    const {
  // Drag'n'drop manuals usually suggest putting data in order so the more
  // specific a MIME type is, the earlier it occurs in the list.  Wayland
  // specs don't say anything like that, but here we follow that common
  // practice: begin with URIs and end with plain text.  Just in case.
  std::vector<std::string> mime_types;
  if (HasFile())
    mime_types.push_back(ui::kMimeTypeURIList);

  if (HasURL(kFilenameToURLPolicy))
    mime_types.push_back(ui::kMimeTypeMozillaURL);

  if (HasHtml())
    mime_types.push_back(ui::kMimeTypeHTML);

  if (HasString()) {
    mime_types.push_back(ui::kMimeTypeTextUtf8);
    mime_types.push_back(ui::kMimeTypeText);
  }

  if (HasFileContents()) {
    base::FilePath file_contents_filename;
    std::string file_contents;
    GetFileContents(&file_contents_filename, &file_contents);

    std::string filename = file_contents_filename.value();
    base::ReplaceChars(filename, "\\", "\\\\", &filename);
    base::ReplaceChars(filename, "\"", "\\\"", &filename);
    const std::string mime_type =
        base::StrCat({ui::kMimeTypeOctetStream, ";name=\"", filename, "\""});
    mime_types.push_back(mime_type);
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (GetSource() != nullptr) {
    mime_types.push_back(ui::kMimeTypeDataTransferEndpoint);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  for (auto item : pickle_data())
    mime_types.push_back(item.first.GetName());

  return mime_types;
}

// TODO(crbug.com/1236708): Support custom formats/pickled data.
void WaylandExchangeDataProvider::AddData(PlatformClipboard::Data data,
                                          const std::string& mime_type) {
  DCHECK(data);
  DCHECK(IsMimeTypeSupported(mime_type));
  int format = MimeTypeToFormat(mime_type);
  switch (format) {
    case OSExchangeData::STRING:
      AddString(data, this);
      break;
    case OSExchangeData::HTML:
      AddHtml(data, this);
      break;
    case OSExchangeData::URL:
      AddUrl(data, this);
      break;
    case OSExchangeData::FILE_NAME:
      AddFiles(data, this);
      break;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    case OSExchangeData::DATA_TRANSFER_ENDPOINT:
      AddSource(data, this);
      break;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  }
}

// TODO(crbug.com/1236708): Support custom formats/pickled data.
bool WaylandExchangeDataProvider::ExtractData(const std::string& mime_type,
                                              std::string* out_content) const {
  DCHECK(out_content);
  DCHECK(IsMimeTypeSupported(mime_type));
  if (mime_type == ui::kMimeTypeMozillaURL && HasURL(kFilenameToURLPolicy)) {
    GURL url;
    std::u16string title;
    GetURLAndTitle(kFilenameToURLPolicy, &url, &title);
    out_content->append(url.spec());
    return true;
  }
  if (mime_type == ui::kMimeTypeHTML && HasHtml()) {
    std::u16string data;
    GURL base_url;
    GetHtml(&data, &base_url);
    out_content->append(base::UTF16ToUTF8(data));
    return true;
  }
  if (base::StartsWith(mime_type, ui::kMimeTypeOctetStream) &&
      HasFileContents()) {
    base::FilePath filename;
    std::string file_contents;
    GetFileContents(&filename, &file_contents);
    out_content->append(file_contents);
    return true;
  }
  if (HasCustomFormat(ui::ClipboardFormatType::WebCustomDataType())) {
    base::Pickle pickle;
    GetPickledData(ui::ClipboardFormatType::WebCustomDataType(), &pickle);
    *out_content = std::string(reinterpret_cast<const char*>(pickle.data()),
                               pickle.size());
    return true;
  }
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (mime_type == ui::kMimeTypeDataTransferEndpoint &&
      GetSource() != nullptr) {
    DataTransferEndpoint* data_src = GetSource();
    out_content->append(ConvertDataTransferEndpointToJson(*data_src));
    return true;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  // Lastly, attempt to extract string data. Note: Keep this as the last
  // condition otherwise, for data maps that contain both string and custom
  // data, for example, it may result in subtle issues, such as,
  // https://crbug.com/1271311.
  if (HasString()) {
    std::u16string data;
    GetString(&data);
    out_content->append(base::UTF16ToUTF8(data));
    return true;
  }
  return false;
}

bool IsMimeTypeSupported(const std::string& mime_type) {
  return MimeTypeToFormat(mime_type) != 0;
}

}  // namespace ui
