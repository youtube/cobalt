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

#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "SkData.h"
#include "SkOSFile.h"
#include "SkStream.h"
#include "SkTSearch.h"

namespace {

const char* kSystemFontsFile = "fonts.xml";

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
  SK_COMPILE_ASSERT(std::numeric_limits<T>::is_integer, T_must_be_integer);
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
      SkDebugf("---- ParseNonNegativeInteger error: overflow)");
      return false;
    }
    n = (n * 10) + d;
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
        SkDebugf("---- ParsePageRangeList error: non-ascii digit page range");
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
          SkDebugf("---- ParsePageRangeList error: page range overflow");
          return false;
        }

        n = (n * 10) + d;
      }

      if (i == 0) {
        // Verify that this new range is larger than the previously encountered
        // max. Ranges must appear in order. If it isn't, then the parsing has
        // failed.
        if (last_max >= n) {
          SkDebugf("---- ParsePageRangeList error: pages unordered");
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
          SkDebugf("---- ParsePageRangeList error: page range flipped");
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
        SkDebugf("---- ParsePageRangeList error: invalid character");
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

// The FamilyData structure is passed around by the parser so that each handler
// can read these variables that are relevant to the current parsing.
struct FamilyData {
  explicit FamilyData(SkTDArray<FontFamily*>* families_array)
      : families(families_array), current_font_info(NULL) {}

  // The array that each family is put into as it is parsed
  SkTDArray<FontFamily*>* families;
  // The current family being created. FamilyData owns this object while it is
  // not NULL.
  scoped_ptr<FontFamily> current_family;
  // The current fontInfo being created. It is owned by FontFamily.
  FontFileInfo* current_font_info;
  std::stack<ElementType> element_stack;
};

void FamilyElementHandler(FontFamily* family, const char** attributes) {
  if (attributes == NULL) {
    return;
  }

  // A <family> may have the following attributes:
  // name (string), fallback (true, false), lang (string), pages (comma
  // delimited int ranges)
  // A <family> tag must have a canonical name attribute or be a fallback
  // family in order to be usable, unless it is the default family.
  // If the fallback attribute exists, then it alone determines whether the
  // family is a fallback family.  However, if it does not exist, then the
  // lack of a name attribute makes it a fallback family.
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
        SkDebugf("---- Page ranges %s (INVALID)", value);
        family->page_ranges.reset();
      }
    } else if (name_len == 8 && strncmp("fallback", name, name_len) == 0) {
      encountered_fallback_attribute = true;
      family->is_fallback_family =
          strcmp("true", value) == 0 || strcmp("1", value) == 0;
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
            DLOG(WARNING) << "Invalid font index [" << value << "]";
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
            NOTREACHED() << "Unsupported style [" << value << "]";
          }
        }
        break;
      case 6:
        if (strncmp("weight", name, 6) == 0) {
          if (!ParseNonNegativeInteger(value, &file->weight)) {
            DLOG(WARNING) << "Invalid font weight [" << value << "]";
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

    NOTREACHED() << "Unsupported attribute [" << name << "]";
  }
}

FontFamily* FindFamily(FamilyData* family_data, const char* family_name) {
  size_t name_len = strlen(family_name);
  for (int i = 0; i < family_data->families->count(); i++) {
    FontFamily* candidate = (*family_data->families)[i];
    for (int j = 0; j < candidate->names.count(); j++) {
      if (!strncmp(candidate->names[j].c_str(), family_name, name_len) &&
          name_len == strlen(candidate->names[j].c_str())) {
        return candidate;
      }
    }
  }

  return NULL;
}

void AliasElementHandler(FamilyData* family_data, const char** attributes) {
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
  FontFamily* target_family = FindFamily(family_data, to.c_str());
  if (!target_family) {
    SkDebugf("---- Font alias target %s (NOT FOUND)", to.c_str());
    return;
  } else if (alias_name.size() == 0) {
    SkDebugf("---- Font alias name for %s (NOT SET)", to.c_str());
    return;
  }

  target_family->names.push_back().set(alias_name);
}

bool FindWeight400(FontFamily* family) {
  for (int i = 0; i < family->fonts.count(); i++) {
    if (family->fonts[i].weight == 400) {
      return true;
    }
  }
  return false;
}

bool DesiredWeight(int weight) { return (weight == 400 || weight == 700); }

int CountDesiredWeight(FontFamily* family) {
  int count = 0;
  for (int i = 0; i < family->fonts.count(); i++) {
    if (DesiredWeight(family->fonts[i].weight)) {
      count++;
    }
  }
  return count;
}

// To meet Skia's expectations, any family that contains weight=400
// fonts should *only* contain {400,700}
void PurgeUndesiredWeights(FontFamily* family) {
  int count = CountDesiredWeight(family);
  for (int i = 1, j = 0; i < family->fonts.count(); i++) {
    if (DesiredWeight(family->fonts[j].weight)) {
      j++;
    }
    if ((i != j) && DesiredWeight(family->fonts[i].weight)) {
      family->fonts[j] = family->fonts[i];
    }
  }
  family->fonts.resize_back(count);
}

