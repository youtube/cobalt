// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/page_state/page_state_serialization.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "base/pickle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "mojo/public/cpp/base/string16_mojom_traits.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/bindings/enum_utils.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "third_party/blink/public/common/loader/http_body_element_type.h"
#include "third_party/blink/public/common/unique_name/unique_name_helper.h"
#include "third_party/blink/public/mojom/page_state/page_state.mojom.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "url/mojom/url_gurl_mojom_traits.h"

namespace blink {

namespace {

#if BUILDFLAG(IS_ANDROID)
float g_device_scale_factor_for_testing = 0.0;
#endif

//-----------------------------------------------------------------------------

void AppendDataToRequestBody(
    const scoped_refptr<network::ResourceRequestBody>& request_body,
    const char* data,
    size_t data_length) {
  request_body->AppendBytes(data, data_length);
}

void AppendFileRangeToRequestBody(
    const scoped_refptr<network::ResourceRequestBody>& request_body,
    const absl::optional<std::u16string>& file_path,
    int file_start,
    int file_length,
    base::Time file_modification_time) {
  request_body->AppendFileRange(
      file_path ? base::FilePath::FromUTF16Unsafe(*file_path)
                : base::FilePath(),
      static_cast<uint64_t>(file_start), static_cast<uint64_t>(file_length),
      file_modification_time);
}

//----------------------------------------------------------------------------

void AppendReferencedFilesFromHttpBody(
    const std::vector<network::DataElement>& elements,
    std::vector<absl::optional<std::u16string>>* referenced_files) {
  for (size_t i = 0; i < elements.size(); ++i) {
    if (elements[i].type() == network::DataElement::Tag::kFile) {
      referenced_files->emplace_back(
          elements[i].As<network::DataElementFile>().path().AsUTF16Unsafe());
    }
  }
}

bool AppendReferencedFilesFromDocumentState(
    const std::vector<absl::optional<std::u16string>>& document_state,
    std::vector<absl::optional<std::u16string>>* referenced_files) {
  if (document_state.empty())
    return true;

  // This algorithm is adapted from Blink's FormController code.
  // We only care about how that code worked when this code snapshot was taken
  // as this code is only needed for backwards compat.
  //
  // For reference, see FormController::formStatesFromStateVector in
  // third_party/WebKit/Source/core/html/forms/FormController.cpp.

  size_t index = 0;

  if (document_state.size() < 3)
    return false;

  index++;  // Skip over magic signature.
  index++;  // Skip over form key.

  size_t item_count;
  if (!document_state[index] ||
      !base::StringToSizeT(*document_state[index++], &item_count))
    return false;

  while (item_count--) {
    if (index + 1 >= document_state.size())
      return false;

    index++;  // Skip over name.
    const absl::optional<std::u16string>& type = document_state[index++];

    if (index >= document_state.size())
      return false;

    size_t value_size;
    if (!document_state[index] ||
        !base::StringToSizeT(*document_state[index++], &value_size))
      return false;

    if (index + value_size > document_state.size() ||
        index + value_size < index)  // Check for overflow.
      return false;

    if (type && base::EqualsASCII(*type, "file")) {
      if (value_size != 2)
        return false;

      referenced_files->emplace_back(document_state[index++]);
      index++;  // Skip over display name.
    } else {
      index += value_size;
    }
  }

  return true;
}

bool RecursivelyAppendReferencedFiles(
    const ExplodedFrameState& frame_state,
    std::vector<absl::optional<std::u16string>>* referenced_files) {
  if (frame_state.http_body.request_body) {
    AppendReferencedFilesFromHttpBody(
        *frame_state.http_body.request_body->elements(), referenced_files);
  }

  if (!AppendReferencedFilesFromDocumentState(frame_state.document_state,
                                              referenced_files))
    return false;

  for (size_t i = 0; i < frame_state.children.size(); ++i) {
    if (!RecursivelyAppendReferencedFiles(frame_state.children[i],
                                          referenced_files))
      return false;
  }

  return true;
}

//----------------------------------------------------------------------------

struct SerializeObject {
  SerializeObject() : version(0), parse_error(false) {}

  SerializeObject(const char* data, int len)
      : pickle(data, len), version(0), parse_error(false) {
    iter = base::PickleIterator(pickle);
  }

  std::string GetAsString() {
    return std::string(pickle.data_as_char(), pickle.size());
  }

