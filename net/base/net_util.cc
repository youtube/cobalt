// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_util.h"

#include <algorithm>
#include <map>
#include <unicode/ucnv.h>
#include <unicode/uidna.h>
#include <unicode/ulocdata.h>
#include <unicode/uniset.h>
#include <unicode/uscript.h>
#include <unicode/uset.h>

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <wspiapi.h>  // Needed for Win2k compat.
#elif defined(OS_POSIX)
#include <netdb.h>
#include <sys/socket.h>
#include <fcntl.h>
#endif

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/i18n/file_util_icu.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/i18n/time_formatting.h"
#include "base/json/string_escape.h"
#include "base/lock.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/singleton.h"
#include "base/stl_util-inl.h"
#include "base/string_piece.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "grit/net_resources.h"
#include "googleurl/src/gurl.h"
#include "googleurl/src/url_canon.h"
#include "googleurl/src/url_parse.h"
#include "net/base/escape.h"
#include "net/base/net_module.h"
#if defined(OS_WIN)
#include "net/base/winsock_init.h"
#endif
#include "net/base/base64.h"
#include "unicode/datefmt.h"


using base::Time;

namespace {

// what we prepend to get a file URL
static const FilePath::CharType kFileURLPrefix[] =
    FILE_PATH_LITERAL("file:///");

// The general list of blocked ports. Will be blocked unless a specific
// protocol overrides it. (Ex: ftp can use ports 20 and 21)
static const int kRestrictedPorts[] = {
  1,    // tcpmux
  7,    // echo
  9,    // discard
  11,   // systat
  13,   // daytime
  15,   // netstat
  17,   // qotd
  19,   // chargen
  20,   // ftp data
  21,   // ftp access
  22,   // ssh
  23,   // telnet
  25,   // smtp
  37,   // time
  42,   // name
  43,   // nicname
  53,   // domain
  77,   // priv-rjs
  79,   // finger
  87,   // ttylink
  95,   // supdup
  101,  // hostriame
  102,  // iso-tsap
  103,  // gppitnp
  104,  // acr-nema
  109,  // pop2
  110,  // pop3
  111,  // sunrpc
  113,  // auth
  115,  // sftp
  117,  // uucp-path
  119,  // nntp
  123,  // NTP
  135,  // loc-srv /epmap
  139,  // netbios
  143,  // imap2
  179,  // BGP
  389,  // ldap
  465,  // smtp+ssl
  512,  // print / exec
  513,  // login
  514,  // shell
  515,  // printer
  526,  // tempo
  530,  // courier
  531,  // chat
  532,  // netnews
  540,  // uucp
  556,  // remotefs
  563,  // nntp+ssl
  587,  // stmp?
  601,  // ??
  636,  // ldap+ssl
  993,  // ldap+ssl
  995,  // pop3+ssl
  2049, // nfs
  3659, // apple-sasl / PasswordServer
  4045, // lockd
  6000, // X11
};

// FTP overrides the following restricted ports.
static const int kAllowedFtpPorts[] = {
  21,   // ftp data
  22,   // ssh
};

template<typename STR>
STR GetSpecificHeaderT(const STR& headers, const STR& name) {
  // We want to grab the Value from the "Key: Value" pairs in the headers,
  // which should look like this (no leading spaces, \n-separated) (we format
  // them this way in url_request_inet.cc):
  //    HTTP/1.1 200 OK\n
  //    ETag: "6d0b8-947-24f35ec0"\n
  //    Content-Length: 2375\n
  //    Content-Type: text/html; charset=UTF-8\n
  //    Last-Modified: Sun, 03 Sep 2006 04:34:43 GMT\n
  if (headers.empty())
    return STR();

  STR match;
  match.push_back('\n');
  match.append(name);
  match.push_back(':');

  typename STR::const_iterator begin =
      search(headers.begin(), headers.end(), match.begin(), match.end(),
             CaseInsensitiveCompareASCII<typename STR::value_type>());

  if (begin == headers.end())
    return STR();

  begin += match.length();

  typename STR::const_iterator end = find(begin, headers.end(), '\n');

  STR ret;
  TrimWhitespace(STR(begin, end), TRIM_ALL, &ret);
  return ret;
}

// TODO(jungshik): We have almost identical hex-decoding code else where.
// Consider refactoring and moving it somewhere(base?). Bug 1224311
inline bool IsHexDigit(unsigned char c) {
  return (('0' <= c && c <= '9') || ('A' <= c && c <= 'F') ||
          ('a' <= c && c <= 'f'));
}

inline unsigned char HexToInt(unsigned char c) {
  DCHECK(IsHexDigit(c));
  static unsigned char kOffset[4] = {0, 0x30u, 0x37u, 0x57u};
  return c - kOffset[(c >> 5) & 3];
}

// Similar to Base64Decode. Decodes a Q-encoded string to a sequence
// of bytes. If input is invalid, return false.
bool QPDecode(const std::string& input, std::string* output) {
  std::string temp;
  temp.reserve(input.size());
  std::string::const_iterator it = input.begin();
  while (it != input.end()) {
    if (*it == '_') {
      temp.push_back(' ');
    } else if (*it == '=') {
      if (input.end() - it < 3) {
        return false;
      }
      if (IsHexDigit(static_cast<unsigned char>(*(it + 1))) &&
          IsHexDigit(static_cast<unsigned char>(*(it + 2)))) {
        unsigned char ch = HexToInt(*(it + 1)) * 16 + HexToInt(*(it + 2));
        temp.push_back(static_cast<char>(ch));
        ++it;
        ++it;
      } else {
        return false;
      }
    } else if (0x20 < *it && *it < 0x7F) {
      // In a Q-encoded word, only printable ASCII characters
      // represent themselves. Besides, space, '=', '_' and '?' are
      // not allowed, but they're already filtered out.
      DCHECK(*it != 0x3D && *it != 0x5F && *it != 0x3F);
      temp.push_back(*it);
    } else {
      return false;
    }
    ++it;
  }
  output->swap(temp);
  return true;
}

enum RFC2047EncodingType {Q_ENCODING, B_ENCODING};
bool DecodeBQEncoding(const std::string& part, RFC2047EncodingType enc_type,
                       const std::string& charset, std::string* output) {
  std::string decoded;
  if (enc_type == B_ENCODING) {
    if (!net::Base64Decode(part, &decoded)) {
      return false;
    }
  } else {
    if (!QPDecode(part, &decoded)) {
      return false;
    }
  }

  UErrorCode err = U_ZERO_ERROR;
  UConverter* converter(ucnv_open(charset.c_str(), &err));
  if (U_FAILURE(err)) {
    return false;
  }

  // A single byte in a legacy encoding can be expanded to 3 bytes in UTF-8.
  // A 'two-byte character' in a legacy encoding can be expanded to 4 bytes
  // in UTF-8. Therefore, the expansion ratio is 3 at most.
  int length = static_cast<int>(decoded.length());
  char* buf = WriteInto(output, length * 3);
  length = ucnv_toAlgorithmic(UCNV_UTF8, converter, buf, length * 3,
      decoded.data(), length, &err);
  ucnv_close(converter);
  if (U_FAILURE(err)) {
    return false;
  }
  output->resize(length);
  return true;
}

bool DecodeWord(const std::string& encoded_word,
                const std::string& referrer_charset,
                bool *is_rfc2047,
                std::string* output) {
  if (!IsStringASCII(encoded_word)) {
    // Try UTF-8, referrer_charset and the native OS default charset in turn.
    if (IsStringUTF8(encoded_word)) {
      *output = encoded_word;
    } else {
      std::wstring wide_output;
      if (!referrer_charset.empty() &&
          base::CodepageToWide(encoded_word, referrer_charset.c_str(),
                               base::OnStringConversionError::FAIL,
                               &wide_output)) {
        *output = WideToUTF8(wide_output);
      } else {
        *output = WideToUTF8(base::SysNativeMBToWide(encoded_word));
      }
    }
    *is_rfc2047 = false;
    return true;
  }

  // RFC 2047 : one of encoding methods supported by Firefox and relatively
  // widely used by web servers.
  // =?charset?<E>?<encoded string>?= where '<E>' is either 'B' or 'Q'.
  // We don't care about the length restriction (72 bytes) because
  // many web servers generate encoded words longer than the limit.
  std::string tmp;
  *is_rfc2047 = true;
  int part_index = 0;
  std::string charset;
  StringTokenizer t(encoded_word, "?");
  RFC2047EncodingType enc_type = Q_ENCODING;
  while (*is_rfc2047 && t.GetNext()) {
    std::string part = t.token();
    switch (part_index) {
      case 0:
        if (part != "=") {
          *is_rfc2047 = false;
          break;
        }
        ++part_index;
        break;
      case 1:
        // Do we need charset validity check here?
        charset = part;
        ++part_index;
        break;
      case 2:
        if (part.size() > 1 ||
            part.find_first_of("bBqQ") == std::string::npos) {
          *is_rfc2047 = false;
          break;
        }
        if (part[0] == 'b' || part[0] == 'B') {
          enc_type = B_ENCODING;
        }
        ++part_index;
        break;
      case 3:
        *is_rfc2047 = DecodeBQEncoding(part, enc_type, charset, &tmp);
        if (!*is_rfc2047) {
          // Last minute failure. Invalid B/Q encoding. Rather than
          // passing it through, return now.
          return false;
        }
        ++part_index;
        break;
      case 4:
        if (part != "=") {
          // Another last minute failure !
          // Likely to be a case of two encoded-words in a row or
          // an encoded word followed by a non-encoded word. We can be
          // generous, but it does not help much in terms of compatibility,
          // I believe. Return immediately.
          *is_rfc2047 = false;
          return false;
        }
        ++part_index;
        break;
      default:
        *is_rfc2047 = false;
        return false;
    }
  }

  if (*is_rfc2047) {
    if (*(encoded_word.end() - 1) == '=') {
      output->swap(tmp);
      return true;
    }
    // encoded_word ending prematurelly with '?' or extra '?'
    *is_rfc2047 = false;
    return false;
  }

  // We're not handling 'especial' characters quoted with '\', but
  // it should be Ok because we're not an email client but a
  // web browser.

  // What IE6/7 does: %-escaped UTF-8. We could extend this to
  // support a rudimentary form of RFC 2231 with charset label, but
  // it'd gain us little in terms of compatibility.
  tmp = UnescapeURLComponent(encoded_word, UnescapeRule::SPACES);
  if (IsStringUTF8(tmp)) {
    output->swap(tmp);
    return true;
    // We can try either the OS default charset or 'origin charset' here,
    // As far as I can tell, IE does not support it. However, I've seen
    // web servers emit %-escaped string in a legacy encoding (usually
    // origin charset).
    // TODO(jungshik) : Test IE further and consider adding a fallback here.
  }
  return false;
}

bool DecodeParamValue(const std::string& input,
                      const std::string& referrer_charset,
                      std::string* output) {
  std::string tmp;
  // Tokenize with whitespace characters.
  StringTokenizer t(input, " \t\n\r");
  t.set_options(StringTokenizer::RETURN_DELIMS);
  bool is_previous_token_rfc2047 = true;
  while (t.GetNext()) {
    if (t.token_is_delim()) {
      // If the previous non-delimeter token is not RFC2047-encoded,
      // put in a space in its place. Otheriwse, skip over it.
      if (!is_previous_token_rfc2047) {
        tmp.push_back(' ');
      }
      continue;
    }
    // We don't support a single multibyte character split into
    // adjacent encoded words. Some broken mail clients emit headers
    // with that problem, but most web servers usually encode a filename
    // in a single encoded-word. Firefox/Thunderbird do not support
    // it, either.
    std::string decoded;
    if (!DecodeWord(t.token(), referrer_charset, &is_previous_token_rfc2047,
                    &decoded))
      return false;
    tmp.append(decoded);
  }
  output->swap(tmp);
  return true;
}

// TODO(mpcomplete): This is a quick and dirty implementation for now.  I'm
// sure this doesn't properly handle all (most?) cases.
template<typename STR>
STR GetHeaderParamValueT(const STR& header, const STR& param_name) {
  // This assumes args are formatted exactly like "bla; arg1=value; arg2=value".
  typename STR::const_iterator param_begin =
      search(header.begin(), header.end(), param_name.begin(), param_name.end(),
             CaseInsensitiveCompareASCII<typename STR::value_type>());

  if (param_begin == header.end())
    return STR();
  param_begin += param_name.length();

  STR whitespace;
  whitespace.push_back(' ');
  whitespace.push_back('\t');
  const typename STR::size_type equals_offset =
      header.find_first_not_of(whitespace, param_begin - header.begin());
  if (equals_offset == STR::npos || header.at(equals_offset) != '=')
    return STR();

  param_begin = header.begin() + equals_offset + 1;
  if (param_begin == header.end())
    return STR();

  typename STR::const_iterator param_end;
  if (*param_begin == '"') {
    param_end = find(param_begin+1, header.end(), '"');
    if (param_end == header.end())
      return STR();  // poorly formatted param?

    ++param_begin;  // skip past the quote.
  } else {
    param_end = find(param_begin+1, header.end(), ';');
  }

  return STR(param_begin, param_end);
}

// Does some simple normalization of scripts so we can allow certain scripts
// to exist together.
// TODO(brettw) bug 880223: we should allow some other languages to be
// oombined such as Chinese and Latin. We will probably need a more
// complicated system of language pairs to have more fine-grained control.
UScriptCode NormalizeScript(UScriptCode code) {
  switch (code) {
    case USCRIPT_KATAKANA:
    case USCRIPT_HIRAGANA:
    case USCRIPT_KATAKANA_OR_HIRAGANA:
    case USCRIPT_HANGUL:  // This one is arguable.
      return USCRIPT_HAN;
    default:
      return code;
  }
}

bool IsIDNComponentInSingleScript(const char16* str, int str_len) {
  UScriptCode first_script = USCRIPT_INVALID_CODE;
  bool is_first = true;

  int i = 0;
  while (i < str_len) {
    unsigned code_point;
    U16_NEXT(str, i, str_len, code_point);

    UErrorCode err = U_ZERO_ERROR;
    UScriptCode cur_script = uscript_getScript(code_point, &err);
    if (err != U_ZERO_ERROR)
      return false;  // Report mixed on error.
    cur_script = NormalizeScript(cur_script);

    // TODO(brettw) We may have to check for USCRIPT_INHERENT as well.
    if (is_first && cur_script != USCRIPT_COMMON) {
      first_script = cur_script;
      is_first = false;
    } else {
      if (cur_script != USCRIPT_COMMON && cur_script != first_script)
        return false;
    }
  }
  return true;
}

// Check if the script of a language can be 'safely' mixed with
// Latin letters in the ASCII range.
bool IsCompatibleWithASCIILetters(const std::string& lang) {
  // For now, just list Chinese, Japanese and Korean (positive list).
  // An alternative is negative-listing (languages using Greek and
  // Cyrillic letters), but it can be more dangerous.
  return !lang.substr(0, 2).compare("zh") ||
         !lang.substr(0, 2).compare("ja") ||
         !lang.substr(0, 2).compare("ko");
}

typedef std::map<std::string, icu::UnicodeSet*> LangToExemplarSetMap;

class LangToExemplarSet {
 private:
  LangToExemplarSetMap map;
  LangToExemplarSet() { }
  ~LangToExemplarSet() {
    STLDeleteContainerPairSecondPointers(map.begin(), map.end());
  }

