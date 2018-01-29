// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkFontConfigParser_cobalt.h"

#include <libxml/parser.h>
#include <limits>
#include <stack>
#include <string>

#include "SkData.h"
#include "SkOSFile.h"
#include "SkOSPath.h"
#include "SkStream.h"
#include "SkTSearch.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "base/stringprintf.h"

namespace {

const char* kConfigFile = "fonts.xml";

/////////////////////////////////////////////////////////////////////////////
// Helpers
/////////////////////////////////////////////////////////////////////////////

std::string StringPrintVAndTrim(const char* message, va_list arguments) {
  const std::string formatted_message = base::StringPrintV(message, arguments);

  std::string trimmed_message;
  TrimWhitespace(formatted_message, TRIM_ALL, &trimmed_message);

  return trimmed_message;
}

// https://www.w3.org/TR/html-markup/datatypes.html#common.data.integer.non-negative-def
template <typename T>
bool ParseNonNegativeInteger(const char* s, T* value) {
  static_assert(std::numeric_limits<T>::is_integer, "T must be integer");
  const T n_max = std::numeric_limits<T>::max() / 10;
  const T d_max = std::numeric_limits<T>::max() - (n_max * 10);
  T n = 0;
  for (; *s; ++s) {
    // Check if digit
    if (!IsAsciiDigit(*s)) {
      return false;
    }
    int d = *s - '0';
    // Check for overflow
    if (n > n_max || (n == n_max && d > d_max)) {
      LOG(ERROR) << "---- ParseNonNegativeInteger error: overflow";
      return false;
    }
    n = (n * 10) + d;
  }
  *value = n;
  return true;
}

template <typename T>
bool ParseInteger(const char* s, T* value) {
  static_assert(std::numeric_limits<T>::is_signed, "T must be signed");
  T multiplier = 1;
  if (*s && *s == '-') {
    multiplier = -1;
    ++s;
  }
  if (!ParseNonNegativeInteger(s, value)) {
    return false;
  }
  *value *= multiplier;
  return true;
}

template <typename T>
bool ParseFontWeight(const char* s, T* value) {
  T n = 0;
  if (!ParseNonNegativeInteger(s, &n)) {
    return false;
  }
  // Verify that the weight is a multiple of 100 between 100 and 900.
  // https://www.w3.org/TR/css-fonts-3/#font-weight-prop
  if (n < 100 || n > 900 || n % 100 != 0) {
    return false;
  }
  *value = n;
  return true;
}

// The page range list is a comma-delimited list of character page ranges. Each
// page range can either consist of either a single integer value, or a pair of
// values separated by a hyphen (representing the min and max value of the
// range). All page values must fall between 0 and kMaxPageValue. Additionally,
// the page ranges must be provided in ascending order. Any failure to meet
// these expectations will result in a parsing error.
bool ParsePageRangeList(const char* s, font_character_map::PageRanges* ranges) {
  const int16 n_max = font_character_map::kMaxPageValue / 10;
  const int16 d_max = font_character_map::kMaxPageValue - (n_max * 10);

  int16 last_max = -1;

  while (*s) {
    font_character_map::PageRange range_value;

    // Skip whitespace
    while (IsAsciiWhitespace(*s)) {
      ++s;
      if (!*s) {
        return true;
      }
    }

    for (int i = 0; i <= 1; ++i) {
      if (!IsAsciiDigit(*s)) {
        LOG(ERROR)
            << "---- ParsePageRangeList error: non-ascii digit page range";
        return false;
      }

      int16 n = 0;
      for (; *s; ++s) {
        if (!IsAsciiDigit(*s)) {
          break;
        }
        int d = *s - '0';
        // Check for overflow
        if (n > n_max || (n == n_max && d > d_max)) {
          LOG(ERROR) << "---- ParsePageRangeList error: page range overflow";
          return false;
        }

        n = (n * 10) + d;
      }

      if (i == 0) {
        // Verify that this new range is larger than the previously encountered
        // max. Ranges must appear in order. If it isn't, then the parsing has
        // failed.
        if (last_max >= n) {
          LOG(ERROR) << "---- ParsePageRangeList error: pages unordered";
          return false;
        }

        range_value.first = n;

        if (*s && *s == '-') {
          ++s;
          continue;
        } else {
          last_max = n;
          range_value.second = n;
          ranges->push_back(range_value);
          break;
        }
      } else {
        if (range_value.first <= n) {
          last_max = n;
          range_value.second = n;
          ranges->push_back(range_value);
        } else {
          LOG(ERROR) << "---- ParsePageRangeList error: page range flipped";
          return false;
        }
      }
    }

    if (*s) {
      // Skip whitespace
      while (IsAsciiWhitespace(*s)) {
        ++s;
        if (!*s) {
          return true;
        }
      }

      if (*s == ',') {
        ++s;
      } else {
        LOG(ERROR) << "---- ParsePageRangeList error: invalid character";
        return false;
      }
    }
  }

  return true;
}

/////////////////////////////////////////////////////////////////////////////
// Libxml SAX Handlers
/////////////////////////////////////////////////////////////////////////////

typedef unsigned char xmlChar;

void StartElement(void* context, const xmlChar* name,
                  const xmlChar** attribute_pairs);
void EndElement(void* context, const xmlChar* name);
void Characters(void* context, const xmlChar* ch, int len);
void ParserWarning(void* context, const char* message, ...);
void ParserError(void* context, const char* message, ...);
void ParserFatal(void* context, const char* message, ...);

xmlSAXHandler xml_sax_handler = {
    NULL,           /* internalSubset */
    NULL,           /* isStandalone */
    NULL,           /* hasInternalSubset */
    NULL,           /* hasExternalSubset */
    NULL,           /* resolveEntity */
    NULL,           /* getEntity */
    NULL,           /* entityDecl */
    NULL,           /* notationDecl */
    NULL,           /* attributeDecl */
    NULL,           /* elementDecl */
    NULL,           /* unparsedEntityDecl */
    NULL,           /* setDocumentLocator */
    NULL,           /* startDocument */
    NULL,           /* endDocument */
    &StartElement,  /* startElement */
    &EndElement,    /* endElement */
    NULL,           /* reference */
    &Characters,    /* characters */
    NULL,           /* ignorableWhitespace */
    NULL,           /* processingInstruction */
    NULL,           /* comment */
    &ParserWarning, /* xmlParserWarning */
    &ParserError,   /* xmlParserError */
    &ParserFatal,   /* xmlParserFatalError */
    NULL,           /* getParameterEntity */
    NULL,           /* cdataBlock */
    NULL,           /* externalSubset */
    1,              /* initialized */
    NULL,           /* private */
    NULL,           /* startElementNsSAX2Func */
    NULL,           /* endElementNsSAX2Func */
    NULL            /* xmlStructuredErrorFunc */
};

enum ElementType {
  kFamilyElementType,
  kFontElementType,
  kAliasElementType,
  kOtherElementType,
};

// The ParserContext structure is passed around by the parser so that each
// handler can read these variables that are relevant to the current parsing.
struct ParserContext {
  explicit ParserContext(SkTDArray<FontFamilyInfo*>* families_array)
      : families(families_array), current_font_info(NULL) {}

