// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DATA_DECODER_PUBLIC_CPP_SAFE_WEB_BUNDLE_PARSER_H_
#define SERVICES_DATA_DECODER_PUBLIC_CPP_SAFE_WEB_BUNDLE_PARSER_H_

#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace data_decoder {

// A class to wrap remote web_package::mojom::WebBundleParserFactory and
// web_package::mojom::WebBundleParser service.
//
// It is safe to delete this object from within the callbacks passed to its
// methods.
class SafeWebBundleParser {
 public:
  explicit SafeWebBundleParser(const absl::optional<GURL>& base_url);

  SafeWebBundleParser(const SafeWebBundleParser&) = delete;
  SafeWebBundleParser& operator=(const SafeWebBundleParser&) = delete;

  // Remaining callbacks on flight will be dropped.
  ~SafeWebBundleParser();

  // Binds |this| instance to the given |file| for subsequent parse calls.
  base::File::Error OpenFile(base::File file);

  // Binds |this| instance to the given |data_source| for subsequent parse
  // calls.
  void OpenDataSource(
      mojo::PendingRemote<web_package::mojom::BundleDataSource> data_source);

  // Parses the integrity block of a signed web bundle. See
  // web_package::mojom::WebBundleParser::ParseIntegrityBlock for
  // details. This method fails when it's called before the previous call
  // finishes.
  void ParseIntegrityBlock(
      web_package::mojom::WebBundleParser::ParseIntegrityBlockCallback
          callback);

  // Parses metadata. If `offset` is >= 0, then parsing of the web bundle starts
  // at that offset. See web_package::mojom::WebBundleParser::ParseMetadata for
  // details. This method fails when it's called before the previous call
  // finishes.
  void ParseMetadata(
      int64_t offset,
      web_package::mojom::WebBundleParser::ParseMetadataCallback callback);

  // Parses response. See web_package::mojom::WebBundleParser::ParseResponse for
  // details.
  void ParseResponse(
      uint64_t response_offset,
      uint64_t response_length,
      web_package::mojom::WebBundleParser::ParseResponseCallback callback);

  // Sets a callback to be called when the data_decoder service connection is
  // terminated.
  void SetDisconnectCallback(base::OnceClosure callback);

 private:
  web_package::mojom::WebBundleParserFactory* GetFactory();
  void OnDisconnect();
  void OnIntegrityBlockParsed(
      web_package::mojom::BundleIntegrityBlockPtr integrity_block,
      web_package::mojom::BundleIntegrityBlockParseErrorPtr error);
  void OnMetadataParsed(web_package::mojom::BundleMetadataPtr metadata,
                        web_package::mojom::BundleMetadataParseErrorPtr error);
  void OnResponseParsed(size_t callback_id,
                        web_package::mojom::BundleResponsePtr response,
                        web_package::mojom::BundleResponseParseErrorPtr error);

  absl::optional<GURL> base_url_;
  DataDecoder data_decoder_;
  mojo::Remote<web_package::mojom::WebBundleParserFactory> factory_;
  mojo::Remote<web_package::mojom::WebBundleParser> parser_;
  web_package::mojom::WebBundleParser::ParseIntegrityBlockCallback
      integrity_block_callback_;
  web_package::mojom::WebBundleParser::ParseMetadataCallback metadata_callback_;
  base::flat_map<size_t,
                 web_package::mojom::WebBundleParser::ParseResponseCallback>
      response_callbacks_;
  base::OnceClosure disconnect_callback_;
  size_t response_callback_next_id_ = 0;
  bool disconnected_ = true;
};

}  // namespace data_decoder

#endif  // SERVICES_DATA_DECODER_PUBLIC_CPP_SAFE_WEB_BUNDLE_PARSER_H_