  base::Pickle pickle;
  base::PickleIterator iter;
  int version;
  bool parse_error;
};

// IMPORTANT: When making updates to the PageState serialization code, be sure
// to first read
// https://chromium.googlesource.com/chromium/src/+/main/docs/modifying_session_history_serialization.md

// Version ID of serialized format.
// 11: Min version
// 12: Adds support for contains_passwords in HTTP body
// 13: Adds support for URL (FileSystem URL)
// 14: Adds list of referenced files, version written only for first item.
// 15: Removes a bunch of values we defined but never used.
// 16: Switched from blob urls to blob uuids.
// 17: Add a target frame id number.
// 18: Add referrer policy.
// 19: Remove target frame id, which was a bad idea, and original url string,
//         which is no longer used.
// 20: Add visual viewport scroll offset, the offset of the pinched zoomed
//     viewport within the unzoomed main frame.
// 21: Add frame sequence number.
// 22: Add scroll restoration type.
// 23: Remove frame sequence number, there are easier ways.
// 24: Add did save scroll or scale state.
// 25: Limit the length of unique names: https://crbug.com/626202
// 26: Switch to mojo-based serialization.
// 27: Add serialized scroll anchor to FrameState.
// 28: Add initiator origin to FrameState.
// 29: Add navigation API key.
// 30: Add navigation API state.
// 31: Add protect url in navigation API bit.
// 32: Fix assign() for initiator origin.
// 33: Add initiator base url to FrameState.
// NOTE: If the version is -1, then the pickle contains only a URL string.
// See ReadPageState.
//
const int kMinVersion = 11;
// NOTE: When changing the version, please add a backwards compatibility test.
// See PageStateSerializationTest.DumpExpectedPageStateForBackwardsCompat for
// instructions on how to generate the new test case.
const int kCurrentVersion = 33;

// A bunch of convenience functions to write to/read from SerializeObjects.  The
// de-serializers assume the input data will be in the correct format and fall
// back to returning safe defaults when not. These are mostly used by
// legacy(pre-mojo) serialization methods. If you're making changes to the
// PageState serialization format you almost certainly want to add/remove fields
// in page_state.mojom rather than using these methods.

void WriteData(const void* data, size_t length, SerializeObject* obj) {
  obj->pickle.WriteData(static_cast<const char*>(data), length);
}

void ReadData(SerializeObject* obj, const void** data, size_t* length) {
  const char* tmp;
  if (obj->iter.ReadData(&tmp, length)) {
    *data = tmp;
  } else {
    obj->parse_error = true;
    *data = nullptr;
    *length = 0;
  }
}

void WriteInteger(int data, SerializeObject* obj) {
  obj->pickle.WriteInt(data);
}

int ReadInteger(SerializeObject* obj) {
  int tmp;
  if (obj->iter.ReadInt(&tmp))
    return tmp;
  obj->parse_error = true;
  return 0;
}

void WriteInteger64(int64_t data, SerializeObject* obj) {
  obj->pickle.WriteInt64(data);
}

int64_t ReadInteger64(SerializeObject* obj) {
  int64_t tmp = 0;
  if (obj->iter.ReadInt64(&tmp))
    return tmp;
  obj->parse_error = true;
  return 0;
}

void WriteReal(double data, SerializeObject* obj) {
  WriteData(&data, sizeof(double), obj);
}

double ReadReal(SerializeObject* obj) {
  const void* tmp = nullptr;
  size_t length = 0;
  double value = 0.0;
  ReadData(obj, &tmp, &length);
  if (length == sizeof(double)) {
    // Use memcpy, as tmp may not be correctly aligned.
    memcpy(&value, tmp, length);
  } else {
    obj->parse_error = true;
  }
  return value;
}

void WriteBoolean(bool data, SerializeObject* obj) {
  obj->pickle.WriteInt(data ? 1 : 0);
}

bool ReadBoolean(SerializeObject* obj) {
  bool tmp;
  if (obj->iter.ReadBool(&tmp))
    return tmp;
  obj->parse_error = true;
  return false;
}

GURL ReadGURL(SerializeObject* obj) {
  std::string spec;
  if (obj->iter.ReadString(&spec))
    return GURL(spec);
  obj->parse_error = true;
  return GURL();
}

std::string ReadStdString(SerializeObject* obj) {
  std::string s;
  if (obj->iter.ReadString(&s))
    return s;
  obj->parse_error = true;
  return std::string();
}

// Pickles a std::u16string as <int length>:<char*16 data> tuple>.
void WriteString(const std::u16string& str, SerializeObject* obj) {
  obj->pickle.WriteData(reinterpret_cast<const char*>(str.data()),
                        str.length() * sizeof(char16_t));
}

// If str is a null optional, this simply pickles a length of -1. Otherwise,
// delegates to the std::u16string overload.
void WriteString(const absl::optional<std::u16string>& str,
                 SerializeObject* obj) {
  if (!str) {
    obj->pickle.WriteInt(-1);
  } else {
    WriteString(*str, obj);
  }
}

// This reads a serialized absl::optional<std::u16string> from obj. If a string
// can't be read, nullptr is returned.
const char16_t* ReadStringNoCopy(SerializeObject* obj, int* num_chars) {
  int length_in_bytes;
  if (!obj->iter.ReadInt(&length_in_bytes)) {
    obj->parse_error = true;
    return nullptr;
  }

  if (length_in_bytes < 0)  // Not an error!  See WriteString(nullopt).
    return nullptr;

  const char* data;
  if (!obj->iter.ReadBytes(&data, static_cast<size_t>(length_in_bytes))) {
    obj->parse_error = true;
    return nullptr;
  }

  if (num_chars)
    *num_chars = length_in_bytes / sizeof(char16_t);
  return reinterpret_cast<const char16_t*>(data);
}

absl::optional<std::u16string> ReadString(SerializeObject* obj) {
  int num_chars;
  const char16_t* chars = ReadStringNoCopy(obj, &num_chars);
  absl::optional<std::u16string> result;
  if (chars)
    result.emplace(chars, num_chars);
  return result;
}

template <typename T>
void WriteAndValidateVectorSize(const std::vector<T>& v, SerializeObject* obj) {
  CHECK_LT(v.size(), std::numeric_limits<int>::max() / sizeof(T));
  WriteInteger(static_cast<int>(v.size()), obj);
}

size_t ReadAndValidateVectorSize(SerializeObject* obj, size_t element_size) {
  size_t num_elements = static_cast<size_t>(ReadInteger(obj));

  // Ensure that resizing a vector to size num_elements makes sense.
  if (std::numeric_limits<int>::max() / element_size <= num_elements) {
    obj->parse_error = true;
    return 0;
  }

  // Ensure that it is plausible for the pickle to contain num_elements worth
  // of data.
  if (obj->pickle.payload_size() <= num_elements) {
    obj->parse_error = true;
    return 0;
  }

  return num_elements;
}

// Writes a Vector of strings into a SerializeObject for serialization.
void WriteStringVector(const std::vector<absl::optional<std::u16string>>& data,
                       SerializeObject* obj) {
  WriteAndValidateVectorSize(data, obj);
  for (size_t i = 0; i < data.size(); ++i) {
    WriteString(data[i], obj);
  }
}

void ReadStringVector(SerializeObject* obj,
                      std::vector<absl::optional<std::u16string>>* result) {
  size_t num_elements =
      ReadAndValidateVectorSize(obj, sizeof(absl::optional<std::u16string>));

  result->resize(num_elements);
  for (size_t i = 0; i < num_elements; ++i)
    (*result)[i] = ReadString(obj);
}

void WriteResourceRequestBody(const network::ResourceRequestBody& request_body,
                              SerializeObject* obj) {
  WriteAndValidateVectorSize(*request_body.elements(), obj);
  for (const auto& element : *request_body.elements()) {
    switch (element.type()) {
      case network::DataElement::Tag::kBytes: {
        const auto& bytes = element.As<network::DataElementBytes>().bytes();
        WriteInteger(static_cast<int>(HTTPBodyElementType::kTypeData), obj);
        WriteData(bytes.data(), bytes.size(), obj);
        break;
      }
      case network::DataElement::Tag::kFile: {
        const auto& file = element.As<network::DataElementFile>();
        WriteInteger(static_cast<int>(HTTPBodyElementType::kTypeFile), obj);
        WriteString(file.path().AsUTF16Unsafe(), obj);
        WriteInteger64(static_cast<int64_t>(file.offset()), obj);
        WriteInteger64(static_cast<int64_t>(file.length()), obj);
        WriteReal(file.expected_modification_time().ToDoubleT(), obj);
        break;
      }
      default:
        NOTREACHED();
        continue;
    }
  }
  WriteInteger64(request_body.identifier(), obj);
}

void ReadResourceRequestBody(
    SerializeObject* obj,
    const scoped_refptr<network::ResourceRequestBody>& request_body) {
  int num_elements = ReadInteger(obj);
  for (int i = 0; i < num_elements; ++i) {
    HTTPBodyElementType type =
        static_cast<HTTPBodyElementType>(ReadInteger(obj));
    if (type == HTTPBodyElementType::kTypeData) {
      const void* data;
      size_t length;
      ReadData(obj, &data, &length);
      if (!obj->parse_error) {
        AppendDataToRequestBody(request_body, static_cast<const char*>(data),
                                length);
      }
    } else if (type == HTTPBodyElementType::kTypeFile) {
      absl::optional<std::u16string> file_path = ReadString(obj);
      int64_t file_start = ReadInteger64(obj);
      int64_t file_length = ReadInteger64(obj);
      double file_modification_time = ReadReal(obj);
      AppendFileRangeToRequestBody(
          request_body, file_path, file_start, file_length,
          base::Time::FromDoubleT(file_modification_time));
    } else if (type == HTTPBodyElementType::kTypeBlob) {
      // Skip obsolete blob values.
      if (obj->version >= 16) {
        ReadStdString(obj);
      } else {
        ReadGURL(obj);
      }
    }
  }
  request_body->set_identifier(ReadInteger64(obj));
}

void ReadHttpBody(SerializeObject* obj, ExplodedHttpBody* http_body) {
  // An initial boolean indicates if we have an HTTP body.
  if (!ReadBoolean(obj))
    return;

  http_body->request_body = new network::ResourceRequestBody();
  ReadResourceRequestBody(obj, http_body->request_body);

  if (obj->version >= 12)
    http_body->contains_passwords = ReadBoolean(obj);
}

void WriteHttpBody(const ExplodedHttpBody& http_body, SerializeObject* obj) {
  bool is_null = !http_body.request_body;
  WriteBoolean(!is_null, obj);
  if (is_null)
    return;

  WriteResourceRequestBody(*http_body.request_body, obj);
  WriteBoolean(http_body.contains_passwords, obj);
}

void ReadFrameState(
    SerializeObject* obj,
    bool is_top,
    std::vector<UniqueNameHelper::Replacement>* unique_name_replacements,
    ExplodedFrameState* state) {
  if (obj->version < 14 && !is_top)
    ReadInteger(obj);  // Skip over redundant version field.

  state->url_string = ReadString(obj);

  if (obj->version < 19)
    ReadString(obj);  // Skip obsolete original url string field.

  state->target = ReadString(obj);
  if (obj->version < 25 && state->target) {
    state->target = base::UTF8ToUTF16(UniqueNameHelper::UpdateLegacyNameFromV24(
        base::UTF16ToUTF8(*state->target), unique_name_replacements));
  }
  if (obj->version < 15) {
    ReadString(obj);  // Skip obsolete parent field.
    ReadString(obj);  // Skip obsolete title field.
    ReadString(obj);  // Skip obsolete alternate title field.
    ReadReal(obj);    // Skip obsolete visited time field.
  }

  if (obj->version >= 24) {
    state->did_save_scroll_or_scale_state = ReadBoolean(obj);
  } else {
    state->did_save_scroll_or_scale_state = true;
  }

  if (state->did_save_scroll_or_scale_state) {
    int x = ReadInteger(obj);
    int y = ReadInteger(obj);
    state->scroll_offset = gfx::Point(x, y);
  }

  if (obj->version < 15) {
    ReadBoolean(obj);  // Skip obsolete target item flag.
    ReadInteger(obj);  // Skip obsolete visit count field.
  }
  state->referrer = ReadString(obj);

  ReadStringVector(obj, &state->document_state);

  if (state->did_save_scroll_or_scale_state)
    state->page_scale_factor = ReadReal(obj);

  state->item_sequence_number = ReadInteger64(obj);
  state->document_sequence_number = ReadInteger64(obj);
  if (obj->version >= 21 && obj->version < 23)
    ReadInteger64(obj);  // Skip obsolete frame sequence number.

  if (obj->version >= 17 && obj->version < 19)
    ReadInteger64(obj);  // Skip obsolete target frame id number.

  if (obj->version >= 18) {
    state->referrer_policy =
        mojo::ConvertIntToMojoEnum<network::mojom::ReferrerPolicy>(
            ReadInteger(obj))
            .value_or(network::mojom::ReferrerPolicy::kDefault);
  }

  if (obj->version >= 20 && state->did_save_scroll_or_scale_state) {
    double x = ReadReal(obj);
    double y = ReadReal(obj);
    state->visual_viewport_scroll_offset = gfx::PointF(x, y);
  } else {
    state->visual_viewport_scroll_offset = gfx::PointF(-1, -1);
  }

  if (obj->version >= 22) {
    state->scroll_restoration_type =
        static_cast<mojom::ScrollRestorationType>(ReadInteger(obj));
  }

  bool has_state_object = ReadBoolean(obj);
  if (has_state_object)
    state->state_object = ReadString(obj);

  ReadHttpBody(obj, &state->http_body);

  // NOTE: It is a quirk of the format that we still have to read the
  // http_content_type field when the HTTP body is null.  That's why this code
  // is here instead of inside ReadHttpBody.
  state->http_body.http_content_type = ReadString(obj);

  if (obj->version < 14)
    ReadString(obj);  // Skip unused referrer string.

#if BUILDFLAG(IS_ANDROID)
  if (obj->version == 11) {
    // Now-unused values that shipped in this version of Chrome for Android when
    // it was on a private branch.
    ReadReal(obj);
    ReadBoolean(obj);

    // In this version, page_scale_factor included device_scale_factor and
    // scroll offsets were premultiplied by pageScaleFactor.
    if (state->page_scale_factor) {
      float device_scale_factor = g_device_scale_factor_for_testing;
      if (!device_scale_factor) {
        device_scale_factor = display::Screen::GetScreen()
                                  ->GetPrimaryDisplay()
                                  .device_scale_factor();
      }
      state->scroll_offset =
          gfx::Point(state->scroll_offset.x() / state->page_scale_factor,
                     state->scroll_offset.y() / state->page_scale_factor);
      state->page_scale_factor /= device_scale_factor;
    }
  }
#endif

  // Subitems
  size_t num_children =
      ReadAndValidateVectorSize(obj, sizeof(ExplodedFrameState));
  state->children.resize(num_children);
  for (size_t i = 0; i < num_children; ++i)
    ReadFrameState(obj, false, unique_name_replacements, &state->children[i]);
}

// Writes the ExplodedFrameState data into the SerializeObject object for
// serialization. This uses the custom, legacy format, and its implementation
// should remain frozen in order to preserve this format.
// TODO(pnoland, dcheng) Move the legacy write methods into a test-only helper.
void WriteFrameState(const ExplodedFrameState& state,
                     SerializeObject* obj,
                     bool is_top) {
  // WARNING: This data may be persisted for later use. As such, care must be
  // taken when changing the serialized format. If a new field needs to be
  // written, only adding at the end will make it easier to deal with loading
  // older versions. Similarly, this should NOT save fields with sensitive
  // data, such as password fields.

  WriteString(state.url_string, obj);
  WriteString(state.target, obj);
  WriteBoolean(state.did_save_scroll_or_scale_state, obj);

  if (state.did_save_scroll_or_scale_state) {
    WriteInteger(state.scroll_offset.x(), obj);
    WriteInteger(state.scroll_offset.y(), obj);
  }

  WriteString(state.referrer, obj);

  WriteStringVector(state.document_state, obj);

  if (state.did_save_scroll_or_scale_state)
    WriteReal(state.page_scale_factor, obj);

  WriteInteger64(state.item_sequence_number, obj);
  WriteInteger64(state.document_sequence_number, obj);
  WriteInteger(static_cast<int>(state.referrer_policy), obj);

  if (state.did_save_scroll_or_scale_state) {
    WriteReal(state.visual_viewport_scroll_offset.x(), obj);
    WriteReal(state.visual_viewport_scroll_offset.y(), obj);
  }

  WriteInteger(static_cast<int>(state.scroll_restoration_type), obj);

  bool has_state_object = state.state_object.has_value();
  WriteBoolean(has_state_object, obj);
  if (has_state_object)
    WriteString(*state.state_object, obj);

  WriteHttpBody(state.http_body, obj);

  // NOTE: It is a quirk of the format that we still have to write the
  // http_content_type field when the HTTP body is null.  That's why this code
  // is here instead of inside WriteHttpBody.
  WriteString(state.http_body.http_content_type, obj);

  // Subitems
  const std::vector<ExplodedFrameState>& children = state.children;
  WriteAndValidateVectorSize(children, obj);
  for (size_t i = 0; i < children.size(); ++i)
    WriteFrameState(children[i], obj, false);
}

void WritePageState(const ExplodedPageState& state, SerializeObject* obj) {
  WriteInteger(obj->version, obj);
  WriteStringVector(state.referenced_files, obj);
  WriteFrameState(state.top, obj, true);
}

// Legacy read/write functions above this line. Don't change these.
//-----------------------------------------------------------------------------
// "Modern" read/write functions start here. These are probably what you want.

void WriteResourceRequestBody(const network::ResourceRequestBody& request_body,
                              mojom::RequestBody* mojo_body) {
  for (const auto& element : *request_body.elements()) {
    mojom::ElementPtr data_element;
    switch (element.type()) {
      case network::DataElement::Tag::kBytes: {
        const auto& bytes = element.As<network::DataElementBytes>().bytes();
        const char* data = reinterpret_cast<const char*>(bytes.data());
        data_element = mojom::Element::NewBytes(
            std::vector<unsigned char>(data, data + bytes.size()));
        break;
      }
      case network::DataElement::Tag::kFile: {
        const auto& element_file = element.As<network::DataElementFile>();
        mojom::FilePtr file = mojom::File::New(
            element_file.path().AsUTF16Unsafe(), element_file.offset(),
            element_file.length(), element_file.expected_modification_time());
        data_element = mojom::Element::NewFile(std::move(file));
        break;
      }
      case network::DataElement::Tag::kDataPipe:
        NOTIMPLEMENTED();
        continue;
      case network::DataElement::Tag::kChunkedDataPipe:
        NOTREACHED();
        continue;
    }
    mojo_body->elements.push_back(std::move(data_element));
  }
  mojo_body->identifier = request_body.identifier();
}

void ReadResourceRequestBody(
    mojom::RequestBody* mojo_body,
    const scoped_refptr<network::ResourceRequestBody>& request_body) {
  for (const auto& element : mojo_body->elements) {
    mojom::Element::Tag tag = element->which();
    switch (tag) {
      case mojom::Element::Tag::kBytes:
        AppendDataToRequestBody(
            request_body,
            reinterpret_cast<const char*>(element->get_bytes().data()),
            element->get_bytes().size());
        break;
      case mojom::Element::Tag::kFile: {
        mojom::File* file = element->get_file().get();
        AppendFileRangeToRequestBody(request_body, file->path, file->offset,
                                     file->length, file->modification_time);
        break;
      }
      case mojom::Element::Tag::kBlobUuid:
        // No longer supported.
        break;
      case mojom::Element::Tag::kDeprecatedFileSystemFile:
        // No longer supported.
        break;
    }
  }
  request_body->set_identifier(mojo_body->identifier);
}

void WriteHttpBody(const ExplodedHttpBody& http_body,
                   mojom::HttpBody* mojo_body) {
  if (http_body.request_body) {
    mojo_body->request_body = mojom::RequestBody::New();
    mojo_body->contains_passwords = http_body.contains_passwords;
    mojo_body->http_content_type = http_body.http_content_type;
    WriteResourceRequestBody(*http_body.request_body,
                             mojo_body->request_body.get());
  }
}

void ReadHttpBody(mojom::HttpBody* mojo_body, ExplodedHttpBody* http_body) {
  http_body->contains_passwords = mojo_body->contains_passwords;
  http_body->http_content_type = mojo_body->http_content_type;
  if (mojo_body->request_body) {
    http_body->request_body =
        base::MakeRefCounted<network::ResourceRequestBody>();
    ReadResourceRequestBody(mojo_body->request_body.get(),
                            http_body->request_body);
  }
}

// Do not depend on feature state when writing data to frame, so that the
// contents of persisted history do not depend on whether a feature is enabled
// or not.
void WriteFrameState(const ExplodedFrameState& state,
                     mojom::FrameState* frame) {
  frame->url_string = state.url_string;
  frame->referrer = state.referrer;
  if (state.initiator_origin.has_value())
    frame->initiator_origin = state.initiator_origin.value().Serialize();
  frame->initiator_base_url_string = state.initiator_base_url_string;
  frame->target = state.target;
  frame->state_object = state.state_object;

  for (const auto& s : state.document_state) {
    frame->document_state.push_back(s);
  }

  frame->scroll_restoration_type =
      static_cast<mojom::ScrollRestorationType>(state.scroll_restoration_type);

  if (state.did_save_scroll_or_scale_state) {
    frame->view_state = mojom::ViewState::New();
    frame->view_state->scroll_offset = state.scroll_offset;
    frame->view_state->visual_viewport_scroll_offset =
        state.visual_viewport_scroll_offset;
    frame->view_state->page_scale_factor = state.page_scale_factor;
    // We discard all scroll anchor data if the selector is over the length
    // limit. We don't want to bloat the size of FrameState, and the other
    // fields are useless without the selector.
    if (state.scroll_anchor_selector && state.scroll_anchor_selector->length() <
                                            kMaxScrollAnchorSelectorLength) {
      frame->view_state->scroll_anchor_selector = state.scroll_anchor_selector;
      frame->view_state->scroll_anchor_offset = state.scroll_anchor_offset;
      frame->view_state->scroll_anchor_simhash = state.scroll_anchor_simhash;
    }
  }

  frame->item_sequence_number = state.item_sequence_number;
  frame->document_sequence_number = state.document_sequence_number;

  frame->referrer_policy = state.referrer_policy;

  frame->http_body = mojom::HttpBody::New();
  WriteHttpBody(state.http_body, frame->http_body.get());

  frame->navigation_api_key = state.navigation_api_key;
  frame->navigation_api_id = state.navigation_api_id;
  frame->navigation_api_state = state.navigation_api_state;
  frame->protect_url_in_navigation_api = state.protect_url_in_navigation_api;

  // Subitems
  const std::vector<ExplodedFrameState>& children = state.children;
  for (const auto& child : children) {
    mojom::FrameStatePtr child_frame = mojom::FrameState::New();
    WriteFrameState(child, child_frame.get());
    frame->children.push_back(std::move(child_frame));
  }
}

void ReadFrameState(mojom::FrameState* frame, ExplodedFrameState* state) {
  state->url_string = frame->url_string;
  state->referrer = frame->referrer;
  if (frame->initiator_origin.has_value()) {
    state->initiator_origin =
        url::Origin::Create(GURL(frame->initiator_origin.value()));
  }
  state->initiator_base_url_string = frame->initiator_base_url_string;

  state->target = frame->target;
  state->state_object = frame->state_object;

  for (const auto& s : frame->document_state) {
    state->document_state.push_back(s);
  }

  state->scroll_restoration_type =
      static_cast<mojom::ScrollRestorationType>(frame->scroll_restoration_type);

  if (frame->view_state) {
    state->did_save_scroll_or_scale_state = true;
    state->visual_viewport_scroll_offset =
        frame->view_state->visual_viewport_scroll_offset;
    state->scroll_offset = frame->view_state->scroll_offset;
    state->page_scale_factor = frame->view_state->page_scale_factor;
  }

  if (frame->view_state) {
    state->scroll_anchor_selector = frame->view_state->scroll_anchor_selector;
    state->scroll_anchor_offset =
        frame->view_state->scroll_anchor_offset.value_or(gfx::PointF());
    state->scroll_anchor_simhash = frame->view_state->scroll_anchor_simhash;
  }

  state->item_sequence_number = frame->item_sequence_number;
  state->document_sequence_number = frame->document_sequence_number;

  state->referrer_policy = frame->referrer_policy;
  if (frame->http_body) {
    ReadHttpBody(frame->http_body.get(), &state->http_body);
  } else {
    state->http_body.request_body = nullptr;
  }

  state->navigation_api_key = frame->navigation_api_key;
  state->navigation_api_id = frame->navigation_api_id;
  state->navigation_api_state = frame->navigation_api_state;
  state->protect_url_in_navigation_api = frame->protect_url_in_navigation_api;

  state->children.resize(frame->children.size());
  int i = 0;
  for (const auto& child : frame->children)
    ReadFrameState(child.get(), &state->children[i++]);
}

void ReadMojoPageState(SerializeObject* obj, ExplodedPageState* state) {
  const void* tmp = nullptr;
  size_t length = 0;
  ReadData(obj, &tmp, &length);
  DCHECK_GT(length, 0u);
  if (obj->parse_error)
    return;

  mojom::PageStatePtr page;
  obj->parse_error = !(mojom::PageState::Deserialize(tmp, length, &page));
  if (obj->parse_error)
    return;

  for (const auto& referenced_file : page->referenced_files) {
    state->referenced_files.push_back(referenced_file);
  }

  ReadFrameState(page->top.get(), &state->top);

  state->referenced_files.erase(std::unique(state->referenced_files.begin(),
                                            state->referenced_files.end()),
                                state->referenced_files.end());
}

void WriteMojoPageState(const ExplodedPageState& state, SerializeObject* obj) {
  WriteInteger(obj->version, obj);

  mojom::PageStatePtr page = mojom::PageState::New();
  for (const auto& referenced_file : state.referenced_files) {
    page->referenced_files.push_back(referenced_file.value());
  }

  page->top = mojom::FrameState::New();
  WriteFrameState(state.top, page->top.get());

  std::vector<uint8_t> page_bytes = mojom::PageState::Serialize(&page);
  obj->pickle.WriteData(reinterpret_cast<char*>(page_bytes.data()),
                        page_bytes.size());
}

void ReadPageState(SerializeObject* obj, ExplodedPageState* state) {
  obj->version = ReadInteger(obj);

  if (obj->version == -1) {
    GURL url = ReadGURL(obj);
    // NOTE: GURL::possibly_invalid_spec() always returns valid UTF-8.
    state->top.url_string = base::UTF8ToUTF16(url.possibly_invalid_spec());
    return;
  }

  if (obj->version > kCurrentVersion || obj->version < kMinVersion) {
    obj->parse_error = true;
    return;
  }

  if (obj->version >= 26) {
    ReadMojoPageState(obj, state);
    return;
  }

  if (obj->version >= 14)
    ReadStringVector(obj, &state->referenced_files);

  std::vector<UniqueNameHelper::Replacement> unique_name_replacements;
  ReadFrameState(obj, true, &unique_name_replacements, &state->top);

  if (obj->version < 14)
    RecursivelyAppendReferencedFiles(state->top, &state->referenced_files);

  // De-dupe
  state->referenced_files.erase(std::unique(state->referenced_files.begin(),
                                            state->referenced_files.end()),
                                state->referenced_files.end());
}

}  // namespace

ExplodedHttpBody::ExplodedHttpBody() : contains_passwords(false) {}

ExplodedHttpBody::~ExplodedHttpBody() {}

ExplodedFrameState::ExplodedFrameState() = default;

ExplodedFrameState::ExplodedFrameState(const ExplodedFrameState& other) {
  assign(other);
}

ExplodedFrameState::~ExplodedFrameState() {}

void ExplodedFrameState::operator=(const ExplodedFrameState& other) {
  if (&other != this)
    assign(other);
}

// All members of ExplodedFrameState should be copied.
void ExplodedFrameState::assign(const ExplodedFrameState& other) {
  url_string = other.url_string;
  referrer = other.referrer;
  initiator_origin = other.initiator_origin;
  initiator_base_url_string = other.initiator_base_url_string;
  target = other.target;
  state_object = other.state_object;
  document_state = other.document_state;
  scroll_restoration_type = other.scroll_restoration_type;
  did_save_scroll_or_scale_state = other.did_save_scroll_or_scale_state;
  visual_viewport_scroll_offset = other.visual_viewport_scroll_offset;
  scroll_offset = other.scroll_offset;
  item_sequence_number = other.item_sequence_number;
  document_sequence_number = other.document_sequence_number;
  page_scale_factor = other.page_scale_factor;
  referrer_policy = other.referrer_policy;
  http_body = other.http_body;
  scroll_anchor_selector = other.scroll_anchor_selector;
  scroll_anchor_offset = other.scroll_anchor_offset;
  scroll_anchor_simhash = other.scroll_anchor_simhash;
  navigation_api_key = other.navigation_api_key;
  navigation_api_id = other.navigation_api_id;
  navigation_api_state = other.navigation_api_state;
  protect_url_in_navigation_api = other.protect_url_in_navigation_api;
  children = other.children;
}

ExplodedPageState::ExplodedPageState() {}

ExplodedPageState::~ExplodedPageState() {}

int DecodePageStateInternal(const std::string& encoded,
                            ExplodedPageState* exploded) {
  *exploded = ExplodedPageState();

  if (encoded.empty())
    return true;

  SerializeObject obj(encoded.data(), static_cast<int>(encoded.size()));
  ReadPageState(&obj, exploded);
  return obj.parse_error ? -1 : obj.version;
}

bool DecodePageState(const std::string& encoded, ExplodedPageState* exploded) {
  return DecodePageStateInternal(encoded, exploded) != -1;
}

int DecodePageStateForTesting(const std::string& encoded,
                              ExplodedPageState* exploded) {
  return DecodePageStateInternal(encoded, exploded);
}

void EncodePageState(const ExplodedPageState& exploded, std::string* encoded) {
  SerializeObject obj;
  obj.version = kCurrentVersion;
  WriteMojoPageState(exploded, &obj);
  *encoded = obj.GetAsString();
  DCHECK(!encoded->empty());
}

void LegacyEncodePageStateForTesting(const ExplodedPageState& exploded,
                                     int version,
                                     std::string* encoded) {
  SerializeObject obj;
  obj.version = version;
  WritePageState(exploded, &obj);
  *encoded = obj.GetAsString();
}

#if BUILDFLAG(IS_ANDROID)
bool DecodePageStateWithDeviceScaleFactorForTesting(
    const std::string& encoded,
    float device_scale_factor,
    ExplodedPageState* exploded) {
  g_device_scale_factor_for_testing = device_scale_factor;
  bool rv = DecodePageState(encoded, exploded);
  g_device_scale_factor_for_testing = 0.0;
  return rv;
}

scoped_refptr<network::ResourceRequestBody> DecodeResourceRequestBody(
    const char* data,
    size_t size) {
  scoped_refptr<network::ResourceRequestBody> result =
      new network::ResourceRequestBody();
  SerializeObject obj(data, static_cast<int>(size));
  ReadResourceRequestBody(&obj, result);
  // Please see the EncodeResourceRequestBody() function below for information
  // about why the contains_sensitive_info() field is being explicitly
  // deserialized.
  result->set_contains_sensitive_info(ReadBoolean(&obj));
  return obj.parse_error ? nullptr : result;
}

std::string EncodeResourceRequestBody(
    const network::ResourceRequestBody& resource_request_body) {
  SerializeObject obj;
  obj.version = 25;
  WriteResourceRequestBody(resource_request_body, &obj);
  // EncodeResourceRequestBody() is different from WriteResourceRequestBody()
  // because it covers additional data (e.g.|contains_sensitive_info|) which
  // is marshaled between native code and java. WriteResourceRequestBody()
  // serializes data which needs to be saved out to disk.
  WriteBoolean(resource_request_body.contains_sensitive_info(), &obj);
  return obj.GetAsString();
}

#endif

}  // namespace blink