  // The array that each family is put into as it is parsed
  SkTDArray<FontFamilyInfo*>* families;
  // The current family being created. FamilyData owns this object while it is
  // not NULL.
  scoped_ptr<FontFamilyInfo> current_family;
  // The current FontFileInfo being created. It is owned by FontFamilyInfo.
  FontFileInfo* current_font_info;

  // Contains all of the elements that are actively being parsed.
  std::stack<ElementType> element_stack;
};

void FamilyElementHandler(FontFamilyInfo* family, const char** attributes) {
  if (attributes == NULL) {
    return;
  }

  // A <family> may have the following attributes:
  // name (string), fallback_priority (int), lang (string), pages (comma
  // delimited int ranges)
  // A <family> tag must have a canonical name attribute or be a fallback
  // family in order to be usable, unless it is the default family.
  // A family is a fallback family if one of two conditions are met:
  //   1. It does not have a "name" attribute.
  //   2. It has a "fallback_priority" attribute.
  // If the fallback_priority attribute exists, then the family is given the
  // fallback priority specified by the value. If it does not exist, then the
  // fallback priority defaults to 0.
  // The lang and pages attributes are only used by fallback families.

  bool encountered_fallback_attribute = false;
  family->is_fallback_family = true;

  for (size_t i = 0; attributes[i] != NULL && attributes[i + 1] != NULL;
       i += 2) {
    const char* name = attributes[i];
    const char* value = attributes[i + 1];
    size_t name_len = strlen(name);

    if (name_len == 4 && strncmp(name, "name", name_len) == 0) {
      SkAutoAsciiToLC to_lowercase(value);
      family->names.push_back().set(to_lowercase.lc());
      // As long as no fallback attribute is encountered, then the existence of
      // a name attribute removes this family from fallback.
      if (!encountered_fallback_attribute) {
        family->is_fallback_family = false;
      }
    } else if (name_len == 4 && strncmp("lang", name, name_len) == 0) {
      family->language = SkLanguage(value);
    } else if (name_len == 5 && strncmp("pages", name, name_len) == 0) {
      if (!ParsePageRangeList(value, &family->page_ranges)) {
        LOG(ERROR) << "---- Invalid page ranges [" << value << "]";
        NOTREACHED();
        family->page_ranges.reset();
      }
    } else if (name_len == 17 &&
               strncmp("fallback_priority", name, name_len) == 0) {
      encountered_fallback_attribute = true;
      if (!ParseInteger(value, &family->fallback_priority)) {
        LOG(ERROR) << "---- Invalid fallback priority [" << value << "]";
        NOTREACHED();
      }
    } else {
      LOG(ERROR) << "---- Unsupported family attribute [" << name << "]";
      NOTREACHED();
    }
  }
}

void FontElementHandler(FontFileInfo* file, const char** attributes) {
  DCHECK(file != NULL);

  // A <font> may have following attributes:
  // weight (non-negative integer), style (normal, italic), font_name (string),
  // postscript_name (string), and index (non-negative integer)
  // The element should contain a filename.

  for (size_t i = 0; attributes[i] != NULL && attributes[i + 1] != NULL;
       i += 2) {
    const char* name = attributes[i];
    const char* value = attributes[i + 1];

    switch (strlen(name)) {
      case 5:
        if (strncmp("index", name, 5) == 0) {
          if (!ParseNonNegativeInteger(value, &file->index)) {
            LOG(ERROR) << "---- Invalid font index [" << value << "]";
            NOTREACHED();
          }
          continue;
        } else if (strncmp("style", name, 5) == 0) {
          if (strncmp("italic", value, 6) == 0) {
            file->style = FontFileInfo::kItalic_FontStyle;
            continue;
          } else if (strncmp("normal", value, 6) == 0) {
            file->style = FontFileInfo::kNormal_FontStyle;
            continue;
          } else {
            LOG(ERROR) << "---- Unsupported style [" << value << "]";
            NOTREACHED();
          }
        }
        break;
      case 6:
        if (strncmp("weight", name, 6) == 0) {
          if (!ParseFontWeight(value, &file->weight)) {
            LOG(ERROR) << "---- Invalid font weight [" << value << "]";
            NOTREACHED();
          }
          continue;
        }
        break;
      case 9:
        if (strncmp("font_name", name, 9) == 0) {
          SkAutoAsciiToLC to_lowercase(value);
          file->full_font_name = to_lowercase.lc();
          continue;
        }
        break;
      case 15:
        if (strncmp("postscript_name", name, 15) == 0) {
          SkAutoAsciiToLC to_lowercase(value);
          file->postscript_name = to_lowercase.lc();
          continue;
        }
        break;
      case 25:
        if (strncmp("disable_synthetic_bolding", name, 25) == 0) {
          file->disable_synthetic_bolding =
              strcmp("true", value) == 0 || strcmp("1", value) == 0;
          continue;
        }
        break;
      default:
        break;
    }

    LOG(ERROR) << "---- Unsupported font attribute [" << name << "]";
    NOTREACHED();
  }
}

FontFamilyInfo* FindFamily(ParserContext* context, const char* family_name) {
  size_t name_len = strlen(family_name);
  for (int i = 0; i < context->families->count(); i++) {
    FontFamilyInfo* candidate = (*context->families)[i];
    for (int j = 0; j < candidate->names.count(); j++) {
      if (!strncmp(candidate->names[j].c_str(), family_name, name_len) &&
          name_len == strlen(candidate->names[j].c_str())) {
        return candidate;
      }
    }
  }

  return NULL;
}

void AliasElementHandler(ParserContext* context, const char** attributes) {
  // An <alias> must have name and to attributes.
  // It is a variant name for a <family>.

  SkString alias_name;
  SkString to;
  for (size_t i = 0; attributes[i] != NULL && attributes[i + 1] != NULL;
       i += 2) {
    const char* name = attributes[i];
    const char* value = attributes[i + 1];
    size_t name_len = strlen(name);
    if (name_len == 4 && strncmp("name", name, name_len) == 0) {
      SkAutoAsciiToLC to_lowercase(value);
      alias_name.set(to_lowercase.lc());
    } else if (name_len == 2 && strncmp("to", name, name_len) == 0) {
      to.set(value);
    }
  }

  // Assumes that the named family is already declared
  FontFamilyInfo* target_family = FindFamily(context, to.c_str());
  if (!target_family) {
    LOG(ERROR) << "---- Invalid alias target [name: " << alias_name.c_str()
               << ", to: " << to.c_str() << "]";
    NOTREACHED();
    return;
  } else if (alias_name.size() == 0) {
    LOG(ERROR) << "---- Invalid alias name [to: " << to.c_str() << "]";
    NOTREACHED();
    return;
  }

  target_family->names.push_back().set(alias_name);
}

void StartElement(void* context, const xmlChar* xml_tag,
                  const xmlChar** xml_attribute_pairs) {
  ParserContext* parser_context = reinterpret_cast<ParserContext*>(context);
  const char* tag = reinterpret_cast<const char*>(xml_tag);
  const char** attribute_pairs =
      reinterpret_cast<const char**>(xml_attribute_pairs);
  size_t tag_len = strlen(tag);

  if (tag_len == 6 && strncmp("family", tag, tag_len) == 0) {
    parser_context->element_stack.push(kFamilyElementType);
    parser_context->current_family = make_scoped_ptr(new FontFamilyInfo());
    FamilyElementHandler(parser_context->current_family.get(), attribute_pairs);
  } else if (tag_len == 4 && strncmp("font", tag, tag_len) == 0) {
    parser_context->element_stack.push(kFontElementType);
    FontFileInfo* file = &parser_context->current_family->fonts.push_back();
    parser_context->current_font_info = file;
    FontElementHandler(file, attribute_pairs);
  } else if (tag_len == 5 && strncmp("alias", tag, tag_len) == 0) {
    parser_context->element_stack.push(kAliasElementType);
    AliasElementHandler(parser_context, attribute_pairs);
  } else {
    parser_context->element_stack.push(kOtherElementType);
  }
}

void EndElement(void* context, const xmlChar* xml_tag) {
  ParserContext* parser_context = reinterpret_cast<ParserContext*>(context);
  const char* tag = reinterpret_cast<const char*>(xml_tag);
  size_t tag_len = strlen(tag);

  if (tag_len == 6 && strncmp("family", tag, tag_len) == 0) {
    if (parser_context->current_family != NULL) {
      *parser_context->families->append() =
          parser_context->current_family.release();
    } else {
      LOG(ERROR) << "---- Encountered end family tag with no current family";
      NOTREACHED();
    }
  }

  parser_context->element_stack.pop();
}

void Characters(void* context, const xmlChar* xml_characters, int len) {
  ParserContext* parser_context = reinterpret_cast<ParserContext*>(context);
  const char* characters = reinterpret_cast<const char*>(xml_characters);

  if (parser_context->element_stack.size() > 0 &&
      parser_context->element_stack.top() == kFontElementType) {
    parser_context->current_font_info->file_name.set(characters, len);
  }
}

void ParserWarning(void* context, const char* message, ...) {
  va_list arguments;
  va_start(arguments, message);

  DLOG(WARNING) << "---- Parsing warning: "
                << StringPrintVAndTrim(message, arguments).c_str();
}

void ParserError(void* context, const char* message, ...) {
  va_list arguments;
  va_start(arguments, message);

  LOG(ERROR) << "---- Parsing error: "
             << StringPrintVAndTrim(message, arguments).c_str();
  NOTREACHED();
}

void ParserFatal(void* context, const char* message, ...) {
  va_list arguments;
  va_start(arguments, message);

  LOG(ERROR) << "---- Parsing fatal error: "
             << StringPrintVAndTrim(message, arguments).c_str();
  NOTREACHED();
}

// This function parses the given filename and stores the results in the given
// families array.
void ParseConfigFile(const char* directory,
                     SkTDArray<FontFamilyInfo*>* families) {
  SkString file_path = SkOSPath::Join(directory, kConfigFile);

  std::unique_ptr<SkStream> file_stream(
      SkStream::MakeFromFile(file_path.c_str()));
  if (file_stream == NULL) {
    LOG(ERROR) << "---- Failed to open %s", file_path.c_str();
    return;
  }

  sk_sp<SkData> file_data(
      SkData::MakeFromStream(file_stream.get(), file_stream->getLength()));
  if (file_data == NULL) {
    LOG(ERROR) << "---- Failed to read %s", file_path.c_str();
    return;
  }

  ParserContext parser_context(families);
  int return_value =
      xmlSAXUserParseMemory(&xml_sax_handler, &parser_context,
                            static_cast<const char*>(file_data->data()),
                            static_cast<int>(file_data->size()));
  DCHECK_EQ(return_value, 0);
}

}  // namespace

namespace SkFontConfigParser {

// Loads data on font families from the configuration file. The resulting data
// is returned in the given fontFamilies array.
void GetFontFamilies(const char* directory,
                     SkTDArray<FontFamilyInfo*>* font_families) {
  ParseConfigFile(directory, font_families);
}

}  // namespace SkFontConfigParser