void FamilySetElementEndHandler(FamilyData* familyData) {
  for (int i = 0; i < familyData->families->count(); i++) {
    if (FindWeight400((*familyData->families)[i])) {
      PurgeUndesiredWeights((*familyData->families)[i]);
    }
  }
}

void StartElement(void* data, const xmlChar* xml_tag,
                  const xmlChar** xml_attribute_pairs) {
  FamilyData* family_data = reinterpret_cast<FamilyData*>(data);
  const char* tag = reinterpret_cast<const char*>(xml_tag);
  const char** attribute_pairs =
      reinterpret_cast<const char**>(xml_attribute_pairs);
  size_t tag_len = strlen(tag);

  if (tag_len == 6 && strncmp("family", tag, tag_len) == 0) {
    family_data->element_stack.push(kFamilyElementType);
    family_data->current_family = make_scoped_ptr(new FontFamily());
    FamilyElementHandler(family_data->current_family.get(), attribute_pairs);
  } else if (tag_len == 4 && strncmp("font", tag, tag_len) == 0) {
    family_data->element_stack.push(kFontElementType);
    FontFileInfo* file = &family_data->current_family->fonts.push_back();
    family_data->current_font_info = file;
    FontElementHandler(file, attribute_pairs);
  } else if (tag_len == 5 && strncmp("alias", tag, tag_len) == 0) {
    family_data->element_stack.push(kAliasElementType);
    AliasElementHandler(family_data, attribute_pairs);
  } else {
    family_data->element_stack.push(kOtherElementType);
  }
}

void EndElement(void* data, const xmlChar* xml_tag) {
  FamilyData* family_data = reinterpret_cast<FamilyData*>(data);
  const char* tag = reinterpret_cast<const char*>(xml_tag);
  size_t tag_len = strlen(tag);

  if (tag_len == 9 && strncmp("familyset", tag, tag_len) == 0) {
    FamilySetElementEndHandler(family_data);
  } else if (tag_len == 6 && strncmp("family", tag, tag_len) == 0) {
    if (family_data->current_family != NULL) {
      *family_data->families->append() = family_data->current_family.release();
    } else {
      SkDebugf("---- Encountered end family tag with no current family");
    }
  }

  family_data->element_stack.pop();
}

void Characters(void* data, const xmlChar* xml_characters, int len) {
  FamilyData* family_data = reinterpret_cast<FamilyData*>(data);
  const char* characters = reinterpret_cast<const char*>(xml_characters);

  if (family_data->element_stack.size() > 0 &&
      family_data->element_stack.top() == kFontElementType) {
    family_data->current_font_info->file_name.set(characters, len);
  }
}

void ParserWarning(void* context, const char* message, ...) {
  va_list arguments;
  va_start(arguments, message);

  SkDebugf("---- Parsing warning: %s",
           StringPrintVAndTrim(message, arguments).c_str());
}

void ParserError(void* context, const char* message, ...) {
  va_list arguments;
  va_start(arguments, message);

  SkDebugf("---- Parsing error: %s",
           StringPrintVAndTrim(message, arguments).c_str());
}

void ParserFatal(void* context, const char* message, ...) {
  va_list arguments;
  va_start(arguments, message);

  SkDebugf("---- Parsing fatal error: %s",
           StringPrintVAndTrim(message, arguments).c_str());
}

// This function parses the given filename and stores the results in the given
// families array.
void ParseConfigFile(const char* directory, const char* filename,
                     SkTDArray<FontFamily*>* families) {
  SkString file_path = SkOSPath::Join(directory, filename);

  SkAutoTUnref<SkStream> file_stream(SkStream::NewFromFile(file_path.c_str()));
  if (file_stream == NULL) {
    SkDebugf("---- Failed to open %s", file_path.c_str());
    return;
  }

  SkAutoDataUnref file_data(
      SkData::NewFromStream(file_stream, file_stream->getLength()));
  if (file_data == NULL) {
    SkDebugf("---- Failed to read %s", file_path.c_str());
    return;
  }

  FamilyData family_data(families);
  xmlSAXUserParseMemory(&xml_sax_handler, &family_data,
                        static_cast<const char*>(file_data->data()),
                        static_cast<int>(file_data->size()));
}

void GetSystemFontFamilies(const char* directory,
                           SkTDArray<FontFamily*>* font_families) {
  ParseConfigFile(directory, kSystemFontsFile, font_families);
}

}  // namespace

namespace SkFontConfigParser {

// Loads data on font families from the configuration file. The resulting data
// is returned in the given fontFamilies array.
void GetFontFamilies(const char* directory,
                     SkTDArray<FontFamily*>* font_families) {
  GetSystemFontFamilies(directory, font_families);
}

}  // namespace SkFontConfigParser
