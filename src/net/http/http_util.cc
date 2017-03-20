// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The rules for parsing content-types were borrowed from Firefox:
// http://lxr.mozilla.org/mozilla/source/netwerk/base/src/nsURLHelper.cpp#834

#include "net/http/http_util.h"

#include <algorithm>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/time.h"

using std::string;

namespace net {

//-----------------------------------------------------------------------------

// Return the index of the closing quote of the string, if any.
static size_t FindStringEnd(const string& line, size_t start, char delim) {
  DCHECK(start < line.length() && line[start] == delim &&
         (delim == '"' || delim == '\''));

  const char set[] = { delim, '\\', '\0' };
  for (;;) {
    // start points to either the start quote or the last
    // escaped char (the char following a '\\')

    size_t end = line.find_first_of(set, start + 1);
    if (end == string::npos)
      return line.length();

    if (line[end] == '\\') {
      // Hit a backslash-escaped char.  Need to skip over it.
      start = end + 1;
      if (start == line.length())
        return start;

      // Go back to looking for the next escape or the string end
      continue;
    }

    return end;
  }

  NOTREACHED();
  return line.length();
}

//-----------------------------------------------------------------------------

// static
size_t HttpUtil::FindDelimiter(const string& line, size_t search_start,
                               char delimiter) {
  do {
    // search_start points to the spot from which we should start looking
    // for the delimiter.
    const char delim_str[] = { delimiter, '"', '\'', '\0' };
    size_t cur_delim_pos = line.find_first_of(delim_str, search_start);
    if (cur_delim_pos == string::npos)
      return line.length();

    char ch = line[cur_delim_pos];
    if (ch == delimiter) {
      // Found delimiter
      return cur_delim_pos;
    }

    // We hit the start of a quoted string.  Look for its end.
    search_start = FindStringEnd(line, cur_delim_pos, ch);
    if (search_start == line.length())
      return search_start;

    ++search_start;

    // search_start now points to the first char after the end of the
    // string, so just go back to the top of the loop and look for
    // |delimiter| again.
  } while (true);

  NOTREACHED();
  return line.length();
}

// static
void HttpUtil::ParseContentType(const string& content_type_str,
                                string* mime_type,
                                string* charset,
                                bool* had_charset,
                                string* boundary) {
  const string::const_iterator begin = content_type_str.begin();

  // Trim leading and trailing whitespace from type.  We include '(' in
  // the trailing trim set to catch media-type comments, which are not at all
  // standard, but may occur in rare cases.
  size_t type_val = content_type_str.find_first_not_of(HTTP_LWS);
  type_val = std::min(type_val, content_type_str.length());
  size_t type_end = content_type_str.find_first_of(HTTP_LWS ";(", type_val);
  if (string::npos == type_end)
    type_end = content_type_str.length();

  size_t charset_val = 0;
  size_t charset_end = 0;
  bool type_has_charset = false;

  // Iterate over parameters
  size_t param_start = content_type_str.find_first_of(';', type_end);
  if (param_start != string::npos) {
    StringTokenizer tokenizer(begin + param_start, content_type_str.end(),
                              ";");
    tokenizer.set_quote_chars("\"");
    while (tokenizer.GetNext()) {
      string::const_iterator equals_sign =
          std::find(tokenizer.token_begin(), tokenizer.token_end(), '=');
      if (equals_sign == tokenizer.token_end())
        continue;

      string::const_iterator param_name_begin = tokenizer.token_begin();
      string::const_iterator param_name_end = equals_sign;
      TrimLWS(&param_name_begin, &param_name_end);

      string::const_iterator param_value_begin = equals_sign + 1;
      string::const_iterator param_value_end = tokenizer.token_end();
      DCHECK(param_value_begin <= tokenizer.token_end());
      TrimLWS(&param_value_begin, &param_value_end);

      if (LowerCaseEqualsASCII(param_name_begin, param_name_end, "charset")) {
        // TODO(abarth): Refactor this function to consistently use iterators.
        charset_val = param_value_begin - begin;
        charset_end = param_value_end - begin;
        type_has_charset = true;
      } else if (LowerCaseEqualsASCII(param_name_begin, param_name_end,
                                      "boundary")) {
        if (boundary)
          boundary->assign(param_value_begin, param_value_end);
      }
    }
  }

  if (type_has_charset) {
    // Trim leading and trailing whitespace from charset_val.  We include
    // '(' in the trailing trim set to catch media-type comments, which are
    // not at all standard, but may occur in rare cases.
    charset_val = content_type_str.find_first_not_of(HTTP_LWS, charset_val);
    charset_val = std::min(charset_val, charset_end);
    char first_char = content_type_str[charset_val];
    if (first_char == '"' || first_char == '\'') {
      charset_end = FindStringEnd(content_type_str, charset_val, first_char);
      ++charset_val;
      DCHECK(charset_end >= charset_val);
    } else {
      charset_end = std::min(content_type_str.find_first_of(HTTP_LWS ";(",
                                                            charset_val),
                             charset_end);
    }
  }

  // if the server sent "*/*", it is meaningless, so do not store it.
  // also, if type_val is the same as mime_type, then just update the
  // charset.  however, if charset is empty and mime_type hasn't
  // changed, then don't wipe-out an existing charset.  We
  // also want to reject a mime-type if it does not include a slash.
  // some servers give junk after the charset parameter, which may
  // include a comma, so this check makes us a bit more tolerant.
  if (content_type_str.length() != 0 &&
      content_type_str != "*/*" &&
      content_type_str.find_first_of('/') != string::npos) {
    // Common case here is that mime_type is empty
    bool eq = !mime_type->empty() && LowerCaseEqualsASCII(begin + type_val,
                                                          begin + type_end,
                                                          mime_type->data());
    if (!eq) {
      mime_type->assign(begin + type_val, begin + type_end);
      StringToLowerASCII(mime_type);
    }
    if ((!eq && *had_charset) || type_has_charset) {
      *had_charset = true;
      charset->assign(begin + charset_val, begin + charset_end);
      StringToLowerASCII(charset);
    }
  }
}

// static
// Parse the Range header according to RFC 2616 14.35.1
// ranges-specifier = byte-ranges-specifier
// byte-ranges-specifier = bytes-unit "=" byte-range-set
// byte-range-set  = 1#( byte-range-spec | suffix-byte-range-spec )
// byte-range-spec = first-byte-pos "-" [last-byte-pos]
// first-byte-pos  = 1*DIGIT
// last-byte-pos   = 1*DIGIT
bool HttpUtil::ParseRanges(const std::string& headers,
                           std::vector<HttpByteRange>* ranges) {
  std::string ranges_specifier;
  HttpUtil::HeadersIterator it(headers.begin(), headers.end(), "\r\n");

  while (it.GetNext()) {
    // Look for "Range" header.
    if (!LowerCaseEqualsASCII(it.name(), "range"))
      continue;
    ranges_specifier = it.values();
    // We just care about the first "Range" header, so break here.
    break;
  }

  if (ranges_specifier.empty())
    return false;

  return ParseRangeHeader(ranges_specifier, ranges);
}

// static
bool HttpUtil::ParseRangeHeader(const std::string& ranges_specifier,
                                std::vector<HttpByteRange>* ranges) {
  size_t equal_char_offset = ranges_specifier.find('=');
  if (equal_char_offset == std::string::npos)
    return false;

  // Try to extract bytes-unit part.
  std::string::const_iterator bytes_unit_begin = ranges_specifier.begin();
  std::string::const_iterator bytes_unit_end = bytes_unit_begin +
                                               equal_char_offset;
  std::string::const_iterator byte_range_set_begin = bytes_unit_end + 1;
  std::string::const_iterator byte_range_set_end = ranges_specifier.end();

  TrimLWS(&bytes_unit_begin, &bytes_unit_end);
  // "bytes" unit identifier is not found.
  if (!LowerCaseEqualsASCII(bytes_unit_begin, bytes_unit_end, "bytes"))
    return false;

  ValuesIterator byte_range_set_iterator(byte_range_set_begin,
                                         byte_range_set_end, ',');
  while (byte_range_set_iterator.GetNext()) {
    size_t minus_char_offset = byte_range_set_iterator.value().find('-');
    // If '-' character is not found, reports failure.
    if (minus_char_offset == std::string::npos)
      return false;

    std::string::const_iterator first_byte_pos_begin =
        byte_range_set_iterator.value_begin();
    std::string::const_iterator first_byte_pos_end =
        first_byte_pos_begin +  minus_char_offset;
    TrimLWS(&first_byte_pos_begin, &first_byte_pos_end);
    std::string first_byte_pos(first_byte_pos_begin, first_byte_pos_end);

    HttpByteRange range;
    // Try to obtain first-byte-pos.
    if (!first_byte_pos.empty()) {
      int64 first_byte_position = -1;
      if (!base::StringToInt64(first_byte_pos, &first_byte_position))
        return false;
      range.set_first_byte_position(first_byte_position);
    }

    std::string::const_iterator last_byte_pos_begin =
        byte_range_set_iterator.value_begin() + minus_char_offset + 1;
    std::string::const_iterator last_byte_pos_end =
        byte_range_set_iterator.value_end();
    TrimLWS(&last_byte_pos_begin, &last_byte_pos_end);
    std::string last_byte_pos(last_byte_pos_begin, last_byte_pos_end);

    // We have last-byte-pos or suffix-byte-range-spec in this case.
    if (!last_byte_pos.empty()) {
      int64 last_byte_position;
      if (!base::StringToInt64(last_byte_pos, &last_byte_position))
        return false;
      if (range.HasFirstBytePosition())
        range.set_last_byte_position(last_byte_position);
      else
        range.set_suffix_length(last_byte_position);
    } else if (!range.HasFirstBytePosition()) {
      return false;
    }

    // Do a final check on the HttpByteRange object.
    if (!range.IsValid())
      return false;
    ranges->push_back(range);
  }
  return !ranges->empty();
}

// static
bool HttpUtil::HasHeader(const std::string& headers, const char* name) {
  size_t name_len = strlen(name);
  string::const_iterator it =
      std::search(headers.begin(),
                  headers.end(),
                  name,
                  name + name_len,
                  base::CaseInsensitiveCompareASCII<char>());
  if (it == headers.end())
    return false;

  // ensure match is prefixed by newline
  if (it != headers.begin() && it[-1] != '\n')
    return false;

  // ensure match is suffixed by colon
  if (it + name_len >= headers.end() || it[name_len] != ':')
    return false;

  return true;
}

namespace {
// A header string containing any of the following fields will cause
// an error. The list comes from the XMLHttpRequest standard.
// http://www.w3.org/TR/XMLHttpRequest/#the-setrequestheader-method
const char* const kForbiddenHeaderFields[] = {
  "accept-charset",
  "accept-encoding",
  "access-control-request-headers",
  "access-control-request-method",
  "connection",
  "content-length",
  "cookie",
  "cookie2",
  "content-transfer-encoding",
  "date",
  "dnt",
  "expect",
  "host",
  "keep-alive",
  "origin",
  "referer",
  "te",
  "trailer",
  "transfer-encoding",
  "upgrade",
  "user-agent",
  "via",
};
}  // anonymous namespace

// static
bool HttpUtil::IsSafeHeader(const std::string& name) {
  std::string lower_name(StringToLowerASCII(name));
  if (StartsWithASCII(lower_name, "proxy-", true) ||
      StartsWithASCII(lower_name, "sec-", true))
    return false;
  for (size_t i = 0; i < arraysize(kForbiddenHeaderFields); ++i) {
    if (lower_name == kForbiddenHeaderFields[i])
      return false;
  }
  return true;
}

// static
std::string HttpUtil::StripHeaders(const std::string& headers,
                                   const char* const headers_to_remove[],
                                   size_t headers_to_remove_len) {
  std::string stripped_headers;
  net::HttpUtil::HeadersIterator it(headers.begin(), headers.end(), "\r\n");

  while (it.GetNext()) {
    bool should_remove = false;
    for (size_t i = 0; i < headers_to_remove_len; ++i) {
      if (LowerCaseEqualsASCII(it.name_begin(), it.name_end(),
                               headers_to_remove[i])) {
        should_remove = true;
        break;
      }
    }
    if (!should_remove) {
      // Assume that name and values are on the same line.
      stripped_headers.append(it.name_begin(), it.values_end());
      stripped_headers.append("\r\n");
    }
  }
  return stripped_headers;
}

// static
bool HttpUtil::IsNonCoalescingHeader(string::const_iterator name_begin,
                                     string::const_iterator name_end) {
  // NOTE: "set-cookie2" headers do not support expires attributes, so we don't
  // have to list them here.
  const char* kNonCoalescingHeaders[] = {
    "date",
    "expires",
    "last-modified",
    "location",  // See bug 1050541 for details
    "retry-after",
    "set-cookie",
    // The format of auth-challenges mixes both space separated tokens and
    // comma separated properties, so coalescing on comma won't work.
    "www-authenticate",
    "proxy-authenticate",
    // STS specifies that UAs must not process any STS headers after the first
    // one.
    "strict-transport-security"
  };
  for (size_t i = 0; i < arraysize(kNonCoalescingHeaders); ++i) {
    if (LowerCaseEqualsASCII(name_begin, name_end, kNonCoalescingHeaders[i]))
      return true;
  }
  return false;
}

bool HttpUtil::IsLWS(char c) {
  return strchr(HTTP_LWS, c) != NULL;
}

void HttpUtil::TrimLWS(string::const_iterator* begin,
                       string::const_iterator* end) {
  // leading whitespace
  while (*begin < *end && IsLWS((*begin)[0]))
    ++(*begin);

  // trailing whitespace
  while (*begin < *end && IsLWS((*end)[-1]))
    --(*end);
}

bool HttpUtil::IsQuote(char c) {
  // Single quote mark isn't actually part of quoted-text production,
  // but apparently some servers rely on this.
  return c == '"' || c == '\'';
}

bool HttpUtil::IsTokenChar(char c) {
  return !(c >= 0x7F || c <= 0x20 || c == '(' || c == ')' || c == '<' ||
           c == '>' || c == '@' || c == ',' || c == ';' || c == ':' ||
           c == '\\' || c == '"' || c == '/' || c == '[' || c == ']' ||
           c == '?' || c == '=' || c == '{' || c == '}');
}

// See RFC 2616 Sec 2.2 for the definition of |token|.
bool HttpUtil::IsToken(string::const_iterator begin,
                       string::const_iterator end) {
  if (begin == end)
    return false;
  for (std::string::const_iterator iter = begin; iter != end; ++iter) {
    unsigned char c = *iter;
    if (c >= 0x80 || c <= 0x1F || c == 0x7F ||
        c == '(' || c == ')' || c == '<' || c == '>' || c == '@' ||
        c == ',' || c == ';' || c == ':' || c == '\\' || c == '"' ||
        c == '/' || c == '[' || c == ']' || c == '?' || c == '=' ||
        c == '{' || c == '}' || c == ' ' || c == '\t')
      return false;
  }
  return true;
}

std::string HttpUtil::Unquote(std::string::const_iterator begin,
                              std::string::const_iterator end) {
  // Empty string
  if (begin == end)
    return std::string();

  // Nothing to unquote.
  if (!IsQuote(*begin))
    return std::string(begin, end);

  // No terminal quote mark.
  if (end - begin < 2 || *begin != *(end - 1))
    return std::string(begin, end);

  // Strip quotemarks
  ++begin;
  --end;

  // Unescape quoted-pair (defined in RFC 2616 section 2.2)
  std::string unescaped;
  bool prev_escape = false;
  for (; begin != end; ++begin) {
    char c = *begin;
    if (c == '\\' && !prev_escape) {
      prev_escape = true;
      continue;
    }
    prev_escape = false;
    unescaped.push_back(c);
  }
  return unescaped;
}

// static
std::string HttpUtil::Unquote(const std::string& str) {
  return Unquote(str.begin(), str.end());
}

// static
std::string HttpUtil::Quote(const std::string& str) {
  std::string escaped;
  escaped.reserve(2 + str.size());

  std::string::const_iterator begin = str.begin();
  std::string::const_iterator end = str.end();

  // Esape any backslashes or quotemarks within the string, and
  // then surround with quotes.
  escaped.push_back('"');
  for (; begin != end; ++begin) {
    char c = *begin;
    if (c == '"' || c == '\\')
      escaped.push_back('\\');
    escaped.push_back(c);
  }
  escaped.push_back('"');
  return escaped;
}

// Find the "http" substring in a status line. This allows for
// some slop at the start. If the "http" string could not be found
// then returns -1.
// static
int HttpUtil::LocateStartOfStatusLine(const char* buf, int buf_len) {
  const int slop = 4;
  const int http_len = 4;

  if (buf_len >= http_len) {
    int i_max = std::min(buf_len - http_len, slop);
    for (int i = 0; i <= i_max; ++i) {
      if (LowerCaseEqualsASCII(buf + i, buf + i + http_len, "http"))
        return i;
    }
  }
  return -1;  // Not found
}

int HttpUtil::LocateEndOfHeaders(const char* buf, int buf_len, int i) {
  bool was_lf = false;
  char last_c = '\0';
  for (; i < buf_len; ++i) {
    char c = buf[i];
    if (c == '\n') {
      if (was_lf)
        return i + 1;
      was_lf = true;
    } else if (c != '\r' || last_c != '\n') {
      was_lf = false;
    }
    last_c = c;
  }
  return -1;
}

// In order for a line to be continuable, it must specify a
// non-blank header-name. Line continuations are specifically for
// header values -- do not allow headers names to span lines.
static bool IsLineSegmentContinuable(const char* begin, const char* end) {
  if (begin == end)
    return false;

  const char* colon = std::find(begin, end, ':');
  if (colon == end)
    return false;

  const char* name_begin = begin;
  const char* name_end = colon;

  // Name can't be empty.
  if (name_begin == name_end)
    return false;

  // Can't start with LWS (this would imply the segment is a continuation)
  if (HttpUtil::IsLWS(*name_begin))
    return false;

  return true;
}

// Helper used by AssembleRawHeaders, to find the end of the status line.
static const char* FindStatusLineEnd(const char* begin, const char* end) {
  size_t i = base::StringPiece(begin, end - begin).find_first_of("\r\n");
  if (i == base::StringPiece::npos)
    return end;
  return begin + i;
}

// Helper used by AssembleRawHeaders, to skip past leading LWS.
static const char* FindFirstNonLWS(const char* begin, const char* end) {
  for (const char* cur = begin; cur != end; ++cur) {
    if (!HttpUtil::IsLWS(*cur))
      return cur;
  }
  return end;  // Not found.
}

std::string HttpUtil::AssembleRawHeaders(const char* input_begin,
                                         int input_len) {
  std::string raw_headers;
  raw_headers.reserve(input_len);

  const char* input_end = input_begin + input_len;

  // Skip any leading slop, since the consumers of this output
  // (HttpResponseHeaders) don't deal with it.
  int status_begin_offset = LocateStartOfStatusLine(input_begin, input_len);
  if (status_begin_offset != -1)
    input_begin += status_begin_offset;

  // Copy the status line.
  const char* status_line_end = FindStatusLineEnd(input_begin, input_end);
  raw_headers.append(input_begin, status_line_end);

  // After the status line, every subsequent line is a header line segment.
  // Should a segment start with LWS, it is a continuation of the previous
  // line's field-value.

  // TODO(ericroman): is this too permissive? (delimits on [\r\n]+)
  CStringTokenizer lines(status_line_end, input_end, "\r\n");

  // This variable is true when the previous line was continuable.
  bool prev_line_continuable = false;

  while (lines.GetNext()) {
    const char* line_begin = lines.token_begin();
    const char* line_end = lines.token_end();

    if (prev_line_continuable && IsLWS(*line_begin)) {
      // Join continuation; reduce the leading LWS to a single SP.
      raw_headers.push_back(' ');
      raw_headers.append(FindFirstNonLWS(line_begin, line_end), line_end);
    } else {
      // Terminate the previous line.
      raw_headers.push_back('\n');

      // Copy the raw data to output.
      raw_headers.append(line_begin, line_end);

      // Check if the current line can be continued.
      prev_line_continuable = IsLineSegmentContinuable(line_begin, line_end);
    }
  }

  raw_headers.append("\n\n", 2);

  // Use '\0' as the canonical line terminator. If the input already contained
  // any embeded '\0' characters we will strip them first to avoid interpreting
  // them as line breaks.
  raw_headers.erase(std::remove(raw_headers.begin(), raw_headers.end(), '\0'),
                    raw_headers.end());
  std::replace(raw_headers.begin(), raw_headers.end(), '\n', '\0');

  return raw_headers;
}

std::string HttpUtil::ConvertHeadersBackToHTTPResponse(const std::string& str) {
  std::string disassembled_headers;
  StringTokenizer tokenizer(str, std::string(1, '\0'));
  while (tokenizer.GetNext()) {
    disassembled_headers.append(tokenizer.token_begin(), tokenizer.token_end());
    disassembled_headers.append("\r\n");
  }
  disassembled_headers.append("\r\n");

  return disassembled_headers;
}

// TODO(jungshik): 1. If the list is 'fr-CA,fr-FR,en,de', we have to add
// 'fr' after 'fr-CA' with the same q-value as 'fr-CA' because
// web servers, in general, do not fall back to 'fr' and may end up picking
// 'en' which has a lower preference than 'fr-CA' and 'fr-FR'.
// 2. This function assumes that the input is a comma separated list
// without any whitespace. As long as it comes from the preference and
// a user does not manually edit the preference file, it's the case. Still,
// we may have to make it more robust.
std::string HttpUtil::GenerateAcceptLanguageHeader(
    const std::string& raw_language_list) {
  // We use integers for qvalue and qvalue decrement that are 10 times
  // larger than actual values to avoid a problem with comparing
  // two floating point numbers.
  const unsigned int kQvalueDecrement10 = 2;
  unsigned int qvalue10 = 10;
  StringTokenizer t(raw_language_list, ",");
  std::string lang_list_with_q;
  while (t.GetNext()) {
    std::string language = t.token();
    if (qvalue10 == 10) {
      // q=1.0 is implicit.
      lang_list_with_q = language;
    } else {
      DCHECK_LT(qvalue10, 10U);
      base::StringAppendF(&lang_list_with_q, ",%s;q=0.%d", language.c_str(),
                          qvalue10);
    }
    // It does not make sense to have 'q=0'.
    if (qvalue10 > kQvalueDecrement10)
      qvalue10 -= kQvalueDecrement10;
  }
  return lang_list_with_q;
}

std::string HttpUtil::GenerateAcceptCharsetHeader(const std::string& charset) {
  std::string charset_with_q = charset;
  if (LowerCaseEqualsASCII(charset, "utf-8")) {
    charset_with_q += ",*;q=0.5";
  } else {
    charset_with_q += ",utf-8;q=0.7,*;q=0.3";
  }
  return charset_with_q;
}

void HttpUtil::AppendHeaderIfMissing(const char* header_name,
                                     const std::string& header_value,
                                     std::string* headers) {
  if (header_value.empty())
    return;
  if (net::HttpUtil::HasHeader(*headers, header_name))
    return;
  *headers += std::string(header_name) + ": " + header_value + "\r\n";
}

bool HttpUtil::HasStrongValidators(HttpVersion version,
                                   const std::string& etag_header,
                                   const std::string& last_modified_header,
                                   const std::string& date_header) {
  if (version < HttpVersion(1, 1))
    return false;

  if (!etag_header.empty()) {
    size_t slash = etag_header.find('/');
    if (slash == std::string::npos || slash == 0)
      return true;

    std::string::const_iterator i = etag_header.begin();
    std::string::const_iterator j = etag_header.begin() + slash;
    TrimLWS(&i, &j);
    if (!LowerCaseEqualsASCII(i, j, "w"))
      return true;
  }

  base::Time last_modified;
  if (!base::Time::FromString(last_modified_header.c_str(), &last_modified))
    return false;

  base::Time date;
  if (!base::Time::FromString(date_header.c_str(), &date))
    return false;

  return ((date - last_modified).InSeconds() >= 60);
}

// Functions for histogram initialization.  The code 0 is put in the map to
// track status codes that are invalid.
// TODO(gavinp): Greatly prune the collected codes once we learn which
// ones are not sent in practice, to reduce upload size & memory use.

enum {
  HISTOGRAM_MIN_HTTP_STATUS_CODE = 100,
  HISTOGRAM_MAX_HTTP_STATUS_CODE = 599,
};

// static
std::vector<int> HttpUtil::GetStatusCodesForHistogram() {
  std::vector<int> codes;
  codes.reserve(
      HISTOGRAM_MAX_HTTP_STATUS_CODE - HISTOGRAM_MIN_HTTP_STATUS_CODE + 2);
  codes.push_back(0);
  for (int i = HISTOGRAM_MIN_HTTP_STATUS_CODE;
       i <= HISTOGRAM_MAX_HTTP_STATUS_CODE; ++i)
    codes.push_back(i);
  return codes;
}

// static
int HttpUtil::MapStatusCodeForHistogram(int code) {
  if (HISTOGRAM_MIN_HTTP_STATUS_CODE <= code &&
      code <= HISTOGRAM_MAX_HTTP_STATUS_CODE)
    return code;
  return 0;
}

// BNF from section 4.2 of RFC 2616:
//
//   message-header = field-name ":" [ field-value ]
//   field-name     = token
//   field-value    = *( field-content | LWS )
//   field-content  = <the OCTETs making up the field-value
//                     and consisting of either *TEXT or combinations
//                     of token, separators, and quoted-string>
//

HttpUtil::HeadersIterator::HeadersIterator(string::const_iterator headers_begin,
                                           string::const_iterator headers_end,
                                           const std::string& line_delimiter)
    : lines_(headers_begin, headers_end, line_delimiter) {
}

HttpUtil::HeadersIterator::~HeadersIterator() {
}

bool HttpUtil::HeadersIterator::GetNext() {
  while (lines_.GetNext()) {
    name_begin_ = lines_.token_begin();
    values_end_ = lines_.token_end();

    string::const_iterator colon = std::find(name_begin_, values_end_, ':');
    if (colon == values_end_)
      continue;  // skip malformed header

    name_end_ = colon;

    // If the name starts with LWS, it is an invalid line.
    // Leading LWS implies a line continuation, and these should have
    // already been joined by AssembleRawHeaders().
    if (name_begin_ == name_end_ || IsLWS(*name_begin_))
      continue;

    TrimLWS(&name_begin_, &name_end_);
    if (name_begin_ == name_end_)
      continue;  // skip malformed header

    values_begin_ = colon + 1;
    TrimLWS(&values_begin_, &values_end_);

    // if we got a header name, then we are done.
    return true;
  }
  return false;
}

bool HttpUtil::HeadersIterator::AdvanceTo(const char* name) {
  DCHECK(name != NULL);
  DCHECK_EQ(0, StringToLowerASCII<std::string>(name).compare(name))
      << "the header name must be in all lower case";

  while (GetNext()) {
    if (LowerCaseEqualsASCII(name_begin_, name_end_, name)) {
      return true;
    }
  }

  return false;
}

HttpUtil::ValuesIterator::ValuesIterator(
    string::const_iterator values_begin,
    string::const_iterator values_end,
    char delimiter)
    : values_(values_begin, values_end, string(1, delimiter)) {
  values_.set_quote_chars("\'\"");
}

HttpUtil::ValuesIterator::~ValuesIterator() {
}

bool HttpUtil::ValuesIterator::GetNext() {
  while (values_.GetNext()) {
    value_begin_ = values_.token_begin();
    value_end_ = values_.token_end();
    TrimLWS(&value_begin_, &value_end_);

    // bypass empty values.
    if (value_begin_ != value_end_)
      return true;
  }
  return false;
}

HttpUtil::NameValuePairsIterator::NameValuePairsIterator(
    string::const_iterator begin,
    string::const_iterator end,
    char delimiter)
    : props_(begin, end, delimiter),
      valid_(true),
      name_begin_(end),
      name_end_(end),
      value_begin_(end),
      value_end_(end),
      value_is_quoted_(false) {
}

HttpUtil::NameValuePairsIterator::~NameValuePairsIterator() {}

// We expect properties to be formatted as one of:
//   name="value"
//   name='value'
//   name='\'value\''
//   name=value
//   name = value
//   name=
// Due to buggy implementations found in some embedded devices, we also
// accept values with missing close quotemark (http://crbug.com/39836):
//   name="value
bool HttpUtil::NameValuePairsIterator::GetNext() {
  if (!props_.GetNext())
    return false;

  // Set the value as everything. Next we will split out the name.
  value_begin_ = props_.value_begin();
  value_end_ = props_.value_end();
  name_begin_ = name_end_ = value_end_;

  // Scan for the equals sign.
  std::string::const_iterator equals = std::find(value_begin_, value_end_, '=');
  if (equals == value_end_ || equals == value_begin_)
    return valid_ = false;  // Malformed, no equals sign

  // Verify that the equals sign we found wasn't inside of quote marks.
  for (std::string::const_iterator it = value_begin_; it != equals; ++it) {
    if (HttpUtil::IsQuote(*it))
      return valid_ = false;  // Malformed, quote appears before equals sign
  }

  name_begin_ = value_begin_;
  name_end_ = equals;
  value_begin_ = equals + 1;

  TrimLWS(&name_begin_, &name_end_);
  TrimLWS(&value_begin_, &value_end_);
  value_is_quoted_ = false;
  unquoted_value_.clear();

  if (value_begin_ == value_end_)
    return valid_ = false;  // Malformed, value is empty

  if (HttpUtil::IsQuote(*value_begin_)) {
    // Trim surrounding quotemarks off the value
    if (*value_begin_ != *(value_end_ - 1) || value_begin_ + 1 == value_end_) {
      // NOTE: This is not as graceful as it sounds:
      // * quoted-pairs will no longer be unquoted
      //   (["\"hello] should give ["hello]).
      // * Does not detect when the final quote is escaped
      //   (["value\"] should give [value"])
      ++value_begin_;  // Gracefully recover from mismatching quotes.
    } else {
      value_is_quoted_ = true;
      // Do not store iterators into this. See declaration of unquoted_value_.
      unquoted_value_ = HttpUtil::Unquote(value_begin_, value_end_);
    }
  }

  return true;
}

}  // namespace net
