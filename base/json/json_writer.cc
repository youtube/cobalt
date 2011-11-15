// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_writer.h"

#include "base/json/string_escape.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "base/utf_string_conversions.h"

namespace base {

#if defined(OS_WIN)
static const char kPrettyPrintLineEnding[] = "\r\n";
#else
static const char kPrettyPrintLineEnding[] = "\n";
#endif

/* static */
const char* JSONWriter::kEmptyArray = "[]";

/* static */
void JSONWriter::Write(const Value* const node,
                       bool pretty_print,
                       std::string* json) {
  WriteWithOptions(node, pretty_print, 0, json);
}

/* static */
void JSONWriter::WriteWithOptions(const Value* const node,
                                  bool pretty_print,
                                  int options,
                                  std::string* json) {
  json->clear();
  // Is there a better way to estimate the size of the output?
  json->reserve(1024);
  JSONWriter writer(pretty_print, json);
  bool escape = !(options & OPTIONS_DO_NOT_ESCAPE);
  bool omit_binary_values = !!(options & OPTIONS_OMIT_BINARY_VALUES);
  writer.BuildJSONString(node, 0, escape, omit_binary_values);
  if (pretty_print)
    json->append(kPrettyPrintLineEnding);
}

JSONWriter::JSONWriter(bool pretty_print, std::string* json)
    : json_string_(json),
      pretty_print_(pretty_print) {
  DCHECK(json);
}

void JSONWriter::BuildJSONString(const Value* const node,
                                 int depth,
                                 bool escape,
                                 bool omit_binary_values) {
  switch (node->GetType()) {
    case Value::TYPE_NULL:
      json_string_->append("null");
      break;

    case Value::TYPE_BOOLEAN:
      {
        bool value;
        bool result = node->GetAsBoolean(&value);
        DCHECK(result);
        json_string_->append(value ? "true" : "false");
        break;
      }

    case Value::TYPE_INTEGER:
      {
        int value;
        bool result = node->GetAsInteger(&value);
        DCHECK(result);
        base::StringAppendF(json_string_, "%d", value);
        break;
      }

    case Value::TYPE_DOUBLE:
      {
        double value;
        bool result = node->GetAsDouble(&value);
        DCHECK(result);
        std::string real = DoubleToString(value);
        // Ensure that the number has a .0 if there's no decimal or 'e'.  This
        // makes sure that when we read the JSON back, it's interpreted as a
        // real rather than an int.
        if (real.find('.') == std::string::npos &&
            real.find('e') == std::string::npos &&
            real.find('E') == std::string::npos) {
          real.append(".0");
        }
        // The JSON spec requires that non-integer values in the range (-1,1)
        // have a zero before the decimal point - ".52" is not valid, "0.52" is.
        if (real[0] == '.') {
          real.insert(0, "0");
        } else if (real.length() > 1 && real[0] == '-' && real[1] == '.') {
          // "-.1" bad "-0.1" good
          real.insert(1, "0");
        }
        json_string_->append(real);
        break;
      }

    case Value::TYPE_STRING:
      {
        std::string value;
        bool result = node->GetAsString(&value);
        DCHECK(result);
        if (escape) {
          JsonDoubleQuote(UTF8ToUTF16(value), true, json_string_);
        } else {
          JsonDoubleQuote(value, true, json_string_);
        }
        break;
      }

    case Value::TYPE_LIST:
      {
        json_string_->append("[");
        if (pretty_print_)
          json_string_->append(" ");

        const ListValue* list = static_cast<const ListValue*>(node);
        for (size_t i = 0; i < list->GetSize(); ++i) {
          Value* value = NULL;
          bool result = list->Get(i, &value);
          DCHECK(result);

          if (omit_binary_values && value->GetType() == Value::TYPE_BINARY) {
            continue;
          }

          if (i != 0) {
            json_string_->append(",");
            if (pretty_print_)
              json_string_->append(" ");
          }

          BuildJSONString(value, depth, escape, omit_binary_values);
        }

        if (pretty_print_)
          json_string_->append(" ");
        json_string_->append("]");
        break;
      }

    case Value::TYPE_DICTIONARY:
      {
        json_string_->append("{");
        if (pretty_print_)
          json_string_->append(kPrettyPrintLineEnding);

        const DictionaryValue* dict =
          static_cast<const DictionaryValue*>(node);
        for (DictionaryValue::key_iterator key_itr = dict->begin_keys();
             key_itr != dict->end_keys();
             ++key_itr) {
          Value* value = NULL;
          bool result = dict->GetWithoutPathExpansion(*key_itr, &value);
          DCHECK(result);

          if (omit_binary_values && value->GetType() == Value::TYPE_BINARY) {
            continue;
          }

          if (key_itr != dict->begin_keys()) {
            json_string_->append(",");
            if (pretty_print_)
              json_string_->append(kPrettyPrintLineEnding);
          }

          if (pretty_print_)
            IndentLine(depth + 1);
          AppendQuotedString(*key_itr);
          if (pretty_print_) {
            json_string_->append(": ");
          } else {
            json_string_->append(":");
          }
          BuildJSONString(value, depth + 1, escape, omit_binary_values);
        }

        if (pretty_print_) {
          json_string_->append(kPrettyPrintLineEnding);
          IndentLine(depth);
          json_string_->append("}");
        } else {
          json_string_->append("}");
        }
        break;
      }

    case Value::TYPE_BINARY:
      {
        if (!omit_binary_values) {
          NOTREACHED() << "Cannot serialize binary value.";
        }
        break;
      }

    default:
      NOTREACHED() << "unknown json type";
  }
}

void JSONWriter::AppendQuotedString(const std::string& str) {
  // TODO(viettrungluu): |str| is UTF-8, not ASCII, so to properly escape it we
  // have to convert it to UTF-16. This round-trip is suboptimal.
  JsonDoubleQuote(UTF8ToUTF16(str), true, json_string_);
}

void JSONWriter::IndentLine(int depth) {
  // It may be faster to keep an indent string so we don't have to keep
  // reallocating.
  json_string_->append(std::string(depth * 3, ' '));
}

}  // namespace base