  friend class Singleton<LangToExemplarSet>;
  friend struct DefaultSingletonTraits<LangToExemplarSet>;
  friend bool GetExemplarSetForLang(const std::string&, icu::UnicodeSet**);
  friend void SetExemplarSetForLang(const std::string&, icu::UnicodeSet*);

  DISALLOW_COPY_AND_ASSIGN(LangToExemplarSet);
};

bool GetExemplarSetForLang(const std::string& lang,
                           icu::UnicodeSet** lang_set) {
  const LangToExemplarSetMap& map = Singleton<LangToExemplarSet>()->map;
  LangToExemplarSetMap::const_iterator pos = map.find(lang);
  if (pos != map.end()) {
    *lang_set = pos->second;
    return true;
  }
  return false;
}

void SetExemplarSetForLang(const std::string& lang,
                           icu::UnicodeSet* lang_set) {
  LangToExemplarSetMap& map = Singleton<LangToExemplarSet>()->map;
  map.insert(std::make_pair(lang, lang_set));
}

static Lock lang_set_lock;

// Returns true if all the characters in component_characters are used by
// the language |lang|.
bool IsComponentCoveredByLang(const icu::UnicodeSet& component_characters,
                              const std::string& lang) {
  static const icu::UnicodeSet kASCIILetters(0x61, 0x7a);  // [a-z]
  icu::UnicodeSet* lang_set;
  // We're called from both the UI thread and the history thread.
  {
    AutoLock lock(lang_set_lock);
    if (!GetExemplarSetForLang(lang, &lang_set)) {
      UErrorCode status = U_ZERO_ERROR;
      ULocaleData* uld = ulocdata_open(lang.c_str(), &status);
      // TODO(jungshik) Turn this check on when the ICU data file is
      // rebuilt with the minimal subset of locale data for languages
      // to which Chrome is not localized but which we offer in the list
      // of languages selectable for Accept-Languages. With the rebuilt ICU
      // data, ulocdata_open never should fall back to the default locale.
      // (issue 2078)
      // DCHECK(U_SUCCESS(status) && status != U_USING_DEFAULT_WARNING);
      if (U_SUCCESS(status) && status != U_USING_DEFAULT_WARNING) {
        lang_set = reinterpret_cast<icu::UnicodeSet *>(
            ulocdata_getExemplarSet(uld, NULL, 0,
                                    ULOCDATA_ES_STANDARD, &status));
        // If |lang| is compatible with ASCII Latin letters, add them.
        if (IsCompatibleWithASCIILetters(lang))
          lang_set->addAll(kASCIILetters);
      } else {
        lang_set = new icu::UnicodeSet(1, 0);
      }
      lang_set->freeze();
      SetExemplarSetForLang(lang, lang_set);
      ulocdata_close(uld);
    }
  }
  return !lang_set->isEmpty() && lang_set->containsAll(component_characters);
}

// Returns true if the given Unicode host component is safe to display to the
// user.
bool IsIDNComponentSafe(const char16* str,
                        int str_len,
                        const std::wstring& languages) {
  // Most common cases (non-IDN) do not reach here so that we don't
  // need a fast return path.
  // TODO(jungshik) : Check if there's any character inappropriate
  // (although allowed) for domain names.
  // See http://www.unicode.org/reports/tr39/#IDN_Security_Profiles and
  // http://www.unicode.org/reports/tr39/data/xidmodifications.txt
  // For now, we borrow the list from Mozilla and tweaked it slightly.
  // (e.g. Characters like U+00A0, U+3000, U+3002 are omitted because
  //  they're gonna be canonicalized to U+0020 and full stop before
  //  reaching here.)
  // The original list is available at
  // http://kb.mozillazine.org/Network.IDN.blacklist_chars and
  // at http://mxr.mozilla.org/seamonkey/source/modules/libpref/src/init/all.js#703

  UErrorCode status = U_ZERO_ERROR;
#ifdef U_WCHAR_IS_UTF16
  icu::UnicodeSet dangerous_characters(icu::UnicodeString(
      L"[[\\ \u00bc\u00bd\u01c3\u0337\u0338"
      L"\u05c3\u05f4\u06d4\u0702\u115f\u1160][\u2000-\u200b]"
      L"[\u2024\u2027\u2028\u2029\u2039\u203a\u2044\u205f]"
      L"[\u2154-\u2156][\u2159-\u215b][\u215f\u2215\u23ae"
      L"\u29f6\u29f8\u2afb\u2afd][\u2ff0-\u2ffb][\u3014"
      L"\u3015\u3033\u3164\u321d\u321e\u33ae\u33af\u33c6\u33df\ufe14"
      L"\ufe15\ufe3f\ufe5d\ufe5e\ufeff\uff0e\uff06\uff61\uffa0\ufff9]"
      L"[\ufffa-\ufffd]]"), status);
#else
  icu::UnicodeSet dangerous_characters(icu::UnicodeString(
      "[[\\u0020\\u00bc\\u00bd\\u01c3\\u0337\\u0338"
      "\\u05c3\\u05f4\\u06d4\\u0702\\u115f\\u1160][\\u2000-\\u200b]"
      "[\\u2024\\u2027\\u2028\\u2029\\u2039\\u203a\\u2044\\u205f]"
      "[\\u2154-\\u2156][\\u2159-\\u215b][\\u215f\\u2215\\u23ae"
      "\\u29f6\\u29f8\\u2afb\\u2afd][\\u2ff0-\\u2ffb][\\u3014"
      "\\u3015\\u3033\\u3164\\u321d\\u321e\\u33ae\\u33af\\u33c6\\u33df\\ufe14"
      "\\ufe15\\ufe3f\\ufe5d\\ufe5e\\ufeff\\uff0e\\uff06\\uff61\\uffa0\\ufff9]"
      "[\\ufffa-\\ufffd]]", -1, US_INV), status);
#endif
  DCHECK(U_SUCCESS(status));
  icu::UnicodeSet component_characters;
  component_characters.addAll(icu::UnicodeString(str, str_len));
  if (dangerous_characters.containsSome(component_characters))
    return false;

  // If the language list is empty, the result is completely determined
  // by whether a component is a single script or not. This will block
  // even "safe" script mixing cases like <Chinese, Latin-ASCII> that are
  // allowed with |languages| (while it blocks Chinese + Latin letters with
  // an accent as should be the case), but we want to err on the safe side
  // when |languages| is empty.
  if (languages.empty())
    return IsIDNComponentInSingleScript(str, str_len);

  // |common_characters| is made up of  ASCII numbers, hyphen, plus and
  // underscore that are used across scripts and allowed in domain names.
  // (sync'd with characters allowed in url_canon_host with square
  // brackets excluded.) See kHostCharLookup[] array in url_canon_host.cc.
  icu::UnicodeSet common_characters(UNICODE_STRING_SIMPLE("[[0-9]\\-_+\\ ]"),
                                    status);
  DCHECK(U_SUCCESS(status));
  // Subtract common characters because they're always allowed so that
  // we just have to check if a language-specific set contains
  // the remainder.
  component_characters.removeAll(common_characters);

  std::string languages_list(WideToASCII(languages));
  StringTokenizer t(languages_list, ",");
  while (t.GetNext()) {
    if (IsComponentCoveredByLang(component_characters, t.token()))
      return true;
  }
  return false;
}

// Converts one component of a host (between dots) to IDN if safe. The result
// will be APPENDED to the given output string and will be the same as the input
// if it is not IDN or the IDN is unsafe to display.  Returns whether any
// conversion was performed.
bool IDNToUnicodeOneComponent(const char16* comp,
                              size_t comp_len,
                              const std::wstring& languages,
                              string16* out) {
  DCHECK(out);
  if (comp_len == 0)
    return false;

  // Only transform if the input can be an IDN component.
  static const char16 kIdnPrefix[] = {'x', 'n', '-', '-'};
  if ((comp_len > arraysize(kIdnPrefix)) &&
      !memcmp(comp, kIdnPrefix, arraysize(kIdnPrefix) * sizeof(char16))) {
    // Repeatedly expand the output string until it's big enough.  It looks like
    // ICU will return the required size of the buffer, but that's not
    // documented, so we'll just grow by 2x. This should be rare and is not on a
    // critical path.
    size_t original_length = out->length();
    for (int extra_space = 64; ; extra_space *= 2) {
      UErrorCode status = U_ZERO_ERROR;
      out->resize(out->length() + extra_space);
      int output_chars = uidna_IDNToUnicode(comp,
          static_cast<int32_t>(comp_len), &(*out)[original_length], extra_space,
          UIDNA_DEFAULT, NULL, &status);
      if (status == U_ZERO_ERROR) {
        // Converted successfully.
        out->resize(original_length + output_chars);
        if (IsIDNComponentSafe(out->data() + original_length, output_chars,
                               languages))
          return true;
      }

      if (status != U_BUFFER_OVERFLOW_ERROR)
        break;
    }
    // Failed, revert back to original string.
    out->resize(original_length);
  }

  // We get here with no IDN or on error, in which case we just append the
  // literal input.
  out->append(comp, comp_len);
  return false;
}

// Helper for FormatUrl().
std::wstring FormatViewSourceUrl(const GURL& url,
                                 const std::wstring& languages,
                                 bool omit_username_password,
                                 UnescapeRule::Type unescape_rules,
                                 url_parse::Parsed* new_parsed,
                                 size_t* prefix_end,
                                 size_t* offset_for_adjustment) {
  DCHECK(new_parsed);
  const wchar_t* const kWideViewSource = L"view-source:";
  const size_t kViewSourceLengthPlus1 = 12;

  GURL real_url(url.possibly_invalid_spec().substr(kViewSourceLengthPlus1));
  size_t temp_offset = (*offset_for_adjustment == std::wstring::npos) ?
      std::wstring::npos : (*offset_for_adjustment - kViewSourceLengthPlus1);
  size_t* temp_offset_ptr = (*offset_for_adjustment < kViewSourceLengthPlus1) ?
      NULL : &temp_offset;
  std::wstring result = net::FormatUrl(real_url, languages,
      omit_username_password, unescape_rules, new_parsed, prefix_end,
      temp_offset_ptr);
  result.insert(0, kWideViewSource);

  // Adjust position values.
  if (new_parsed->scheme.is_nonempty()) {
    // Assume "view-source:real-scheme" as a scheme.
    new_parsed->scheme.len += kViewSourceLengthPlus1;
  } else {
    new_parsed->scheme.begin = 0;
    new_parsed->scheme.len = kViewSourceLengthPlus1 - 1;
  }
  if (new_parsed->username.is_nonempty())
    new_parsed->username.begin += kViewSourceLengthPlus1;
  if (new_parsed->password.is_nonempty())
    new_parsed->password.begin += kViewSourceLengthPlus1;
  if (new_parsed->host.is_nonempty())
    new_parsed->host.begin += kViewSourceLengthPlus1;
  if (new_parsed->port.is_nonempty())
    new_parsed->port.begin += kViewSourceLengthPlus1;
  if (new_parsed->path.is_nonempty())
    new_parsed->path.begin += kViewSourceLengthPlus1;
  if (new_parsed->query.is_nonempty())
    new_parsed->query.begin += kViewSourceLengthPlus1;
  if (new_parsed->ref.is_nonempty())
    new_parsed->ref.begin += kViewSourceLengthPlus1;
  if (prefix_end)
    *prefix_end += kViewSourceLengthPlus1;
  if (temp_offset_ptr) {
    *offset_for_adjustment = (temp_offset == std::wstring::npos) ?
        std::wstring::npos : (temp_offset + kViewSourceLengthPlus1);
  }
  return result;
}

// Converts a UTF-8 string to a FilePath string type.
//
// This is inline with the hope that the function will be "free" on non-Windows
// platforms.
inline FilePath::StringType UTF8ToFilePathString(const std::string& utf8) {
#if defined(OS_WIN)
  return FilePath::StringType(UTF8ToUTF16(utf8));
#else
  return utf8;
#endif
}

}  // namespace

