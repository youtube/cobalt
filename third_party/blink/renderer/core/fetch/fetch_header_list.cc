// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/fetch_header_list.h"

#include <algorithm>
#include <utility>
#include "third_party/blink/renderer/platform/loader/cors/cors.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

FetchHeaderList* FetchHeaderList::Clone() const {
  auto* list = MakeGarbageCollected<FetchHeaderList>();
  for (const auto& header : header_list_)
    list->Append(header.first, header.second);
  return list;
}

FetchHeaderList::FetchHeaderList() {}

FetchHeaderList::~FetchHeaderList() {}

void FetchHeaderList::Append(const String& name, const String& value) {
  // https://fetch.spec.whatwg.org/#concept-header-list-append
  // "To append a name/value (|name|/|value|) pair to a header list (|list|),
  // run these steps:
  // 1. If |list| contains |name|, then set |name| to the first such header’s
  //    name. This reuses the casing of the name of the header already in the
  //    header list, if any. If there are multiple matched headers their names
  //    will all be identical.
  // 2. Append a new header whose name is |name| and |value| is |value| to
  //    |list|."
  auto header = header_list_.find(name);
  if (header != header_list_.end())
    header_list_.emplace(header->first, value);
  else
    header_list_.emplace(name, value);
}

void FetchHeaderList::Set(const String& name, const String& value) {
  // https://fetch.spec.whatwg.org/#concept-header-list-set
  // "To set a name/value (|name|/|value|) pair in a header list (|list|), run
  // these steps:
  // 1. If |list| contains |name|, then set the value of the first such header
  //    to |value| and remove the others.
  // 2. Otherwise, append a new header whose name is |name| and value is
  //    |value| to |list|."
  auto existingHeader = header_list_.find(name);
  const FetchHeaderList::Header newHeader = std::make_pair(
      existingHeader != header_list_.end() ? existingHeader->first : name,
      value);
  header_list_.erase(name);
  header_list_.insert(newHeader);
}

String FetchHeaderList::ExtractMIMEType() const {
  // To extract a MIME type from a header list (headers), run these steps:
  // 1. Let MIMEType be the result of parsing `Content-Type` in headers.
  String mime_type;
  if (!Get("Content-Type", mime_type)) {
    // 2. If MIMEType is null or failure, return the empty byte sequence.
    return String();
  }
  // 3. Return MIMEType, byte lowercased.
  return mime_type.LowerASCII();
}

size_t FetchHeaderList::size() const {
  return header_list_.size();
}

void FetchHeaderList::Remove(const String& name) {
  // https://fetch.spec.whatwg.org/#concept-header-list-delete
  // "To delete a name (name) from a header list (list), remove all headers
  // whose name is a byte-case-insensitive match for name from list."
  header_list_.erase(name);
}

bool FetchHeaderList::Get(const String& name, String& result) const {
  // https://fetch.spec.whatwg.org/#concept-header-list-combine
  // "To combine a name/value (|name|/|value|) pair in a header list (|list|),
  // run these steps:
  // 1. If |list| contains |name|, then set the value of the first such header
  //    to its value, followed by 0x2C 0x20, followed by |value|.
  // 2. Otherwise, append a new header whose name is |name| and value is
  //    |value| to |list|."
  StringBuilder resultBuilder;
  bool found = false;
  auto range = header_list_.equal_range(name);
  for (auto header = range.first; header != range.second; ++header) {
    if (!found) {
      resultBuilder.Append(header->second);
      found = true;
    } else {
      resultBuilder.Append(", ");
      resultBuilder.Append(header->second);
    }
  }
  if (found)
    result = resultBuilder.ToString();
  return found;
}

Vector<String> FetchHeaderList::GetSetCookie() const {
  // https://fetch.spec.whatwg.org/#dom-headers-getsetcookie
  // "The getSetCookie() method steps are:
  // 1. If this’s header list does not contain `Set-Cookie`, then return an
  //    empty list.
  // 2. Return the values of all headers in this’s header list whose name is a
  //    byte-case-insensitive match for `Set-Cookie`, in order."
  Vector<String> values;
  auto range = header_list_.equal_range("Set-Cookie");
  for (auto header = range.first; header != range.second; ++header) {
    values.push_back(header->second);
  }
  return values;
}

String FetchHeaderList::GetAsRawString(int status_code,
                                       String status_message) const {
  StringBuilder builder;
  builder.Append("HTTP/1.1 ");
  builder.AppendNumber(status_code);
  builder.Append(" ");
  builder.Append(status_message);
  builder.Append("\r\n");
  for (auto& it : header_list_) {
    builder.Append(it.first);
    builder.Append(":");
    builder.Append(it.second);
    builder.Append("\r\n");
  }
  return builder.ToString();
}

bool FetchHeaderList::Has(const String& name) const {
  // https://fetch.spec.whatwg.org/#header-list-contains
  // "A header list (|list|) contains a name (|name|) if |list| contains a
  // header whose name is a byte-case-insensitive match for |name|."
  return header_list_.find(name) != header_list_.end();
}

void FetchHeaderList::ClearList() {
  header_list_.clear();
}

Vector<FetchHeaderList::Header> FetchHeaderList::SortAndCombine() const {
  // https://fetch.spec.whatwg.org/#concept-header-list-sort-and-combine
  // "To sort and combine a header list (|list|), run these steps:
  // 1. Let |headers| be an empty list of headers with the key being the name
  //    and value the value.
  // 2. Let |names| be the result of convert header names to a sorted-lowercase
  //    set with all the names of the headers in |list|.
  // 3. For each |name| in |names|:
  //    1. If |name| is `set-cookie`, then:
  //       1. Let |values| be a list of all values of headers in |list| whose
  //          name is a byte-case-insensitive match for |name|, in order.
  //       2. For each |value| in |values|:
  //          1. Append (|name|, |value|) to |headers|.
  //    2. Otherwise:
  //       1. Let |value| be the result of getting |name| from |list|.
  //       2. Assert: |value| is not null.
  //       3. Append (|name|, |value|) to |headers|.
  // 4. Return |headers|."
  Vector<FetchHeaderList::Header> ret;
  for (auto it = header_list_.cbegin(); it != header_list_.cend();) {
    String header_name = it->first.LowerASCII();
    if (header_name == "set-cookie") {
      auto set_cookie_end = header_list_.upper_bound(header_name);
      for (; it != set_cookie_end; it++) {
        ret.emplace_back(header_name, it->second);
      }
      // At this point |it| is at the next distinct key.
    } else {
      String combined_value;
      Get(header_name, combined_value);
      ret.emplace_back(header_name, combined_value);
      // Skip to the next distinct key.
      it = header_list_.upper_bound(header_name);
    }
  }
  return ret;
}

bool FetchHeaderList::IsValidHeaderName(const String& name) {
  // "A name is a case-insensitive byte sequence that matches the field-name
  // token production."
  return IsValidHTTPToken(name);
}

bool FetchHeaderList::IsValidHeaderValue(const String& value) {
  // "A value is a byte sequence that matches the field-value token production
  // and contains no 0x0A or 0x0D bytes."
  return IsValidHTTPHeaderValue(value);
}

}  // namespace blink
