// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_STRUCTURED_HEADERS_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_STRUCTURED_HEADERS_MOJOM_TRAITS_H_

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/strings/string_piece.h"
#include "mojo/public/cpp/base/byte_string_mojom_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/cpp/bindings/union_traits.h"
#include "net/http/structured_headers.h"
#include "services/network/public/mojom/structured_headers.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_STRUCTURED_HEADERS)
    UnionTraits<network::mojom::StructuredHeadersItemDataView,
                net::structured_headers::Item> {
  static network::mojom::StructuredHeadersItemDataView::Tag GetTag(
      const net::structured_headers::Item&);

  static uint8_t null_value(const net::structured_headers::Item&) { return 0; }

  static int64_t integer_value(const net::structured_headers::Item& item) {
    return item.GetInteger();
  }

  static double decimal_value(const net::structured_headers::Item& item) {
    return item.GetDecimal();
  }

  static base::StringPiece string_value(
      const net::structured_headers::Item& item) {
    return item.GetString();
  }

  static base::StringPiece token_value(
      const net::structured_headers::Item& item) {
    return item.GetString();
  }

  static const std::string& byte_sequence_value(
      const net::structured_headers::Item& item) {
    return item.GetString();
  }

  static bool boolean_value(const net::structured_headers::Item& item) {
    return item.GetBoolean();
  }

  static bool Read(network::mojom::StructuredHeadersItemDataView,
                   net::structured_headers::Item* out);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_STRUCTURED_HEADERS)
    StructTraits<network::mojom::StructuredHeadersParameterDataView,
                 std::pair<std::string, net::structured_headers::Item>> {
  static base::StringPiece key(
      const std::pair<std::string, net::structured_headers::Item>& param) {
    return param.first;
  }

  static const net::structured_headers::Item& item(
      const std::pair<std::string, net::structured_headers::Item>& param) {
    return param.second;
  }

  static bool Read(network::mojom::StructuredHeadersParameterDataView,
                   std::pair<std::string, net::structured_headers::Item>* out);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_STRUCTURED_HEADERS)
    StructTraits<network::mojom::StructuredHeadersParameterizedItemDataView,
                 net::structured_headers::ParameterizedItem> {
  static const net::structured_headers::Item& item(
      const net::structured_headers::ParameterizedItem& item) {
    return item.item;
  }

  static const std::vector<
      std::pair<std::string, net::structured_headers::Item>>&
  parameters(const net::structured_headers::ParameterizedItem& item) {
    return item.params;
  }

  static bool Read(network::mojom::StructuredHeadersParameterizedItemDataView,
                   net::structured_headers::ParameterizedItem* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_STRUCTURED_HEADERS_MOJOM_TRAITS_H_