namespace net {

std::set<int> explicitly_allowed_ports;

// Appends the substring |in_component| inside of the URL |spec| to |output|,
// and the resulting range will be filled into |out_component|. |unescape_rules|
// defines how to clean the URL for human readability.  |offset_for_adjustment|
// is an offset into |output| which will be adjusted based on how it maps to the
// component being converted; if it is less than output->length(), it will be
// untouched, and if it is greater than output->length() + in_component.len it
// will be shortened by the difference in lengths between the input and output
// components.  Otherwise it points into the component being converted, and is
// adjusted to point to the same logical place in |output|.
// |offset_for_adjustment| may not be NULL.
static void AppendFormattedComponent(const std::string& spec,
                                     const url_parse::Component& in_component,
                                     UnescapeRule::Type unescape_rules,
                                     std::wstring* output,
                                     url_parse::Component* out_component,
                                     size_t* offset_for_adjustment);

GURL FilePathToFileURL(const FilePath& path) {
  // Produce a URL like "file:///C:/foo" for a regular file, or
  // "file://///server/path" for UNC. The URL canonicalizer will fix up the
  // latter case to be the canonical UNC form: "file://server/path"
  FilePath::StringType url_string(kFileURLPrefix);
  url_string.append(path.value());

  // Now do replacement of some characters. Since we assume the input is a
  // literal filename, anything the URL parser might consider special should
  // be escaped here.

  // must be the first substitution since others will introduce percents as the
  // escape character
  ReplaceSubstringsAfterOffset(&url_string, 0,
      FILE_PATH_LITERAL("%"), FILE_PATH_LITERAL("%25"));

  // semicolon is supposed to be some kind of separator according to RFC 2396
  ReplaceSubstringsAfterOffset(&url_string, 0,
      FILE_PATH_LITERAL(";"), FILE_PATH_LITERAL("%3B"));

  ReplaceSubstringsAfterOffset(&url_string, 0,
      FILE_PATH_LITERAL("#"), FILE_PATH_LITERAL("%23"));

#if defined(OS_POSIX)
  ReplaceSubstringsAfterOffset(&url_string, 0,
      FILE_PATH_LITERAL("\\"), FILE_PATH_LITERAL("%5C"));
#endif

  return GURL(url_string);
}

std::wstring GetSpecificHeader(const std::wstring& headers,
                               const std::wstring& name) {
  return GetSpecificHeaderT(headers, name);
}

std::string GetSpecificHeader(const std::string& headers,
                               const std::string& name) {
  return GetSpecificHeaderT(headers, name);
}

std::string GetFileNameFromCD(const std::string& header,
                              const std::string& referrer_charset) {
  std::string param_value = GetHeaderParamValue(header, "filename");
  if (param_value.empty()) {
    // Some servers use 'name' parameter.
    param_value = GetHeaderParamValue(header, "name");
  }
  if (param_value.empty())
    return std::string();
  std::string decoded;
  if (DecodeParamValue(param_value, referrer_charset, &decoded))
    return decoded;
  return std::string();
}

std::wstring GetHeaderParamValue(const std::wstring& field,
                                 const std::wstring& param_name) {
  return GetHeaderParamValueT(field, param_name);
}

std::string GetHeaderParamValue(const std::string& field,
                                const std::string& param_name) {
  return GetHeaderParamValueT(field, param_name);
}

// TODO(brettw) bug 734373: check the scripts for each host component and
// don't un-IDN-ize if there is more than one. Alternatively, only IDN for
// scripts that the user has installed. For now, just put the entire
// path through IDN. Maybe this feature can be implemented in ICU itself?
//
// We may want to skip this step in the case of file URLs to allow unicode
// UNC hostnames regardless of encodings.
std::wstring IDNToUnicode(const char* host,
                          size_t host_len,
                          const std::wstring& languages,
                          size_t* offset_for_adjustment) {
  // Convert the ASCII input to a wide string for ICU.
  string16 input16;
  input16.reserve(host_len);
  std::copy(host, host + host_len, std::back_inserter(input16));

  string16 out16;
  size_t output_offset = offset_for_adjustment ?
      *offset_for_adjustment : std::wstring::npos;

  // Do each component of the host separately, since we enforce script matching
  // on a per-component basis.
  for (size_t component_start = 0, component_end;
       component_start < input16.length();
       component_start = component_end + 1) {
    // Find the end of the component.
    component_end = input16.find('.', component_start);
    if (component_end == string16::npos)
      component_end = input16.length();  // For getting the last component.
    size_t component_length = component_end - component_start;

    size_t output_component_start = out16.length();
    bool converted_idn = false;
    if (component_end > component_start) {
      // Add the substring that we just found.
      converted_idn = IDNToUnicodeOneComponent(input16.data() + component_start,
          component_length, languages, &out16);
    }
    size_t output_component_length = out16.length() - output_component_start;

    if ((output_offset != std::wstring::npos) &&
        (*offset_for_adjustment > component_start)) {
      if ((*offset_for_adjustment < component_end) && converted_idn)
        output_offset = std::wstring::npos;
      else
        output_offset += output_component_length - component_length;
    }

    // Need to add the dot we just found (if we found one).
    if (component_end < input16.length())
      out16.push_back('.');
  }

  if (offset_for_adjustment)
    *offset_for_adjustment = output_offset;

  return UTF16ToWideAndAdjustOffset(out16, offset_for_adjustment);
}

std::string CanonicalizeHost(const std::string& host,
                             url_canon::CanonHostInfo* host_info) {
  // Try to canonicalize the host.
  const url_parse::Component raw_host_component(
      0, static_cast<int>(host.length()));
  std::string canon_host;
  url_canon::StdStringCanonOutput canon_host_output(&canon_host);
  url_canon::CanonicalizeHostVerbose(host.c_str(), raw_host_component,
                                     &canon_host_output, host_info);

  if (host_info->out_host.is_nonempty() &&
      host_info->family != url_canon::CanonHostInfo::BROKEN) {
    // Success!  Assert that there's no extra garbage.
    canon_host_output.Complete();
    DCHECK_EQ(host_info->out_host.len, static_cast<int>(canon_host.length()));
  } else {
    // Empty host, or canonicalization failed.  We'll return empty.
    canon_host.clear();
  }

  return canon_host;
}

std::string CanonicalizeHost(const std::wstring& host,
                             url_canon::CanonHostInfo* host_info) {
  std::string converted_host;
  WideToUTF8(host.c_str(), host.length(), &converted_host);
  return CanonicalizeHost(converted_host, host_info);
}

std::string GetDirectoryListingHeader(const string16& title) {
  static const base::StringPiece header(
      NetModule::GetResource(IDR_DIR_HEADER_HTML));
  // This can be null in unit tests.
  DLOG_IF(WARNING, header.empty()) <<
      "Missing resource: directory listing header";

  std::string result;
  if (!header.empty())
    result.assign(header.data(), header.size());

  result.append("<script>start(");
  base::JsonDoubleQuote(title, true, &result);
  result.append(");</script>\n");

  return result;
}

inline bool IsHostCharAlpha(char c) {
  // We can just check lowercase because uppercase characters have already been
  // normalized.
  return (c >= 'a') && (c <= 'z');
}

inline bool IsHostCharDigit(char c) {
  return (c >= '0') && (c <= '9');
}

bool IsCanonicalizedHostCompliant(const std::string& host) {
  if (host.empty())
    return false;

  bool in_component = false;
  bool most_recent_component_started_alpha = false;
  bool last_char_was_hyphen_or_underscore = false;

  for (std::string::const_iterator i(host.begin()); i != host.end(); ++i) {
    const char c = *i;
    if (!in_component) {
      most_recent_component_started_alpha = IsHostCharAlpha(c);
      if (!most_recent_component_started_alpha && !IsHostCharDigit(c))
        return false;
      in_component = true;
    } else {
      if (c == '.') {
        if (last_char_was_hyphen_or_underscore)
          return false;
        in_component = false;
      } else if (IsHostCharAlpha(c) || IsHostCharDigit(c)) {
        last_char_was_hyphen_or_underscore = false;
      } else if ((c == '-') || (c == '_')) {
        last_char_was_hyphen_or_underscore = true;
      } else {
        return false;
      }
    }
  }

  return most_recent_component_started_alpha;
}

std::string GetDirectoryListingEntry(const string16& name,
                                     const std::string& raw_bytes,
                                     bool is_dir,
                                     int64 size,
                                     Time modified) {
  std::string result;
  result.append("<script>addRow(");
  base::JsonDoubleQuote(name, true, &result);
  result.append(",");
  if (raw_bytes.empty()) {
    base::JsonDoubleQuote(EscapePath(UTF16ToUTF8(name)),
                                   true, &result);
  } else {
    base::JsonDoubleQuote(EscapePath(raw_bytes), true, &result);
  }
  if (is_dir) {
    result.append(",1,");
  } else {
    result.append(",0,");
  }

  base::JsonDoubleQuote(
      WideToUTF16Hack(FormatBytes(size, GetByteDisplayUnits(size), true)), true,
      &result);

  result.append(",");

  string16 modified_str;
  // |modified| can be NULL in FTP listings.
  if (!modified.is_null()) {
    modified_str = WideToUTF16Hack(base::TimeFormatShortDateAndTime(modified));
  }
  base::JsonDoubleQuote(modified_str, true, &result);

  result.append(");</script>\n");

  return result;
}

std::wstring StripWWW(const std::wstring& text) {
  const std::wstring www(L"www.");
  return (text.compare(0, www.length(), www) == 0) ?
      text.substr(www.length()) : text;
}

FilePath GetSuggestedFilename(const GURL& url,
                              const std::string& content_disposition,
                              const std::string& referrer_charset,
                              const FilePath& default_name) {
  // We don't translate this fallback string, "download". If localization is
  // needed, the caller should provide localized fallback default_name.
  static const FilePath::CharType kFinalFallbackName[] =
      FILE_PATH_LITERAL("download");

  // about: and data: URLs don't have file names, but esp. data: URLs may
  // contain parts that look like ones (i.e., contain a slash).
  // Therefore we don't attempt to divine a file name out of them.
  if (url.SchemeIs("about") || url.SchemeIs("data")) {
    return default_name.empty() ? FilePath(kFinalFallbackName) : default_name;
  }

  const std::string filename_from_cd = GetFileNameFromCD(content_disposition,
                                                          referrer_charset);
#if defined(OS_WIN)
  FilePath::StringType filename = UTF8ToWide(filename_from_cd);
#elif defined(OS_POSIX)
  FilePath::StringType filename = filename_from_cd;
#endif

  if (!filename.empty()) {
    // Remove any path information the server may have sent, take the name
    // only.
    filename = FilePath(filename).BaseName().value();

    // Next, remove "." from the beginning and end of the file name to avoid
    // tricks with hidden files, "..", and "."
    TrimString(filename, FILE_PATH_LITERAL("."), &filename);
  }
  if (filename.empty()) {
    if (url.is_valid()) {
      const std::string unescaped_url_filename = UnescapeURLComponent(
          url.ExtractFileName(),
          UnescapeRule::SPACES | UnescapeRule::URL_SPECIAL_CHARS);
#if defined(OS_WIN)
      filename = UTF8ToWide(unescaped_url_filename);
#elif defined(OS_POSIX)
      filename = unescaped_url_filename;
#endif
    }
  }

  // Trim '.' once more.
  TrimString(filename, FILE_PATH_LITERAL("."), &filename);

  // If there's no filename or it gets trimed to be empty, use
  // the URL hostname or default_name
  if (filename.empty()) {
    if (!default_name.empty()) {
      filename = default_name.value();
    } else if (url.is_valid()) {
      // Some schemes (e.g. file) do not have a hostname. Even though it's
      // not likely to reach here, let's hardcode the last fallback name.
      // TODO(jungshik) : Decode a 'punycoded' IDN hostname. (bug 1264451)
      filename = url.host().empty() ? kFinalFallbackName :
#if defined(OS_WIN)
          UTF8ToWide(url.host());
#elif defined(OS_POSIX)
          url.host();
#endif
    } else {
      NOTREACHED();
    }
  }

  file_util::ReplaceIllegalCharactersInPath(&filename, '-');
  return FilePath(filename);
}

bool IsPortAllowedByDefault(int port) {
  int array_size = arraysize(kRestrictedPorts);
  for (int i = 0; i < array_size; i++) {
    if (kRestrictedPorts[i] == port) {
      return false;
    }
  }
  return true;
}

bool IsPortAllowedByFtp(int port) {
  int array_size = arraysize(kAllowedFtpPorts);
  for (int i = 0; i < array_size; i++) {
    if (kAllowedFtpPorts[i] == port) {
        return true;
    }
  }
  // Port not explicitly allowed by FTP, so return the default restrictions.
  return IsPortAllowedByDefault(port);
}

bool IsPortAllowedByOverride(int port) {
  if (explicitly_allowed_ports.empty())
    return false;

  std::set<int>::const_iterator it =
      std::find(explicitly_allowed_ports.begin(),
                explicitly_allowed_ports.end(),
                port);

  return it != explicitly_allowed_ports.end();
}

int SetNonBlocking(int fd) {
#if defined(OS_WIN)
  unsigned long no_block = 1;
  return ioctlsocket(fd, FIONBIO, &no_block);
#elif defined(OS_POSIX)
  int flags = fcntl(fd, F_GETFL, 0);
  if (-1 == flags)
    flags = 0;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif
}

bool ParseHostAndPort(std::string::const_iterator host_and_port_begin,
                      std::string::const_iterator host_and_port_end,
                      std::string* host,
                      int* port) {
  if (host_and_port_begin >= host_and_port_end)
    return false;

  // When using url_parse, we use char*.
  const char* auth_begin = &(*host_and_port_begin);
  int auth_len = host_and_port_end - host_and_port_begin;

  url_parse::Component auth_component(0, auth_len);
  url_parse::Component username_component;
  url_parse::Component password_component;
  url_parse::Component hostname_component;
  url_parse::Component port_component;

  url_parse::ParseAuthority(auth_begin, auth_component, &username_component,
      &password_component, &hostname_component, &port_component);

  // There shouldn't be a username/password.
  if (username_component.is_valid() || password_component.is_valid())
    return false;

  if (!hostname_component.is_nonempty())
    return false;  // Failed parsing.

  int parsed_port_number = -1;
  if (port_component.is_nonempty()) {
    parsed_port_number = url_parse::ParsePort(auth_begin, port_component);

    // If parsing failed, port_number will be either PORT_INVALID or
    // PORT_UNSPECIFIED, both of which are negative.
    if (parsed_port_number < 0)
      return false;  // Failed parsing the port number.
  }

  if (port_component.len == 0)
    return false;  // Reject inputs like "foo:"

  // Pass results back to caller.
  host->assign(auth_begin + hostname_component.begin, hostname_component.len);
  *port = parsed_port_number;

  return true;  // Success.
}

bool ParseHostAndPort(const std::string& host_and_port,
                      std::string* host,
                      int* port) {
  return ParseHostAndPort(
      host_and_port.begin(), host_and_port.end(), host, port);
}

std::string GetHostAndPort(const GURL& url) {
  // For IPv6 literals, GURL::host() already includes the brackets so it is
  // safe to just append a colon.
  return StringPrintf("%s:%d", url.host().c_str(), url.EffectiveIntPort());
}

std::string GetHostAndOptionalPort(const GURL& url) {
  // For IPv6 literals, GURL::host() already includes the brackets
  // so it is safe to just append a colon.
  if (url.has_port())
    return StringPrintf("%s:%s", url.host().c_str(), url.port().c_str());
  return url.host();
}

std::string NetAddressToString(const struct addrinfo* net_address) {
#if defined(OS_WIN)
  EnsureWinsockInit();
#endif

  // This buffer is large enough to fit the biggest IPv6 string.
  char buffer[INET6_ADDRSTRLEN];

  int result = getnameinfo(net_address->ai_addr,
      net_address->ai_addrlen, buffer, sizeof(buffer), NULL, 0, NI_NUMERICHOST);

  if (result != 0) {
    DLOG(INFO) << "getnameinfo() failed with " << result;
    buffer[0] = '\0';
  }
  return std::string(buffer);
}

std::string GetHostName() {
#if defined(OS_WIN)
  EnsureWinsockInit();
#endif

  // Host names are limited to 255 bytes.
  char buffer[256];
  int result = gethostname(buffer, sizeof(buffer));
  if (result != 0) {
    DLOG(INFO) << "gethostname() failed with " << result;
    buffer[0] = '\0';
  }
  return std::string(buffer);
}

void GetIdentityFromURL(const GURL& url,
                        std::wstring* username,
                        std::wstring* password) {
  UnescapeRule::Type flags = UnescapeRule::SPACES;
  *username = UnescapeAndDecodeUTF8URLComponent(url.username(), flags, NULL);
  *password = UnescapeAndDecodeUTF8URLComponent(url.password(), flags, NULL);
}

void AppendFormattedHost(const GURL& url,
                         const std::wstring& languages,
                         std::wstring* output,
                         url_parse::Parsed* new_parsed,
                         size_t* offset_for_adjustment) {
  DCHECK(output);
  const url_parse::Component& host =
      url.parsed_for_possibly_invalid_spec().host;

  if (host.is_nonempty()) {
    // Handle possible IDN in the host name.
    int new_host_begin = static_cast<int>(output->length());
    if (new_parsed)
      new_parsed->host.begin = new_host_begin;
    size_t offset_past_current_output =
        (!offset_for_adjustment ||
         (*offset_for_adjustment == std::wstring::npos) ||
         (*offset_for_adjustment < output->length())) ?
            std::wstring::npos : (*offset_for_adjustment - output->length());
    size_t* offset_into_host =
        (offset_past_current_output >= static_cast<size_t>(host.len)) ?
            NULL : &offset_past_current_output;

    const std::string& spec = url.possibly_invalid_spec();
    DCHECK(host.begin >= 0 &&
           ((spec.length() == 0 && host.begin == 0) ||
            host.begin < static_cast<int>(spec.length())));
    output->append(net::IDNToUnicode(&spec[host.begin],
                   static_cast<size_t>(host.len), languages, offset_into_host));

    int new_host_len = static_cast<int>(output->length()) - new_host_begin;
    if (new_parsed)
      new_parsed->host.len = new_host_len;
    if (offset_into_host) {
      *offset_for_adjustment = (*offset_into_host == std::wstring::npos) ?
          std::wstring::npos : (new_host_begin + *offset_into_host);
    } else if (offset_past_current_output != std::wstring::npos) {
      *offset_for_adjustment += new_host_len - host.len;
    }
  } else if (new_parsed) {
    new_parsed->host.reset();
  }
}

/* static */
void AppendFormattedComponent(const std::string& spec,
                              const url_parse::Component& in_component,
                              UnescapeRule::Type unescape_rules,
                              std::wstring* output,
                              url_parse::Component* out_component,
                              size_t* offset_for_adjustment) {
  DCHECK(output);
  DCHECK(offset_for_adjustment);
  if (in_component.is_nonempty()) {
    out_component->begin = static_cast<int>(output->length());
    size_t offset_past_current_output =
        ((*offset_for_adjustment == std::wstring::npos) ||
         (*offset_for_adjustment < output->length())) ?
            std::wstring::npos : (*offset_for_adjustment - output->length());
    size_t* offset_into_component =
        (offset_past_current_output >= static_cast<size_t>(in_component.len)) ?
            NULL : &offset_past_current_output;
    if (unescape_rules == UnescapeRule::NONE) {
      output->append(UTF8ToWideAndAdjustOffset(
          spec.substr(in_component.begin, in_component.len),
          offset_into_component));
    } else {
      output->append(UnescapeAndDecodeUTF8URLComponent(
          spec.substr(in_component.begin, in_component.len), unescape_rules,
          offset_into_component));
    }
    out_component->len =
        static_cast<int>(output->length()) - out_component->begin;
    if (offset_into_component) {
      *offset_for_adjustment = (*offset_into_component == std::wstring::npos) ?
          std::wstring::npos : (out_component->begin + *offset_into_component);
    } else if (offset_past_current_output != std::wstring::npos) {
      *offset_for_adjustment += out_component->len - in_component.len;
    }
  } else {
    out_component->reset();
  }
}

std::wstring FormatUrl(const GURL& url,
                       const std::wstring& languages,
                       bool omit_username_password,
                       UnescapeRule::Type unescape_rules,
                       url_parse::Parsed* new_parsed,
                       size_t* prefix_end,
                       size_t* offset_for_adjustment) {
  url_parse::Parsed parsed_temp;
  if (!new_parsed)
    new_parsed = &parsed_temp;
  size_t offset_temp = std::wstring::npos;
  if (!offset_for_adjustment)
    offset_for_adjustment = &offset_temp;

  std::wstring url_string;

  // Check for empty URLs or 0 available text width.
  if (url.is_empty()) {
    if (prefix_end)
      *prefix_end = 0;
    *offset_for_adjustment = std::wstring::npos;
    return url_string;
  }

  // Special handling for view-source:.  Don't use chrome::kViewSourceScheme
  // because this library shouldn't depend on chrome.
  const char* const kViewSource = "view-source";
  const char* const kViewSourceTwice = "view-source:view-source:";
  // Rejects view-source:view-source:... to avoid deep recursive call.
  if (url.SchemeIs(kViewSource) &&
      !StartsWithASCII(url.possibly_invalid_spec(), kViewSourceTwice, false)) {
    return FormatViewSourceUrl(url, languages, omit_username_password,
        unescape_rules, new_parsed, prefix_end, offset_for_adjustment);
  }

  // We handle both valid and invalid URLs (this will give us the spec
  // regardless of validity).
  const std::string& spec = url.possibly_invalid_spec();
  const url_parse::Parsed& parsed = url.parsed_for_possibly_invalid_spec();
  if (*offset_for_adjustment >= spec.length())
    *offset_for_adjustment = std::wstring::npos;

  // Copy everything before the username (the scheme and the separators.)
  // These are ASCII.
  std::copy(spec.begin(),
      spec.begin() + parsed.CountCharactersBefore(url_parse::Parsed::USERNAME,
                                                  true),
      std::back_inserter(url_string));
  new_parsed->scheme = parsed.scheme;

  if (omit_username_password) {
    // Remove the username and password fields. We don't want to display those
    // to the user since they can be used for attacks,
    // e.g. "http://google.com:search@evil.ru/"
    new_parsed->username.reset();
    new_parsed->password.reset();
    if ((*offset_for_adjustment != std::wstring::npos) &&
        (parsed.username.is_nonempty() || parsed.password.is_nonempty())) {
      if (parsed.username.is_nonempty() && parsed.password.is_nonempty()) {
        // The seeming off-by-one and off-by-two in these first two lines are to
        // account for the ':' after the username and '@' after the password.
        if (*offset_for_adjustment >
            static_cast<size_t>(parsed.password.end())) {
          *offset_for_adjustment -=
              (parsed.username.len + parsed.password.len + 2);
        } else if (*offset_for_adjustment >
                   static_cast<size_t>(parsed.username.begin)) {
          *offset_for_adjustment = std::wstring::npos;
        }
      } else {
        const url_parse::Component* nonempty_component =
            parsed.username.is_nonempty() ? &parsed.username : &parsed.password;
        // The seeming off-by-one in these first two lines is to account for the
        // '@' after the username/password.
        if (*offset_for_adjustment >
            static_cast<size_t>(nonempty_component->end())) {
          *offset_for_adjustment -= (nonempty_component->len + 1);
        } else if (*offset_for_adjustment >
                   static_cast<size_t>(nonempty_component->begin)) {
          *offset_for_adjustment = std::wstring::npos;
        }
      }
    }
  } else {
    AppendFormattedComponent(spec, parsed.username, unescape_rules, &url_string,
                             &new_parsed->username, offset_for_adjustment);
    if (parsed.password.is_valid()) {
      url_string.push_back(':');
    }
    AppendFormattedComponent(spec, parsed.password, unescape_rules, &url_string,
                             &new_parsed->password, offset_for_adjustment);
    if (parsed.username.is_valid() || parsed.password.is_valid()) {
      url_string.push_back('@');
    }
  }
  if (prefix_end)
    *prefix_end = static_cast<size_t>(url_string.length());

  AppendFormattedHost(url, languages, &url_string, new_parsed,
                      offset_for_adjustment);

  // Port.
  if (parsed.port.is_nonempty()) {
    url_string.push_back(':');
    new_parsed->port.begin = url_string.length();
    std::copy(spec.begin() + parsed.port.begin,
              spec.begin() + parsed.port.end(), std::back_inserter(url_string));
    new_parsed->port.len = url_string.length() - new_parsed->port.begin;
  } else {
    new_parsed->port.reset();
  }

  // Path and query both get the same general unescape & convert treatment.
  AppendFormattedComponent(spec, parsed.path, unescape_rules, &url_string,
                           &new_parsed->path, offset_for_adjustment);
  if (parsed.query.is_valid())
    url_string.push_back('?');
  AppendFormattedComponent(spec, parsed.query, unescape_rules, &url_string,
                           &new_parsed->query, offset_for_adjustment);

  // Reference is stored in valid, unescaped UTF-8, so we can just convert.
  if (parsed.ref.is_valid()) {
    url_string.push_back('#');
    new_parsed->ref.begin = url_string.length();
    size_t offset_past_current_output =
        ((*offset_for_adjustment == std::wstring::npos) ||
         (*offset_for_adjustment < url_string.length())) ?
            std::wstring::npos : (*offset_for_adjustment - url_string.length());
    size_t* offset_into_ref =
        (offset_past_current_output >= static_cast<size_t>(parsed.ref.len)) ?
            NULL : &offset_past_current_output;
    if (parsed.ref.len > 0) {
      url_string.append(UTF8ToWideAndAdjustOffset(spec.substr(parsed.ref.begin,
                                                              parsed.ref.len),
                                                  offset_into_ref));
    }
    new_parsed->ref.len = url_string.length() - new_parsed->ref.begin;
    if (offset_into_ref) {
      *offset_for_adjustment = (*offset_into_ref == std::wstring::npos) ?
          std::wstring::npos : (new_parsed->ref.begin + *offset_into_ref);
    } else if (offset_past_current_output != std::wstring::npos) {
      // We clamped the offset near the beginning of this function to ensure it
      // was within the input URL.  If we reach here, the input was something
      // invalid and non-parseable such that the offset was past any component
      // we could figure out.  In this case it won't be represented in the
      // output string, so reset it.
      *offset_for_adjustment = std::wstring::npos;
    }
  }

  return url_string;
}

GURL SimplifyUrlForRequest(const GURL& url) {
  DCHECK(url.is_valid());
  GURL::Replacements replacements;
  replacements.ClearUsername();
  replacements.ClearPassword();
  replacements.ClearRef();
  return url.ReplaceComponents(replacements);
}

// Specifies a comma separated list of port numbers that should be accepted
// despite bans. If the string is invalid no allowed ports are stored.
void SetExplicitlyAllowedPorts(const std::wstring& allowed_ports) {
  if (allowed_ports.empty())
    return;

  std::set<int> ports;
  size_t last = 0;
  size_t size = allowed_ports.size();
  // The comma delimiter.
  const std::wstring::value_type kComma = L',';

  // Overflow is still possible for evil user inputs.
  for (size_t i = 0; i <= size; ++i) {
    // The string should be composed of only digits and commas.
    if (i != size && !IsAsciiDigit(allowed_ports[i]) &&
        (allowed_ports[i] != kComma))
      return;
    if (i == size || allowed_ports[i] == kComma) {
      size_t length = i - last;
      if (length > 0)
        ports.insert(StringToInt(WideToASCII(
            allowed_ports.substr(last, length))));
      last = i + 1;
    }
  }
  explicitly_allowed_ports = ports;
}

}  // namespace net
