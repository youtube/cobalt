// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#ifndef V8_WASM_MODULE_DECODER_IMPL_H_
#define V8_WASM_MODULE_DECODER_IMPL_H_

#include "src/base/platform/wrappers.h"
#include "src/logging/counters.h"
#include "src/strings/unicode.h"
#include "src/utils/ostreams.h"
#include "src/wasm/canonical-types.h"
#include "src/wasm/constant-expression-interface.h"
#include "src/wasm/function-body-decoder-impl.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-subtyping.h"
#include "src/wasm/well-known-imports.h"

namespace v8::internal::wasm {

#define TRACE(...)                                        \
  do {                                                    \
    if (v8_flags.trace_wasm_decoder) PrintF(__VA_ARGS__); \
  } while (false)

constexpr char kNameString[] = "name";
constexpr char kSourceMappingURLString[] = "sourceMappingURL";
constexpr char kInstTraceString[] = "metadata.code.trace_inst";
constexpr char kCompilationHintsString[] = "compilationHints";
constexpr char kBranchHintsString[] = "metadata.code.branch_hint";
constexpr char kDebugInfoString[] = ".debug_info";
constexpr char kExternalDebugInfoString[] = "external_debug_info";

inline const char* ExternalKindName(ImportExportKindCode kind) {
  switch (kind) {
    case kExternalFunction:
      return "function";
    case kExternalTable:
      return "table";
    case kExternalMemory:
      return "memory";
    case kExternalGlobal:
      return "global";
    case kExternalTag:
      return "tag";
  }
  return "unknown";
}

inline bool validate_utf8(Decoder* decoder, WireBytesRef string) {
  return unibrow::Utf8::ValidateEncoding(
      decoder->start() + decoder->GetBufferRelativeOffset(string.offset()),
      string.length());
}

// Reads a length-prefixed string, checking that it is within bounds. Returns
// the offset of the string, and the length as an out parameter.
inline WireBytesRef consume_string(Decoder* decoder,
                                   unibrow::Utf8Variant grammar,
                                   const char* name, ITracer* tracer) {
  if (tracer) tracer->Description(name);
  uint32_t length = decoder->consume_u32v(" length:", tracer);
  if (tracer) {
    tracer->Description(length);
    tracer->NextLine();
  }
  uint32_t offset = decoder->pc_offset();
  const byte* string_start = decoder->pc();
  // Consume bytes before validation to guarantee that the string is not oob.
  if (length > 0) {
    if (tracer) {
      tracer->Bytes(decoder->pc(), length);
      tracer->Description(name);
      tracer->Description(": ");
      tracer->Description(reinterpret_cast<const char*>(decoder->pc()), length);
      tracer->NextLine();
    }
    decoder->consume_bytes(length, name);
    if (decoder->ok()) {
      switch (grammar) {
        case unibrow::Utf8Variant::kLossyUtf8:
          break;
        case unibrow::Utf8Variant::kUtf8:
          if (!unibrow::Utf8::ValidateEncoding(string_start, length)) {
            decoder->errorf(string_start, "%s: no valid UTF-8 string", name);
          }
          break;
        case unibrow::Utf8Variant::kWtf8:
          if (!unibrow::Wtf8::ValidateEncoding(string_start, length)) {
            decoder->errorf(string_start, "%s: no valid WTF-8 string", name);
          }
          break;
        case unibrow::Utf8Variant::kUtf8NoTrap:
          UNREACHABLE();
      }
    }
  }
  return {offset, decoder->failed() ? 0 : length};
}

inline WireBytesRef consume_string(Decoder* decoder,
                                   unibrow::Utf8Variant grammar,
                                   const char* name) {
  return consume_string(decoder, grammar, name, ITracer::NoTrace);
}

inline WireBytesRef consume_utf8_string(Decoder* decoder, const char* name,
                                        ITracer* tracer) {
  return consume_string(decoder, unibrow::Utf8Variant::kUtf8, name, tracer);
}

inline SectionCode IdentifyUnknownSectionInternal(Decoder* decoder,
                                                  ITracer* tracer) {
  WireBytesRef string = consume_utf8_string(decoder, "section name", tracer);
  if (decoder->failed()) {
    return kUnknownSectionCode;
  }
  const byte* section_name_start =
      decoder->start() + decoder->GetBufferRelativeOffset(string.offset());

  TRACE("  +%d  section name        : \"%.*s\"\n",
        static_cast<int>(section_name_start - decoder->start()),
        string.length() < 20 ? string.length() : 20, section_name_start);

  using SpecialSectionPair = std::pair<base::Vector<const char>, SectionCode>;
  static constexpr SpecialSectionPair kSpecialSections[]{
      {base::StaticCharVector(kNameString), kNameSectionCode},
      {base::StaticCharVector(kSourceMappingURLString),
       kSourceMappingURLSectionCode},
      {base::StaticCharVector(kInstTraceString), kInstTraceSectionCode},
      {base::StaticCharVector(kCompilationHintsString),
       kCompilationHintsSectionCode},
      {base::StaticCharVector(kBranchHintsString), kBranchHintsSectionCode},
      {base::StaticCharVector(kDebugInfoString), kDebugInfoSectionCode},
      {base::StaticCharVector(kExternalDebugInfoString),
       kExternalDebugInfoSectionCode}};

  auto name_vec = base::Vector<const char>::cast(
      base::VectorOf(section_name_start, string.length()));
  for (auto& special_section : kSpecialSections) {
    if (name_vec == special_section.first) return special_section.second;
  }

  return kUnknownSectionCode;
}

// An iterator over the sections in a wasm binary module.
// Automatically skips all unknown sections.
class WasmSectionIterator {
 public:
  explicit WasmSectionIterator(Decoder* decoder, ITracer* tracer)
      : decoder_(decoder),
        tracer_(tracer),
        section_code_(kUnknownSectionCode),
        section_start_(decoder->pc()),
        section_end_(decoder->pc()) {
    next();
  }

  bool more() const { return decoder_->ok() && decoder_->more(); }

  SectionCode section_code() const { return section_code_; }

  const byte* section_start() const { return section_start_; }

  uint32_t section_length() const {
    return static_cast<uint32_t>(section_end_ - section_start_);
  }

  base::Vector<const uint8_t> payload() const {
    return {payload_start_, payload_length()};
  }

  const byte* payload_start() const { return payload_start_; }

  uint32_t payload_length() const {
    return static_cast<uint32_t>(section_end_ - payload_start_);
  }

  const byte* section_end() const { return section_end_; }

  // Advances to the next section, checking that decoding the current section
  // stopped at {section_end_}.
  void advance(bool move_to_section_end = false) {
    if (move_to_section_end && decoder_->pc() < section_end_) {
      decoder_->consume_bytes(
          static_cast<uint32_t>(section_end_ - decoder_->pc()));
    }
    if (decoder_->pc() != section_end_) {
      const char* msg = decoder_->pc() < section_end_ ? "shorter" : "longer";
      decoder_->errorf(decoder_->pc(),
                       "section was %s than expected size "
                       "(%u bytes expected, %zu decoded)",
                       msg, section_length(),
                       static_cast<size_t>(decoder_->pc() - section_start_));
    }
    next();
  }

 private:
  Decoder* decoder_;
  ITracer* tracer_;
  SectionCode section_code_;
  const byte* section_start_;
  const byte* payload_start_;
  const byte* section_end_;

  // Reads the section code/name at the current position and sets up
  // the embedder fields.
  void next() {
    if (!decoder_->more()) {
      section_code_ = kUnknownSectionCode;
      return;
    }
    section_start_ = decoder_->pc();
    // Empty line before next section.
    if (tracer_) tracer_->NextLine();
    uint8_t section_code = decoder_->consume_u8("section kind: ", tracer_);
    if (tracer_) {
      tracer_->Description(SectionName(static_cast<SectionCode>(section_code)));
      tracer_->NextLine();
    }
    // Read and check the section size.
    uint32_t section_length = decoder_->consume_u32v("section length", tracer_);
    if (tracer_) {
      tracer_->Description(section_length);
      tracer_->NextLine();
    }
    payload_start_ = decoder_->pc();
    section_end_ = payload_start_ + section_length;
    if (section_length > decoder_->available_bytes()) {
      decoder_->errorf(
          section_start_,
          "section (code %u, \"%s\") extends past end of the module "
          "(length %u, remaining bytes %u)",
          section_code, SectionName(static_cast<SectionCode>(section_code)),
          section_length, decoder_->available_bytes());
      section_end_ = payload_start_;
    }

    if (section_code == kUnknownSectionCode) {
      // Check for the known "name", "sourceMappingURL", or "compilationHints"
      // section.
      // To identify the unknown section we set the end of the decoder bytes to
      // the end of the custom section, so that we do not read the section name
      // beyond the end of the section.
      const byte* module_end = decoder_->end();
      decoder_->set_end(section_end_);
      section_code = IdentifyUnknownSectionInternal(decoder_, tracer_);
      if (decoder_->ok()) decoder_->set_end(module_end);
      // As a side effect, the above function will forward the decoder to after
      // the identifier string.
      payload_start_ = decoder_->pc();
    } else if (!IsValidSectionCode(section_code)) {
      decoder_->errorf(decoder_->pc(), "unknown section code #0x%02x",
                       section_code);
    }
    section_code_ = decoder_->failed() ? kUnknownSectionCode
                                       : static_cast<SectionCode>(section_code);

    if (section_code_ == kUnknownSectionCode && section_end_ > decoder_->pc()) {
      // Skip to the end of the unknown section.
      uint32_t remaining = static_cast<uint32_t>(section_end_ - decoder_->pc());
      decoder_->consume_bytes(remaining, "section payload", tracer_);
    }
  }
};

inline void DumpModule(const base::Vector<const byte> module_bytes, bool ok) {
  std::string path;
  if (v8_flags.dump_wasm_module_path) {
    path = v8_flags.dump_wasm_module_path;
    if (path.size() && !base::OS::isDirectorySeparator(path[path.size() - 1])) {
      path += base::OS::DirectorySeparator();
    }
  }
  // File are named `<hash>.{ok,failed}.wasm`.
  // Limit the hash to 8 characters (32 bits).
  uint32_t hash = static_cast<uint32_t>(GetWireBytesHash(module_bytes));
  base::EmbeddedVector<char, 32> buf;
  SNPrintF(buf, "%08x.%s.wasm", hash, ok ? "ok" : "failed");
  path += buf.begin();
  size_t rv = 0;
  if (FILE* file = base::OS::FOpen(path.c_str(), "wb")) {
    rv = fwrite(module_bytes.begin(), module_bytes.length(), 1, file);
    base::Fclose(file);
  }
  if (rv != 1) {
    OFStream os(stderr);
    os << "Error while dumping wasm file to " << path << std::endl;
  }
}

// The main logic for decoding the bytes of a module.
class ModuleDecoderImpl : public Decoder {
 public:
  ModuleDecoderImpl(WasmFeatures enabled_features,
                    base::Vector<const uint8_t> wire_bytes, ModuleOrigin origin,
                    ITracer* tracer = ITracer::NoTrace)
      : Decoder(wire_bytes),
        enabled_features_(enabled_features),
        module_(std::make_shared<WasmModule>(origin)),
        module_start_(wire_bytes.begin()),
        module_end_(wire_bytes.end()),
        tracer_(tracer) {}

  void onFirstError() override {
    pc_ = end_;  // On error, terminate section decoding loop.
  }

  void DecodeModuleHeader(base::Vector<const uint8_t> bytes) {
    if (failed()) return;
    Reset(bytes);

    const byte* pos = pc_;
    uint32_t magic_word = consume_u32("wasm magic", tracer_);
    if (tracer_) tracer_->NextLine();
#define BYTES(x) (x & 0xFF), (x >> 8) & 0xFF, (x >> 16) & 0xFF, (x >> 24) & 0xFF
    if (magic_word != kWasmMagic) {
      errorf(pos,
             "expected magic word %02x %02x %02x %02x, "
             "found %02x %02x %02x %02x",
             BYTES(kWasmMagic), BYTES(magic_word));
    }

    pos = pc_;
    {
      uint32_t magic_version = consume_u32("wasm version", tracer_);
      if (tracer_) tracer_->NextLine();
      if (magic_version != kWasmVersion) {
        errorf(pos,
               "expected version %02x %02x %02x %02x, "
               "found %02x %02x %02x %02x",
               BYTES(kWasmVersion), BYTES(magic_version));
      }
    }
#undef BYTES
  }

  bool CheckSectionOrder(SectionCode section_code) {
    // Check the order of ordered sections.
    if (section_code >= kFirstSectionInModule &&
        section_code < kFirstUnorderedSection) {
      if (section_code < next_ordered_section_) {
        errorf(pc(), "unexpected section <%s>", SectionName(section_code));
        return false;
      }
      next_ordered_section_ = section_code + 1;
      return true;
    }

    // Ignore ordering problems in unknown / custom sections. Even allow them to
    // appear multiple times. As optional sections we use them on a "best
    // effort" basis.
    if (section_code == kUnknownSectionCode) return true;
    if (section_code > kLastKnownModuleSection) return true;

    // The rest is standardized unordered sections; they are checked more
    // thoroughly..
    DCHECK_LE(kFirstUnorderedSection, section_code);
    DCHECK_GE(kLastKnownModuleSection, section_code);

    // Check that unordered sections don't appear multiple times.
    if (has_seen_unordered_section(section_code)) {
      errorf(pc(), "Multiple %s sections not allowed",
             SectionName(section_code));
      return false;
    }
    set_seen_unordered_section(section_code);

    // Define a helper to ensure that sections <= {before} appear before the
    // current unordered section, and everything >= {after} appears after it.
    auto check_order = [this, section_code](SectionCode before,
                                            SectionCode after) -> bool {
      DCHECK_LT(before, after);
      if (next_ordered_section_ > after) {
        errorf(pc(), "The %s section must appear before the %s section",
               SectionName(section_code), SectionName(after));
        return false;
      }
      if (next_ordered_section_ <= before) next_ordered_section_ = before + 1;
      return true;
    };

    // Now check the ordering constraints of specific unordered sections.
    switch (section_code) {
      case kDataCountSectionCode:
        return check_order(kElementSectionCode, kCodeSectionCode);
      case kTagSectionCode:
        return check_order(kMemorySectionCode, kGlobalSectionCode);
      case kStringRefSectionCode:
        // TODO(12868): If there's a tag section, assert that we're after the
        // tag section.
        return check_order(kMemorySectionCode, kGlobalSectionCode);
      case kInstTraceSectionCode:
        // Custom section following code.metadata tool convention containing
        // offsets specifying where trace marks should be emitted.
        // Be lenient with placement of instruction trace section. All except
        // first occurrence after function section and before code section are
        // ignored.
        return true;
      default:
        return true;
    }
  }

  void DecodeSection(SectionCode section_code,
                     base::Vector<const uint8_t> bytes, uint32_t offset) {
    if (failed()) return;
    Reset(bytes, offset);
    TRACE("Section: %s\n", SectionName(section_code));
    TRACE("Decode Section %p - %p\n", bytes.begin(), bytes.end());

    if (!CheckSectionOrder(section_code)) return;

    switch (section_code) {
      case kUnknownSectionCode:
        break;
      case kTypeSectionCode:
        DecodeTypeSection();
        break;
      case kImportSectionCode:
        DecodeImportSection();
        break;
      case kFunctionSectionCode:
        DecodeFunctionSection();
        break;
      case kTableSectionCode:
        DecodeTableSection();
        break;
      case kMemorySectionCode:
        DecodeMemorySection();
        break;
      case kGlobalSectionCode:
        DecodeGlobalSection();
        break;
      case kExportSectionCode:
        DecodeExportSection();
        break;
      case kStartSectionCode:
        DecodeStartSection();
        break;
      case kCodeSectionCode:
        DecodeCodeSection();
        break;
      case kElementSectionCode:
        DecodeElementSection();
        break;
      case kDataSectionCode:
        DecodeDataSection();
        break;
      case kNameSectionCode:
        DecodeNameSection();
        break;
      case kSourceMappingURLSectionCode:
        DecodeSourceMappingURLSection();
        break;
      case kDebugInfoSectionCode:
        // If there is an explicit source map, prefer it over DWARF info.
        if (module_->debug_symbols.type == WasmDebugSymbols::Type::None) {
          module_->debug_symbols = {WasmDebugSymbols::Type::EmbeddedDWARF, {}};
        }
        consume_bytes(static_cast<uint32_t>(end_ - start_), ".debug_info");
        break;
      case kExternalDebugInfoSectionCode:
        DecodeExternalDebugInfoSection();
        break;
      case kInstTraceSectionCode:
        if (enabled_features_.has_instruction_tracing()) {
          DecodeInstTraceSection();
        } else {
          // Ignore this section when feature is disabled. It is an optional
          // custom section anyways.
          consume_bytes(static_cast<uint32_t>(end_ - start_), nullptr);
        }
        break;
      case kCompilationHintsSectionCode:
        if (enabled_features_.has_compilation_hints()) {
          DecodeCompilationHintsSection();
        } else {
          // Ignore this section when feature was disabled. It is an optional
          // custom section anyways.
          consume_bytes(static_cast<uint32_t>(end_ - start_), nullptr);
        }
        break;
      case kBranchHintsSectionCode:
        if (enabled_features_.has_branch_hinting()) {
          DecodeBranchHintsSection();
        } else {
          // Ignore this section when feature was disabled. It is an optional
          // custom section anyways.
          consume_bytes(static_cast<uint32_t>(end_ - start_), nullptr);
        }
        break;
      case kDataCountSectionCode:
        DecodeDataCountSection();
        break;
      case kTagSectionCode:
        DecodeTagSection();
        break;
      case kStringRefSectionCode:
        if (enabled_features_.has_stringref()) {
          DecodeStringRefSection();
        } else {
          errorf(pc(),
                 "unexpected section <%s> (enable with "
                 "--experimental-wasm-stringref)",
                 SectionName(section_code));
        }
        break;
      default:
        errorf(pc(), "unexpected section <%s>", SectionName(section_code));
        return;
    }

    if (pc() != bytes.end()) {
      const char* msg = pc() < bytes.end() ? "shorter" : "longer";
      errorf(pc(),
             "section was %s than expected size "
             "(%zu bytes expected, %zu decoded)",
             msg, bytes.size(), static_cast<size_t>(pc() - bytes.begin()));
    }
  }

  static constexpr const char* TypeKindName(uint8_t kind) {
    switch (kind) {
      // clang-format off
      case kWasmFunctionTypeCode:    return "func";
      case kWasmStructTypeCode:      return "struct";
      case kWasmArrayTypeCode:       return "array";
      default:                       return "unknown";
        // clang-format on
    }
  }

  TypeDefinition consume_base_type_definition() {
    DCHECK(enabled_features_.has_gc());
    uint8_t kind = consume_u8(" kind: ", tracer_);
    if (tracer_) tracer_->Description(TypeKindName(kind));
    switch (kind) {
      case kWasmFunctionTypeCode: {
        const FunctionSig* sig = consume_sig(&module_->signature_zone);
        return {sig, kNoSuperType, v8_flags.wasm_final_types};
      }
      case kWasmStructTypeCode: {
        const StructType* type = consume_struct(&module_->signature_zone);
        return {type, kNoSuperType, v8_flags.wasm_final_types};
      }
      case kWasmArrayTypeCode: {
        const ArrayType* type = consume_array(&module_->signature_zone);
        return {type, kNoSuperType, v8_flags.wasm_final_types};
      }
      default:
        if (tracer_) tracer_->NextLine();
        errorf(pc() - 1, "unknown type form: %d", kind);
        return {};
    }
  }

  TypeDefinition consume_subtype_definition() {
    DCHECK(enabled_features_.has_gc());
    uint8_t kind = read_u8<Decoder::FullValidationTag>(pc(), "type kind");
    if (kind == kWasmSubtypeCode || kind == kWasmSubtypeFinalCode) {
      bool is_final =
          v8_flags.wasm_final_types && kind == kWasmSubtypeFinalCode;
      consume_bytes(1, is_final ? " subtype final, " : " subtype extensible, ",
                    tracer_);
      constexpr uint32_t kMaximumSupertypes = 1;
      uint32_t supertype_count =
          consume_count("supertype count", kMaximumSupertypes);
      uint32_t supertype = kNoSuperType;
      if (supertype_count == 1) {
        supertype = consume_u32v("supertype", tracer_);
        if (supertype >= kV8MaxWasmTypes) {
          errorf(
              "supertype %u is greater than the maximum number of type "
              "definitions %zu supported by V8",
              supertype, kV8MaxWasmTypes);
          return {};
        }
        if (tracer_) {
          tracer_->Description(supertype);
          tracer_->NextLine();
        }
      }
      TypeDefinition type = consume_base_type_definition();
      type.supertype = supertype;
      type.is_final = is_final;
      return type;
    } else {
      return consume_base_type_definition();
    }
  }

  void DecodeTypeSection() {
    TypeCanonicalizer* type_canon = GetTypeCanonicalizer();
    uint32_t types_count = consume_count("types count", kV8MaxWasmTypes);

    // Non wasm-gc type section decoding.
    if (!enabled_features_.has_gc()) {
      module_->types.resize(types_count);
      module_->isorecursive_canonical_type_ids.resize(types_count);
      for (uint32_t i = 0; i < types_count; ++i) {
        TRACE("DecodeSignature[%d] module+%d\n", i,
              static_cast<int>(pc_ - start_));
        uint8_t opcode =
            read_u8<FullValidationTag>(pc(), "signature definition");
        if (tracer_) {
          tracer_->Bytes(pc_, 1);
          tracer_->TypeOffset(pc_offset());
          tracer_->Description(" kind: ");
          tracer_->Description(TypeKindName(opcode));
          tracer_->NextLine();
        }
        switch (opcode) {
          case kWasmFunctionTypeCode: {
            consume_bytes(1, "function");
            const FunctionSig* sig = consume_sig(&module_->signature_zone);
            if (!ok()) break;
            module_->types[i] = {sig, kNoSuperType, v8_flags.wasm_final_types};
            type_canon->AddRecursiveGroup(module_.get(), 1, i);
            break;
          }
          case kWasmArrayTypeCode:
          case kWasmStructTypeCode:
          case kWasmSubtypeCode:
          case kWasmSubtypeFinalCode:
          case kWasmRecursiveTypeGroupCode:
            errorf(
                "Unknown type code 0x%02x, enable with --experimental-wasm-gc",
                opcode);
            return;
          default:
            errorf("Expected signature definition 0x%02x, got 0x%02x",
                   kWasmFunctionTypeCode, opcode);
            return;
        }
      }
      return;
    }

    for (uint32_t i = 0; ok() && i < types_count; ++i) {
      TRACE("DecodeType[%d] module+%d\n", i, static_cast<int>(pc_ - start_));
      uint8_t kind = read_u8<Decoder::FullValidationTag>(pc(), "type kind");
      size_t initial_size = module_->types.size();
      if (kind == kWasmRecursiveTypeGroupCode) {
        consume_bytes(1, "rec. group definition", tracer_);
        if (tracer_) tracer_->NextLine();
        uint32_t group_size =
            consume_count("recursive group size", kV8MaxWasmTypes);
        if (initial_size + group_size > kV8MaxWasmTypes) {
          errorf(pc(), "Type definition count exceeds maximum %zu",
                 kV8MaxWasmTypes);
          return;
        }
        // We need to resize types before decoding the type definitions in this
        // group, so that the correct type size is visible to type definitions.
        module_->types.resize(initial_size + group_size);
        module_->isorecursive_canonical_type_ids.resize(initial_size +
                                                        group_size);
        for (uint32_t j = 0; j < group_size; j++) {
          if (tracer_) tracer_->TypeOffset(pc_offset());
          TypeDefinition type = consume_subtype_definition();
          if (ok()) module_->types[initial_size + j] = type;
        }
        if (ok()) {
          type_canon->AddRecursiveGroup(module_.get(), group_size,
                                        static_cast<uint32_t>(initial_size));
        }
      } else {
        if (tracer_) tracer_->TypeOffset(pc_offset());
        // Similarly to above, we need to resize types for a group of size 1.
        module_->types.resize(initial_size + 1);
        module_->isorecursive_canonical_type_ids.resize(initial_size + 1);
        TypeDefinition type = consume_subtype_definition();
        if (ok()) {
          module_->types[initial_size] = type;
          type_canon->AddRecursiveGroup(module_.get(), 1);
        }
      }
    }

    // Check validity of explicitly defined supertypes.
    const WasmModule* module = module_.get();
    for (uint32_t i = 0; ok() && i < module_->types.size(); ++i) {
      uint32_t explicit_super = module_->supertype(i);
      if (explicit_super == kNoSuperType) continue;
      if (explicit_super >= module_->types.size()) {
        errorf("type %u: supertype %u out of bounds", i, explicit_super);
        continue;
      }
      if (explicit_super >= i) {
        errorf("type %u: forward-declared supertype %u", i, explicit_super);
        continue;
      }
      int depth = GetSubtypingDepth(module, i);
      DCHECK_GE(depth, 0);
      if (depth > static_cast<int>(kV8MaxRttSubtypingDepth)) {
        errorf("type %u: subtyping depth is greater than allowed", i);
        continue;
      }
      // This check is technically redundant; we include for the improved error
      // message.
      if (module->types[explicit_super].is_final) {
        errorf("type %u extends final type %u", i, explicit_super);
        continue;
      }
      if (!ValidSubtypeDefinition(i, explicit_super, module, module)) {
        errorf("type %u has invalid explicit supertype %u", i, explicit_super);
        continue;
      }
    }
  }

  void DecodeImportSection() {
    uint32_t import_table_count =
        consume_count("imports count", kV8MaxWasmImports);
    module_->import_table.reserve(import_table_count);
    for (uint32_t i = 0; ok() && i < import_table_count; ++i) {
      TRACE("DecodeImportTable[%d] module+%d\n", i,
            static_cast<int>(pc_ - start_));
      if (tracer_) tracer_->ImportOffset(pc_offset());

      module_->import_table.push_back({
          {0, 0},             // module_name
          {0, 0},             // field_name
          kExternalFunction,  // kind
          0                   // index
      });
      WasmImport* import = &module_->import_table.back();
      const byte* pos = pc_;
      import->module_name = consume_utf8_string(this, "module name", tracer_);
      import->field_name = consume_utf8_string(this, "field name", tracer_);
      import->kind =
          static_cast<ImportExportKindCode>(consume_u8("kind: ", tracer_));
      if (tracer_) tracer_->Description(ExternalKindName(import->kind));
      switch (import->kind) {
        case kExternalFunction: {
          // ===== Imported function ===========================================
          import->index = static_cast<uint32_t>(module_->functions.size());
          module_->num_imported_functions++;
          module_->functions.push_back({nullptr,        // sig
                                        import->index,  // func_index
                                        0,              // sig_index
                                        {0, 0},         // code
                                        true,           // imported
                                        false,          // exported
                                        false});        // declared
          WasmFunction* function = &module_->functions.back();
          function->sig_index =
              consume_sig_index(module_.get(), &function->sig);
          break;
        }
        case kExternalTable: {
          // ===== Imported table ==============================================
          import->index = static_cast<uint32_t>(module_->tables.size());
          module_->num_imported_tables++;
          module_->tables.emplace_back();
          WasmTable* table = &module_->tables.back();
          table->imported = true;
          const byte* type_position = pc();
          ValueType type = consume_value_type();
          if (!type.is_object_reference()) {
            errorf(type_position, "Invalid table type %s", type.name().c_str());
            break;
          }
          table->type = type;
          consume_table_flags("element count", &table->has_maximum_size);
          consume_resizable_limits(
              "element count", "elements", std::numeric_limits<uint32_t>::max(),
              &table->initial_size, table->has_maximum_size,
              std::numeric_limits<uint32_t>::max(), &table->maximum_size,
              k32BitLimits);
          break;
        }
        case kExternalMemory: {
          // ===== Imported memory =============================================
          if (!AddMemory(module_.get())) break;
          consume_memory_flags(&module_->has_shared_memory,
                               &module_->is_memory64,
                               &module_->has_maximum_pages);
          uint32_t max_pages = module_->is_memory64 ? kSpecMaxMemory64Pages
                                                    : kSpecMaxMemory32Pages;
          consume_resizable_limits(
              "memory", "pages", max_pages, &module_->initial_pages,
              module_->has_maximum_pages, max_pages, &module_->maximum_pages,
              module_->is_memory64 ? k64BitLimits : k32BitLimits);
          break;
        }
        case kExternalGlobal: {
          // ===== Imported global =============================================
          import->index = static_cast<uint32_t>(module_->globals.size());
          module_->num_imported_globals++;
          module_->globals.push_back({kWasmVoid, false, {}, {0}, true, false});
          WasmGlobal* global = &module_->globals.back();
          global->type = consume_value_type();
          global->mutability = consume_mutability();
          if (global->mutability) {
            module_->num_imported_mutable_globals++;
          }
          if (tracer_) tracer_->NextLine();
          break;
        }
        case kExternalTag: {
          // ===== Imported tag ================================================
          import->index = static_cast<uint32_t>(module_->tags.size());
          module_->num_imported_tags++;
          const WasmTagSig* tag_sig = nullptr;
          consume_exception_attribute();  // Attribute ignored for now.
          uint32_t sig_index = consume_tag_sig_index(module_.get(), &tag_sig);
          module_->tags.emplace_back(tag_sig, sig_index);
          break;
        }
        default:
          errorf(pos, "unknown import kind 0x%02x", import->kind);
          break;
      }
    }
    UpdateMemorySizes();
    module_->type_feedback.well_known_imports.Initialize(
        module_->num_imported_functions);
    if (tracer_) tracer_->ImportsDone();
  }

  void DecodeFunctionSection() {
    uint32_t functions_count =
        consume_count("functions count", v8_flags.max_wasm_functions);
    DCHECK_EQ(module_->functions.size(), module_->num_imported_functions);
    uint32_t total_function_count =
        module_->num_imported_functions + functions_count;
    module_->functions.resize(total_function_count);
    module_->num_declared_functions = functions_count;
    // Also initialize the {validated_functions} bitset here, now that we know
    // the number of declared functions.
    DCHECK_NULL(module_->validated_functions);
    module_->validated_functions =
        std::make_unique<std::atomic<uint8_t>[]>((functions_count + 7) / 8);
    if (is_asmjs_module(module_.get())) {
      // Mark all asm.js functions as valid by design (it's faster to do this
      // here than to check this in {WasmModule::function_was_validated}).
      std::fill_n(module_->validated_functions.get(), (functions_count + 7) / 8,
                  0xff);
    }

    for (uint32_t func_index = module_->num_imported_functions;
         func_index < total_function_count; ++func_index) {
      WasmFunction* function = &module_->functions[func_index];
      function->func_index = func_index;
      if (tracer_) tracer_->FunctionName(func_index);
      function->sig_index = consume_sig_index(module_.get(), &function->sig);
      if (!ok()) return;
    }
  }

  void DecodeTableSection() {
    uint32_t table_count = consume_count("table count", kV8MaxWasmTables);

    for (uint32_t i = 0; ok() && i < table_count; i++) {
      if (tracer_) tracer_->TableOffset(pc_offset());
      module_->tables.emplace_back();
      WasmTable* table = &module_->tables.back();
      const byte* type_position = pc();

      bool has_initializer = false;
      if (enabled_features_.has_typed_funcref() &&
          read_u8<Decoder::FullValidationTag>(
              pc(), "table-with-initializer byte") == 0x40) {
        consume_bytes(1, "table-with-initializer byte", tracer_);
        has_initializer = true;
        type_position++;
        uint8_t reserved = consume_u8("reserved byte", tracer_);
        if (reserved != 0) {
          error(type_position, "Reserved byte must be 0x00");
          break;
        }
        type_position++;
      }

      ValueType table_type = consume_value_type();
      if (!table_type.is_object_reference()) {
        error(type_position, "Only reference types can be used as table types");
        break;
      }
      if (!has_initializer && !table_type.is_defaultable()) {
        errorf(type_position,
               "Table of non-defaultable table %s needs initial value",
               table_type.name().c_str());
        break;
      }
      table->type = table_type;

      consume_table_flags("table elements", &table->has_maximum_size);
      consume_resizable_limits("table elements", "elements",
                               std::numeric_limits<uint32_t>::max(),
                               &table->initial_size, table->has_maximum_size,
                               std::numeric_limits<uint32_t>::max(),
                               &table->maximum_size, k32BitLimits);

      if (has_initializer) {
        table->initial_value = consume_init_expr(module_.get(), table_type);
      }
    }
  }

  void DecodeMemorySection() {
    uint32_t memory_count = consume_count("memory count", kV8MaxWasmMemories);

    for (uint32_t i = 0; ok() && i < memory_count; i++) {
      if (tracer_) tracer_->MemoryOffset(pc_offset());
      if (!AddMemory(module_.get())) break;
      consume_memory_flags(&module_->has_shared_memory, &module_->is_memory64,
                           &module_->has_maximum_pages);
      uint32_t max_pages =
          module_->is_memory64 ? kSpecMaxMemory64Pages : kSpecMaxMemory32Pages;
      consume_resizable_limits(
          "memory", "pages", max_pages, &module_->initial_pages,
          module_->has_maximum_pages, max_pages, &module_->maximum_pages,
          module_->is_memory64 ? k64BitLimits : k32BitLimits);
    }
    UpdateMemorySizes();
  }

  void UpdateMemorySizes() {
    // Set min and max memory size.
    const uintptr_t platform_max_pages = module_->is_memory64
                                             ? kV8MaxWasmMemory64Pages
                                             : kV8MaxWasmMemory32Pages;
    module_->min_memory_size =
        std::min(platform_max_pages, uintptr_t{module_->initial_pages}) *
        kWasmPageSize;
    module_->max_memory_size =
        std::min(platform_max_pages, uintptr_t{module_->maximum_pages}) *
        kWasmPageSize;
  }

  void DecodeGlobalSection() {
    uint32_t globals_count = consume_count("globals count", kV8MaxWasmGlobals);
    uint32_t imported_globals = static_cast<uint32_t>(module_->globals.size());
    // It is important to not resize the globals vector from the beginning,
    // because we use its current size when decoding the initializer.
    module_->globals.reserve(imported_globals + globals_count);
    for (uint32_t i = 0; ok() && i < globals_count; ++i) {
      TRACE("DecodeGlobal[%d] module+%d\n", i, static_cast<int>(pc_ - start_));
      if (tracer_) tracer_->GlobalOffset(pc_offset());
      ValueType type = consume_value_type();
      bool mutability = consume_mutability();
      if (failed()) break;
      ConstantExpression init = consume_init_expr(module_.get(), type);
      module_->globals.push_back({type, mutability, init, {0}, false, false});
    }
  }

  void DecodeExportSection() {
    uint32_t export_table_count =
        consume_count("exports count", kV8MaxWasmExports);
    module_->export_table.reserve(export_table_count);
    for (uint32_t i = 0; ok() && i < export_table_count; ++i) {
      TRACE("DecodeExportTable[%d] module+%d\n", i,
            static_cast<int>(pc_ - start_));
      if (tracer_) {
        tracer_->Description("export #");
        tracer_->Description(i);
        tracer_->NextLine();
      }

      module_->export_table.push_back({
          {0, 0},             // name
          kExternalFunction,  // kind
          0                   // index
      });
      WasmExport* exp = &module_->export_table.back();

      exp->name = consume_utf8_string(this, "field name", tracer_);

      const byte* pos = pc();
      exp->kind =
          static_cast<ImportExportKindCode>(consume_u8("kind: ", tracer_));
      if (tracer_) {
        tracer_->Description(ExternalKindName(exp->kind));
        tracer_->Description(" ");
      }
      switch (exp->kind) {
        case kExternalFunction: {
          WasmFunction* func = nullptr;
          exp->index = consume_func_index(module_.get(), &func);

          if (failed()) break;
          DCHECK_NOT_NULL(func);

          module_->num_exported_functions++;
          func->exported = true;
          // Exported functions are considered "declared".
          func->declared = true;
          break;
        }
        case kExternalTable: {
          WasmTable* table = nullptr;
          exp->index = consume_table_index(module_.get(), &table);
          if (table) table->exported = true;
          break;
        }
        case kExternalMemory: {
          uint32_t index = consume_u32v("memory index", tracer_);
          // TODO(titzer): This should become more regular
          // once we support multiple memories.
          if (!module_->has_memory || index != 0) {
            error("invalid memory index != 0");
          }
          module_->mem_export = true;
          break;
        }
        case kExternalGlobal: {
          WasmGlobal* global = nullptr;
          exp->index = consume_global_index(module_.get(), &global);
          if (global) {
            global->exported = true;
          }
          break;
        }
        case kExternalTag: {
          WasmTag* tag = nullptr;
          exp->index = consume_tag_index(module_.get(), &tag);
          break;
        }
        default:
          errorf(pos, "invalid export kind 0x%02x", exp->kind);
          break;
      }
      if (tracer_) tracer_->NextLine();
    }
    // Check for duplicate exports (except for asm.js).
    if (ok() && module_->origin == kWasmOrigin &&
        module_->export_table.size() > 1) {
      std::vector<WasmExport> sorted_exports(module_->export_table);

      auto cmp_less = [this](const WasmExport& a, const WasmExport& b) {
        // Return true if a < b.
        if (a.name.length() != b.name.length()) {
          return a.name.length() < b.name.length();
        }
        const byte* left = start() + GetBufferRelativeOffset(a.name.offset());
        const byte* right = start() + GetBufferRelativeOffset(b.name.offset());
        return memcmp(left, right, a.name.length()) < 0;
      };
      std::stable_sort(sorted_exports.begin(), sorted_exports.end(), cmp_less);

      auto it = sorted_exports.begin();
      WasmExport* last = &*it++;
      for (auto end = sorted_exports.end(); it != end; last = &*it++) {
        DCHECK(!cmp_less(*it, *last));  // Vector must be sorted.
        if (!cmp_less(*last, *it)) {
          const byte* pc = start() + GetBufferRelativeOffset(it->name.offset());
          TruncatedUserString<> name(pc, it->name.length());
          errorf(pc, "Duplicate export name '%.*s' for %s %d and %s %d",
                 name.length(), name.start(), ExternalKindName(last->kind),
                 last->index, ExternalKindName(it->kind), it->index);
          break;
        }
      }
    }
  }

  void DecodeStartSection() {
    if (tracer_) tracer_->StartOffset(pc_offset());
    WasmFunction* func;
    const byte* pos = pc_;
    module_->start_function_index = consume_func_index(module_.get(), &func);
    if (tracer_) tracer_->NextLine();
    if (func &&
        (func->sig->parameter_count() > 0 || func->sig->return_count() > 0)) {
      error(pos, "invalid start function: non-zero parameter or return count");
    }
  }

  void DecodeElementSection() {
    uint32_t segment_count =
        consume_count("segment count", v8_flags.wasm_max_table_size);

    for (uint32_t i = 0; i < segment_count; ++i) {
      if (tracer_) tracer_->ElementOffset(pc_offset());
      WasmElemSegment segment = consume_element_segment_header();
      if (tracer_) tracer_->NextLineIfNonEmpty();
      if (failed()) return;
      DCHECK_NE(segment.type, kWasmBottom);

      for (uint32_t j = 0; j < segment.element_count; j++) {
        // Just run validation on elements; do not store them anywhere. We will
        // decode them again from wire bytes as needed.
        consume_element_segment_entry(module_.get(), segment);
        if (failed()) return;
      }
      module_->elem_segments.push_back(std::move(segment));
    }
  }

  void DecodeCodeSection() {
    // Make sure global offset were calculated before they get accessed during
    // function compilation.
    CalculateGlobalOffsets(module_.get());
    uint32_t code_section_start = pc_offset();
    uint32_t functions_count = consume_u32v("functions count", tracer_);
    if (tracer_) {
      tracer_->Description(functions_count);
      tracer_->NextLine();
    }
    CheckFunctionsCount(functions_count, code_section_start);

    auto inst_traces_it = this->inst_traces_.begin();
    std::vector<std::pair<uint32_t, uint32_t>> inst_traces;

    for (uint32_t i = 0; ok() && i < functions_count; ++i) {
      int function_index = module_->num_imported_functions + i;
      if (tracer_) {
        tracer_->Description("function #");
        tracer_->FunctionName(function_index);
        tracer_->NextLine();
      }
      const byte* pos = pc();
      uint32_t size = consume_u32v("body size", tracer_);
      if (tracer_) {
        tracer_->Description(size);
        tracer_->NextLine();
      }
      if (size > kV8MaxWasmFunctionSize) {
        errorf(pos, "size %u > maximum function size %zu", size,
               kV8MaxWasmFunctionSize);
        return;
      }
      uint32_t offset = pc_offset();
      consume_bytes(size, "function body");
      if (failed()) break;
      DecodeFunctionBody(function_index, size, offset);

      // Now that the function has been decoded, we can compute module offsets.
      for (; inst_traces_it != this->inst_traces_.end() &&
             std::get<0>(*inst_traces_it) == i;
           ++inst_traces_it) {
        uint32_t trace_offset = offset + std::get<1>(*inst_traces_it);
        uint32_t mark_id = std::get<2>(*inst_traces_it);
        std::pair<uint32_t, uint32_t> trace_mark = {trace_offset, mark_id};
        inst_traces.push_back(trace_mark);
      }
    }
    // If we have actually decoded traces and they were all decoded without
    // error, then we can move them to the module. If any errors are found, it
    // is safe to throw away all traces.
    if (V8_UNLIKELY(!inst_traces.empty() &&
                    inst_traces_it == this->inst_traces_.end())) {
      // This adds an invalid entry at the end of the traces. An invalid entry
      // is defined as having an module offset of 0 and a markid of 0.
      inst_traces.push_back({0, 0});
      this->module_->inst_traces = std::move(inst_traces);
    }
    DCHECK_GE(pc_offset(), code_section_start);
    module_->code = {code_section_start, pc_offset() - code_section_start};
  }

  void StartCodeSection(WireBytesRef section_bytes) {
    CheckSectionOrder(kCodeSectionCode);
    // Make sure global offset were calculated before they get accessed during
    // function compilation.
    CalculateGlobalOffsets(module_.get());
    module_->code = section_bytes;
  }

  bool CheckFunctionsCount(uint32_t functions_count, uint32_t error_offset) {
    if (functions_count != module_->num_declared_functions) {
      errorf(error_offset, "function body count %u mismatch (%u expected)",
             functions_count, module_->num_declared_functions);
      return false;
    }
    return true;
  }

  void DecodeFunctionBody(uint32_t func_index, uint32_t length,
                          uint32_t offset) {
    WasmFunction* function = &module_->functions[func_index];
    function->code = {offset, length};
    if (tracer_) {
      tracer_->FunctionBody(function, pc_ - (pc_offset() - offset));
    }
  }

  bool CheckDataSegmentsCount(uint32_t data_segments_count) {
    if (has_seen_unordered_section(kDataCountSectionCode) &&
        data_segments_count != module_->num_declared_data_segments) {
      errorf(pc(), "data segments count %u mismatch (%u expected)",
             data_segments_count, module_->num_declared_data_segments);
      return false;
    }
    return true;
  }

  void DecodeDataSection() {
    uint32_t data_segments_count =
        consume_count("data segments count", kV8MaxWasmDataSegments);
    if (!CheckDataSegmentsCount(data_segments_count)) return;

    module_->data_segments.reserve(data_segments_count);
    for (uint32_t i = 0; ok() && i < data_segments_count; ++i) {
      const byte* pos = pc();
      TRACE("DecodeDataSegment[%d] module+%d\n", i,
            static_cast<int>(pc_ - start_));
      if (tracer_) tracer_->DataOffset(pc_offset());

      bool is_active;
      uint32_t memory_index;
      ConstantExpression dest_addr;
      consume_data_segment_header(&is_active, &memory_index, &dest_addr);
      if (failed()) break;

      if (is_active) {
        if (!module_->has_memory) {
          error("cannot load data without memory");
          break;
        }
        if (memory_index != 0) {
          errorf(pos, "illegal memory index %u != 0", memory_index);
          break;
        }
      }

      uint32_t source_length = consume_u32v("source size", tracer_);
      if (tracer_) {
        tracer_->Description(source_length);
        tracer_->NextLine();
      }
      uint32_t source_offset = pc_offset();

      if (is_active) {
        module_->data_segments.emplace_back(std::move(dest_addr));
      } else {
        module_->data_segments.emplace_back();
      }

      WasmDataSegment* segment = &module_->data_segments.back();

      if (tracer_) {
        tracer_->Bytes(pc_, source_length);
        tracer_->Description("segment data");
        tracer_->NextLine();
      }
      consume_bytes(source_length, "segment data");
      if (failed()) break;

      segment->source = {source_offset, source_length};
    }
  }

  void DecodeNameSection() {
    if (tracer_) {
      tracer_->NameSection(
          pc_, end_, buffer_offset_ + static_cast<uint32_t>(pc_ - start_));
    }
    // TODO(titzer): find a way to report name errors as warnings.
    // Ignore all but the first occurrence of name section.
    if (!has_seen_unordered_section(kNameSectionCode)) {
      set_seen_unordered_section(kNameSectionCode);
      module_->name_section = {buffer_offset_,
                               static_cast<uint32_t>(end_ - start_)};
      // Use an inner decoder so that errors don't fail the outer decoder.
      Decoder inner(start_, pc_, end_, buffer_offset_);
      // Decode all name subsections.
      // Be lenient with their order.
      while (inner.ok() && inner.more()) {
        uint8_t name_type = inner.consume_u8("name type");
        if (name_type & 0x80) inner.error("name type if not varuint7");

        uint32_t name_payload_len = inner.consume_u32v("name payload length");
        if (!inner.checkAvailable(name_payload_len)) break;

        // Decode module name, ignore the rest.
        // Function and local names will be decoded when needed.
        if (name_type == NameSectionKindCode::kModuleCode) {
          WireBytesRef name =
              consume_string(&inner, unibrow::Utf8Variant::kLossyUtf8,
                             "module name", ITracer::NoTrace);
          if (inner.ok() && validate_utf8(&inner, name)) {
            module_->name = name;
          }
        } else {
          inner.consume_bytes(name_payload_len, "name subsection payload");
        }
      }
    }
    // Skip the whole names section in the outer decoder.
    consume_bytes(static_cast<uint32_t>(end_ - start_), nullptr);
  }

  void DecodeSourceMappingURLSection() {
    Decoder inner(start_, pc_, end_, buffer_offset_);
    WireBytesRef url =
        wasm::consume_utf8_string(&inner, "module name", tracer_);
    if (inner.ok() &&
        module_->debug_symbols.type != WasmDebugSymbols::Type::SourceMap) {
      module_->debug_symbols = {WasmDebugSymbols::Type::SourceMap, url};
    }
    set_seen_unordered_section(kSourceMappingURLSectionCode);
    consume_bytes(static_cast<uint32_t>(end_ - start_), nullptr);
  }

  void DecodeExternalDebugInfoSection() {
    Decoder inner(start_, pc_, end_, buffer_offset_);
    WireBytesRef url =
        wasm::consume_utf8_string(&inner, "external symbol file", tracer_);
    // If there is an explicit source map, prefer it over DWARF info.
    if (inner.ok() &&
        module_->debug_symbols.type != WasmDebugSymbols::Type::SourceMap) {
      module_->debug_symbols = {WasmDebugSymbols::Type::ExternalDWARF, url};
      set_seen_unordered_section(kExternalDebugInfoSectionCode);
    }
    consume_bytes(static_cast<uint32_t>(end_ - start_), nullptr);
  }

  void DecodeInstTraceSection() {
    TRACE("DecodeInstTrace module+%d\n", static_cast<int>(pc_ - start_));
    if (!has_seen_unordered_section(kInstTraceSectionCode)) {
      set_seen_unordered_section(kInstTraceSectionCode);

      // Use an inner decoder so that errors don't fail the outer decoder.
      Decoder inner(start_, pc_, end_, buffer_offset_);

      std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> inst_traces;

      uint32_t func_count = inner.consume_u32v("number of functions");
      // Keep track of the previous function index to validate the ordering.
      int64_t last_func_idx = -1;
      for (uint32_t i = 0; i < func_count; i++) {
        uint32_t func_idx = inner.consume_u32v("function index");
        if (int64_t{func_idx} <= last_func_idx) {
          inner.errorf("Invalid function index: %d", func_idx);
          break;
        }
        last_func_idx = func_idx;

        uint32_t num_traces = inner.consume_u32v("number of trace marks");
        TRACE("DecodeInstTrace[%d] module+%d\n", func_idx,
              static_cast<int>(inner.pc() - inner.start()));
        // Keep track of the previous offset to validate the ordering.
        int64_t last_func_off = -1;
        for (uint32_t j = 0; j < num_traces; ++j) {
          uint32_t func_off = inner.consume_u32v("function offset");

          uint32_t mark_size = inner.consume_u32v("mark size");
          uint32_t trace_mark_id = 0;
          // Build the mark id from the individual bytes.
          for (uint32_t k = 0; k < mark_size; k++) {
            trace_mark_id |= inner.consume_u8("trace mark id") << k * 8;
          }
          if (int64_t{func_off} <= last_func_off) {
            inner.errorf("Invalid branch offset: %d", func_off);
            break;
          }
          last_func_off = func_off;
          TRACE("DecodeInstTrace[%d][%d] module+%d\n", func_idx, func_off,
                static_cast<int>(inner.pc() - inner.start()));
          // Store the function index, function offset, and mark id into a
          // temporary 3-tuple. This will later be translated to a module
          // offset and  mark id.
          std::tuple<uint32_t, uint32_t, uint32_t> mark_tuple = {
              func_idx, func_off, trace_mark_id};
          inst_traces.push_back(mark_tuple);
        }
      }
      // Extra unexpected bytes are an error.
      if (inner.more()) {
        inner.errorf("Unexpected extra bytes: %d\n",
                     static_cast<int>(inner.pc() - inner.start()));
      }
      // If everything went well, accept the traces for the module.
      if (inner.ok()) {
        this->inst_traces_ = std::move(inst_traces);
      }
    }

    // Skip the whole instruction trace section in the outer decoder.
    consume_bytes(static_cast<uint32_t>(end_ - start_), nullptr);
  }

  void DecodeCompilationHintsSection() {
    TRACE("DecodeCompilationHints module+%d\n", static_cast<int>(pc_ - start_));

    // TODO(frgossen): Find a way to report compilation hint errors as warnings.
    // All except first occurrence after function section and before code
    // section are ignored.
    const bool before_function_section =
        next_ordered_section_ <= kFunctionSectionCode;
    const bool after_code_section = next_ordered_section_ > kCodeSectionCode;
    if (before_function_section || after_code_section ||
        has_seen_unordered_section(kCompilationHintsSectionCode)) {
      return;
    }
    set_seen_unordered_section(kCompilationHintsSectionCode);

    // TODO(frgossen) Propagate errors to outer decoder in experimental phase.
    // We should use an inner decoder later and propagate its errors as
    // warnings.
    Decoder& decoder = *this;
    // Decoder decoder(start_, pc_, end_, buffer_offset_);

    // Ensure exactly one compilation hint per function.
    uint32_t hint_count = decoder.consume_u32v("compilation hint count");
    if (hint_count != module_->num_declared_functions) {
      decoder.errorf(decoder.pc(), "Expected %u compilation hints (%u found)",
                     module_->num_declared_functions, hint_count);
    }

    // Decode sequence of compilation hints.
    if (decoder.ok()) {
      module_->compilation_hints.reserve(hint_count);
    }
    for (uint32_t i = 0; decoder.ok() && i < hint_count; i++) {
      TRACE("DecodeCompilationHints[%d] module+%d\n", i,
            static_cast<int>(pc_ - start_));

      // Compilation hints are encoded in one byte each.
      // +-------+----------+---------------+----------+
      // | 2 bit | 2 bit    | 2 bit         | 2 bit    |
      // | ...   | Top tier | Baseline tier | Strategy |
      // +-------+----------+---------------+----------+
      uint8_t hint_byte = decoder.consume_u8("compilation hint");
      if (!decoder.ok()) break;

      // Validate the hint_byte.
      // For the compilation strategy, all 2-bit values are valid. For the tier,
      // only 0x0, 0x1, and 0x2 are allowed.
      static_assert(
          static_cast<int>(WasmCompilationHintTier::kDefault) == 0 &&
              static_cast<int>(WasmCompilationHintTier::kBaseline) == 1 &&
              static_cast<int>(WasmCompilationHintTier::kOptimized) == 2,
          "The check below assumes that 0x03 is the only invalid 2-bit number "
          "for a compilation tier");
      if (((hint_byte >> 2) & 0x03) == 0x03 ||
          ((hint_byte >> 4) & 0x03) == 0x03) {
        decoder.errorf(decoder.pc(),
                       "Invalid compilation hint %#04x (invalid tier 0x03)",
                       hint_byte);
        break;
      }

      // Decode compilation hint.
      WasmCompilationHint hint;
      hint.strategy =
          static_cast<WasmCompilationHintStrategy>(hint_byte & 0x03);
      hint.baseline_tier =
          static_cast<WasmCompilationHintTier>((hint_byte >> 2) & 0x03);
      hint.top_tier =
          static_cast<WasmCompilationHintTier>((hint_byte >> 4) & 0x03);

      // Ensure that the top tier never downgrades a compilation result. If
      // baseline and top tier are the same compilation will be invoked only
      // once.
      if (hint.top_tier < hint.baseline_tier &&
          hint.top_tier != WasmCompilationHintTier::kDefault) {
        decoder.errorf(decoder.pc(),
                       "Invalid compilation hint %#04x (forbidden downgrade)",
                       hint_byte);
      }

      // Happily accept compilation hint.
      if (decoder.ok()) {
        module_->compilation_hints.push_back(std::move(hint));
      }
    }

    // If section was invalid reset compilation hints.
    if (decoder.failed()) {
      module_->compilation_hints.clear();
    }

    // @TODO(frgossen) Skip the whole compilation hints section in the outer
    // decoder if inner decoder was used.
    // consume_bytes(static_cast<uint32_t>(end_ - start_), nullptr);
  }

  void DecodeBranchHintsSection() {
    TRACE("DecodeBranchHints module+%d\n", static_cast<int>(pc_ - start_));
    if (!has_seen_unordered_section(kBranchHintsSectionCode)) {
      set_seen_unordered_section(kBranchHintsSectionCode);
      // Use an inner decoder so that errors don't fail the outer decoder.
      Decoder inner(start_, pc_, end_, buffer_offset_);
      BranchHintInfo branch_hints;

      uint32_t func_count = inner.consume_u32v("number of functions");
      // Keep track of the previous function index to validate the ordering
      int64_t last_func_idx = -1;
      for (uint32_t i = 0; i < func_count; i++) {
        uint32_t func_idx = inner.consume_u32v("function index");
        if (int64_t{func_idx} <= last_func_idx) {
          inner.errorf("Invalid function index: %d", func_idx);
          break;
        }
        last_func_idx = func_idx;
        uint32_t num_hints = inner.consume_u32v("number of hints");
        BranchHintMap func_branch_hints;
        TRACE("DecodeBranchHints[%d] module+%d\n", func_idx,
              static_cast<int>(inner.pc() - inner.start()));
        // Keep track of the previous branch offset to validate the ordering
        int64_t last_br_off = -1;
        for (uint32_t j = 0; j < num_hints; ++j) {
          uint32_t br_off = inner.consume_u32v("branch instruction offset");
          if (int64_t{br_off} <= last_br_off) {
            inner.errorf("Invalid branch offset: %d", br_off);
            break;
          }
          last_br_off = br_off;
          uint32_t data_size = inner.consume_u32v("data size");
          if (data_size != 1) {
            inner.errorf("Invalid data size: %#x. Expected 1.", data_size);
            break;
          }
          uint32_t br_dir = inner.consume_u8("branch direction");
          TRACE("DecodeBranchHints[%d][%d] module+%d\n", func_idx, br_off,
                static_cast<int>(inner.pc() - inner.start()));
          WasmBranchHint hint;
          switch (br_dir) {
            case 0:
              hint = WasmBranchHint::kUnlikely;
              break;
            case 1:
              hint = WasmBranchHint::kLikely;
              break;
            default:
              hint = WasmBranchHint::kNoHint;
              inner.errorf(inner.pc(), "Invalid branch hint %#x", br_dir);
              break;
          }
          if (!inner.ok()) {
            break;
          }
          func_branch_hints.insert(br_off, hint);
        }
        if (!inner.ok()) {
          break;
        }
        branch_hints.emplace(func_idx, std::move(func_branch_hints));
      }
      // Extra unexpected bytes are an error.
      if (inner.more()) {
        inner.errorf("Unexpected extra bytes: %d\n",
                     static_cast<int>(inner.pc() - inner.start()));
      }
      // If everything went well, accept the hints for the module.
      if (inner.ok()) {
        module_->branch_hints = std::move(branch_hints);
      }
    }
    // Skip the whole branch hints section in the outer decoder.
    consume_bytes(static_cast<uint32_t>(end_ - start_), nullptr);
  }

  void DecodeDataCountSection() {
    module_->num_declared_data_segments =
        consume_count("data segments count", kV8MaxWasmDataSegments);
    if (tracer_) tracer_->NextLineIfNonEmpty();
  }

  void DecodeTagSection() {
    uint32_t tag_count = consume_count("tag count", kV8MaxWasmTags);
    for (uint32_t i = 0; ok() && i < tag_count; ++i) {
      TRACE("DecodeTag[%d] module+%d\n", i, static_cast<int>(pc_ - start_));
      if (tracer_) tracer_->TagOffset(pc_offset());
      const WasmTagSig* tag_sig = nullptr;
      consume_exception_attribute();  // Attribute ignored for now.
      uint32_t sig_index = consume_tag_sig_index(module_.get(), &tag_sig);
      module_->tags.emplace_back(tag_sig, sig_index);
    }
  }

  void DecodeStringRefSection() {
    uint32_t deferred = consume_count("deferred string literal count",
                                      kV8MaxWasmStringLiterals);
    if (deferred) {
      errorf(pc(), "Invalid deferred string literal count %u (expected 0)",
             deferred);
    }
    uint32_t immediate = consume_count("string literal count",
                                       kV8MaxWasmStringLiterals - deferred);
    for (uint32_t i = 0; ok() && i < immediate; ++i) {
      TRACE("DecodeStringLiteral[%d] module+%d\n", i,
            static_cast<int>(pc_ - start_));
      if (tracer_) tracer_->StringOffset(pc_offset());
      // TODO(12868): Throw if the string's utf-16 length > String::kMaxLength.
      WireBytesRef pos = wasm::consume_string(this, unibrow::Utf8Variant::kWtf8,
                                              "string literal", tracer_);
      module_->stringref_literals.emplace_back(pos);
    }
  }

  bool CheckMismatchedCounts() {
    // The declared vs. defined function count is normally checked when
    // decoding the code section, but we have to check it here too in case the
    // code section is absent.
    if (module_->num_declared_functions != 0) {
      DCHECK_LT(module_->num_imported_functions, module_->functions.size());
      // We know that the code section has been decoded if the first
      // non-imported function has its code set.
      if (!module_->functions[module_->num_imported_functions].code.is_set()) {
        errorf(pc(), "function count is %u, but code section is absent",
               module_->num_declared_functions);
        return false;
      }
    }
    // Perform a similar check for the DataCount and Data sections, where data
    // segments are declared but the Data section is absent.
    if (!CheckDataSegmentsCount(
            static_cast<uint32_t>(module_->data_segments.size()))) {
      return false;
    }
    return true;
  }

  ModuleResult FinishDecoding() {
    if (ok() && CheckMismatchedCounts()) {
      // We calculate the global offsets here, because there may not be a
      // global section and code section that would have triggered the
      // calculation before. Even without the globals section the calculation
      // is needed because globals can also be defined in the import section.
      CalculateGlobalOffsets(module_.get());
    }

    return toResult(std::move(module_));
  }

  // Decodes an entire module.
  ModuleResult DecodeModule(bool validate_functions) {
    // Keep a reference to the wire bytes, in case this decoder gets reset on
    // error.
    base::Vector<const byte> wire_bytes(start_, end_ - start_);
    size_t max_size = max_module_size();
    if (wire_bytes.size() > max_size) {
      return ModuleResult{WasmError{0, "size > maximum module size (%zu): %zu",
                                    max_size, wire_bytes.size()}};
    }

    DecodeModuleHeader(wire_bytes);
    if (failed()) return toResult(nullptr);

    static constexpr uint32_t kWasmHeaderSize = 8;
    Decoder section_iterator_decoder(start_ + kWasmHeaderSize, end_,
                                     kWasmHeaderSize);
    WasmSectionIterator section_iter(&section_iterator_decoder, tracer_);

    while (ok()) {
      if (section_iter.section_code() != SectionCode::kUnknownSectionCode) {
        uint32_t offset = static_cast<uint32_t>(section_iter.payload().begin() -
                                                wire_bytes.begin());
        DecodeSection(section_iter.section_code(), section_iter.payload(),
                      offset);
        if (!ok()) break;
      }
      if (!section_iter.more()) break;
      section_iter.advance(true);
    }

    // Check for module structure errors before validating function bodies, to
    // produce consistent error message independent of whether validation
    // happens here or later.
    if (section_iterator_decoder.failed()) {
      return section_iterator_decoder.toResult(nullptr);
    }

    ModuleResult result = FinishDecoding();
    if (!result.failed() && validate_functions) {
      // Pass nullptr for an "empty" filter function.
      if (WasmError validation_error = ValidateFunctions(
              module_.get(), enabled_features_, wire_bytes, nullptr)) {
        result = ModuleResult{validation_error};
      }
    }

    if (v8_flags.dump_wasm_module) DumpModule(wire_bytes, result.ok());

    return result;
  }

  // Decodes a single anonymous function starting at {start_}.
  FunctionResult DecodeSingleFunctionForTesting(Zone* zone,
                                                ModuleWireBytes wire_bytes,
                                                const WasmModule* module) {
    DCHECK(ok());
    pc_ = start_;
    expect_u8("type form", kWasmFunctionTypeCode);
    WasmFunction function;
    function.sig = consume_sig(zone);
    function.code = {off(pc_), static_cast<uint32_t>(end_ - pc_)};

    if (!ok()) return FunctionResult{std::move(error_)};

    FunctionBody body{function.sig, off(pc_), pc_, end_};

    WasmFeatures unused_detected_features;
    DecodeResult result = ValidateFunctionBody(enabled_features_, module,
                                               &unused_detected_features, body);

    if (result.failed()) return FunctionResult{std::move(result).error()};

    return FunctionResult{std::make_unique<WasmFunction>(function)};
  }

  // Decodes a single function signature at {start}.
  const FunctionSig* DecodeFunctionSignature(Zone* zone, const byte* start) {
    pc_ = start;
    if (!expect_u8("type form", kWasmFunctionTypeCode)) return nullptr;
    const FunctionSig* result = consume_sig(zone);
    return ok() ? result : nullptr;
  }

  ConstantExpression DecodeInitExprForTesting(ValueType expected) {
    return consume_init_expr(module_.get(), expected);
  }

  // Takes a module as parameter so that wasm-disassembler.cc can pass its own
  // module.
  ConstantExpression consume_element_segment_entry(
      WasmModule* module, const WasmElemSegment& segment) {
    if (segment.element_type == WasmElemSegment::kExpressionElements) {
      return consume_init_expr(module, segment.type);
    } else {
      return ConstantExpression::RefFunc(
          consume_element_func_index(module, segment.type));
    }
  }

  const std::shared_ptr<WasmModule>& shared_module() const { return module_; }

 private:
  bool has_seen_unordered_section(SectionCode section_code) {
    return seen_unordered_sections_ & (1 << section_code);
  }

  void set_seen_unordered_section(SectionCode section_code) {
    seen_unordered_sections_ |= 1 << section_code;
  }

  uint32_t off(const byte* ptr) {
    return static_cast<uint32_t>(ptr - start_) + buffer_offset_;
  }

  bool AddMemory(WasmModule* module) {
    if (module->has_memory) {
      error("At most one memory is supported");
      return false;
    } else {
      module->has_memory = true;
      return true;
    }
  }

  // Calculate individual global offsets and total size of globals table. This
  // function should be called after all globals have been defined, which is
  // after the import section and the global section, but before the global
  // offsets are accessed, e.g. by the function compilers. The moment when this
  // function should be called is not well-defined, as the global section may
  // not exist. Therefore this function is called multiple times.
  void CalculateGlobalOffsets(WasmModule* module) {
    if (module->globals.empty() || module->untagged_globals_buffer_size != 0 ||
        module->tagged_globals_buffer_size != 0) {
      // This function has already been executed before, so we don't have to
      // execute it again.
      return;
    }
    uint32_t untagged_offset = 0;
    uint32_t tagged_offset = 0;
    uint32_t num_imported_mutable_globals = 0;
    for (WasmGlobal& global : module->globals) {
      if (global.mutability && global.imported) {
        global.index = num_imported_mutable_globals++;
      } else if (global.type.is_reference()) {
        global.offset = tagged_offset;
        // All entries in the tagged_globals_buffer have size 1.
        tagged_offset++;
      } else {
        int size = global.type.value_kind_size();
        untagged_offset = (untagged_offset + size - 1) & ~(size - 1);  // align
        global.offset = untagged_offset;
        untagged_offset += size;
      }
    }
    module->untagged_globals_buffer_size = untagged_offset;
    module->tagged_globals_buffer_size = tagged_offset;
  }

  uint32_t consume_sig_index(WasmModule* module, const FunctionSig** sig) {
    const byte* pos = pc_;
    uint32_t sig_index = consume_u32v("signature index");
    if (tracer_) tracer_->Bytes(pos, static_cast<uint32_t>(pc_ - pos));
    if (!module->has_signature(sig_index)) {
      errorf(pos, "no signature at index %u (%d %s)", sig_index,
             static_cast<int>(module->types.size()),
             enabled_features_.has_gc() ? "types" : "signatures");
      *sig = nullptr;
      return 0;
    }
    *sig = module->signature(sig_index);
    if (tracer_) {
      tracer_->Description(*sig);
      tracer_->NextLine();
    }
    return sig_index;
  }

  uint32_t consume_tag_sig_index(WasmModule* module, const FunctionSig** sig) {
    const byte* pos = pc_;
    uint32_t sig_index = consume_sig_index(module, sig);
    if (*sig && (*sig)->return_count() != 0) {
      errorf(pos, "tag signature %u has non-void return", sig_index);
      *sig = nullptr;
      return 0;
    }
    return sig_index;
  }

  uint32_t consume_count(const char* name, size_t maximum) {
    const byte* p = pc_;
    uint32_t count = consume_u32v(name, tracer_);
    if (tracer_) {
      tracer_->Description(count);
      if (count == 1) {
        tracer_->Description(": ");
      } else {
        tracer_->NextLine();
      }
    }
    if (count > maximum) {
      errorf(p, "%s of %u exceeds internal limit of %zu", name, count, maximum);
      return static_cast<uint32_t>(maximum);
    }
    return count;
  }

  uint32_t consume_func_index(WasmModule* module, WasmFunction** func) {
    return consume_index("function", &module->functions, func);
  }

  uint32_t consume_global_index(WasmModule* module, WasmGlobal** global) {
    return consume_index("global", &module->globals, global);
  }

  uint32_t consume_table_index(WasmModule* module, WasmTable** table) {
    return consume_index("table", &module->tables, table);
  }

  uint32_t consume_tag_index(WasmModule* module, WasmTag** tag) {
    return consume_index("tag", &module->tags, tag);
  }

  template <typename T>
  uint32_t consume_index(const char* name, std::vector<T>* vector, T** ptr) {
    const byte* pos = pc_;
    uint32_t index = consume_u32v("index:", tracer_);
    if (tracer_) tracer_->Description(index);
    if (index >= vector->size()) {
      errorf(pos, "%s index %u out of bounds (%d entr%s)", name, index,
             static_cast<int>(vector->size()),
             vector->size() == 1 ? "y" : "ies");
      *ptr = nullptr;
      return 0;
    }
    *ptr = &(*vector)[index];
    return index;
  }

  void consume_table_flags(const char* name, bool* has_maximum_out) {
    if (tracer_) tracer_->Bytes(pc_, 1);
    uint8_t flags = consume_u8("table limits flags");
    if (tracer_) {
      tracer_->Description(flags == kNoMaximum ? " no maximum"
                                               : " with maximum");
      tracer_->NextLine();
    }

    static_assert(kNoMaximum == 0 && kWithMaximum == 1);
    *has_maximum_out = flags == kWithMaximum;
    if (V8_UNLIKELY(flags > kWithMaximum)) {
      errorf(pc() - 1, "invalid %s limits flags", name);
    }
  }

  void consume_memory_flags(bool* is_shared_out, bool* is_memory64_out,
                            bool* has_maximum_out) {
    if (tracer_) tracer_->Bytes(pc_, 1);
    uint8_t flags = consume_u8("memory limits flags");
    // Flags 0..7 are valid (3 bits).
    if (flags & ~0x7) {
      errorf(pc() - 1, "invalid memory limits flags 0x%x", flags);
    }
    // Decode the three bits.
    bool has_maximum = flags & 0x1;
    bool is_shared = flags & 0x2;
    bool is_memory64 = flags & 0x4;
    // Store into output parameters.
    *has_maximum_out = has_maximum;
    *is_shared_out = is_shared;
    *is_memory64_out = is_memory64;

    // V8 does not support shared memory without a maximum.
    if (is_shared && !has_maximum) {
      error(pc() - 1, "shared memory must have a maximum defined");
    }

    if (is_memory64 && !enabled_features_.has_memory64()) {
      errorf(pc() - 1,
             "invalid memory limits flags 0x%x (enable via "
             "--experimental-wasm-memory64)",
             flags);
    }

    // Tracing.
    if (tracer_) {
      if (is_shared) tracer_->Description(" shared");
      if (is_memory64) tracer_->Description(" mem64");
      tracer_->Description(has_maximum ? " with maximum" : " no maximum");
      tracer_->NextLine();
    }
  }

  enum ResizableLimitsType : bool { k32BitLimits, k64BitLimits };
  void consume_resizable_limits(const char* name, const char* units,
                                uint32_t max_initial, uint32_t* initial,
                                bool has_maximum, uint32_t max_maximum,
                                uint32_t* maximum, ResizableLimitsType type) {
    const byte* pos = pc();
    // Note that even if we read the values as 64-bit value, all V8 limits are
    // still within uint32_t range.
    uint64_t initial_64 = type == k64BitLimits
                              ? consume_u64v("initial size", tracer_)
                              : consume_u32v("initial size", tracer_);
    if (initial_64 > max_initial) {
      errorf(pos,
             "initial %s size (%" PRIu64
             " %s) is larger than implementation limit (%u %s)",
             name, initial_64, units, max_initial, units);
    }
    *initial = static_cast<uint32_t>(initial_64);
    if (tracer_) {
      tracer_->Description(*initial);
      tracer_->NextLine();
    }
    if (has_maximum) {
      pos = pc();
      uint64_t maximum_64 = type == k64BitLimits
                                ? consume_u64v("maximum size", tracer_)
                                : consume_u32v("maximum size", tracer_);
      if (maximum_64 > max_maximum) {
        errorf(pos,
               "maximum %s size (%" PRIu64
               " %s) is larger than implementation limit (%u %s)",
               name, maximum_64, units, max_maximum, units);
      }
      if (maximum_64 < *initial) {
        errorf(pos,
               "maximum %s size (%" PRIu64 " %s) is less than initial (%u %s)",
               name, maximum_64, units, *initial, units);
      }
      *maximum = static_cast<uint32_t>(maximum_64);
      if (tracer_) {
        tracer_->Description(*maximum);
        tracer_->NextLine();
      }
    } else {
      *maximum = max_initial;
    }
  }

  // Consumes a byte, and emits an error if it does not equal {expected}.
  bool expect_u8(const char* name, uint8_t expected) {
    const byte* pos = pc();
    uint8_t value = consume_u8(name);
    if (value != expected) {
      errorf(pos, "expected %s 0x%02x, got 0x%02x", name, expected, value);
      return false;
    }
    return true;
  }

  ConstantExpression consume_init_expr(WasmModule* module, ValueType expected) {
    // The error message mimics the one generated by the {WasmFullDecoder}.
#define TYPE_CHECK(found)                                                \
  if (V8_UNLIKELY(!IsSubtypeOf(found, expected, module))) {              \
    errorf(pc() + 1,                                                     \
           "type error in constant expression[0] (expected %s, got %s)", \
           expected.name().c_str(), found.name().c_str());               \
    return {};                                                           \
  }

    if (tracer_) tracer_->NextLineIfNonEmpty();
    // To avoid initializing a {WasmFullDecoder} for the most common
    // expressions, we replicate their decoding and validation here. The
    // manually handled cases correspond to {ConstantExpression}'s kinds.
    // We need to make sure to check that the expression ends in {kExprEnd};
    // otherwise, it is just the first operand of a composite expression, and we
    // fall back to the default case.
    if (!more()) {
      error("Beyond end of code");
      return {};
    }
    switch (static_cast<WasmOpcode>(*pc())) {
      case kExprI32Const: {
        auto [value, length] =
            read_i32v<FullValidationTag>(pc() + 1, "i32.const");
        if (V8_UNLIKELY(failed())) return {};
        if (V8_LIKELY(lookahead(1 + length, kExprEnd))) {
          TYPE_CHECK(kWasmI32)
          if (tracer_) {
            tracer_->InitializerExpression(pc_, pc_ + length + 2, kWasmI32);
          }
          consume_bytes(length + 2);
          return ConstantExpression::I32Const(value);
        }
        break;
      }
      case kExprRefFunc: {
        auto [index, length] =
            read_u32v<FullValidationTag>(pc() + 1, "ref.func");
        if (V8_UNLIKELY(failed())) return {};
        if (V8_LIKELY(lookahead(1 + length, kExprEnd))) {
          if (V8_UNLIKELY(index >= module->functions.size())) {
            errorf(pc() + 1, "function index %u out of bounds", index);
            return {};
          }
          ValueType type =
              enabled_features_.has_typed_funcref()
                  ? ValueType::Ref(module->functions[index].sig_index)
                  : kWasmFuncRef;
          TYPE_CHECK(type)
          module->functions[index].declared = true;
          if (tracer_) {
            tracer_->InitializerExpression(pc_, pc_ + length + 2, type);
          }
          consume_bytes(length + 2);
          return ConstantExpression::RefFunc(index);
        }
        break;
      }
      case kExprRefNull: {
        auto [type, length] =
            value_type_reader::read_heap_type<FullValidationTag>(
                this, pc() + 1, enabled_features_);
        value_type_reader::ValidateHeapType<FullValidationTag>(this, pc_,
                                                               module, type);
        if (V8_UNLIKELY(failed())) return {};
        if (V8_LIKELY(lookahead(1 + length, kExprEnd))) {
          TYPE_CHECK(ValueType::RefNull(type))
          if (tracer_) {
            tracer_->InitializerExpression(pc_, pc_ + length + 2,
                                           ValueType::RefNull(type));
          }
          consume_bytes(length + 2);
          return ConstantExpression::RefNull(type.representation());
        }
        break;
      }
      default:
        break;
    }
#undef TYPE_CHECK

    auto sig = FixedSizeSignature<ValueType>::Returns(expected);
    FunctionBody body(&sig, buffer_offset_, pc_, end_);
    WasmFeatures detected;
    WasmFullDecoder<Decoder::FullValidationTag, ConstantExpressionInterface,
                    kConstantExpression>
        decoder(&init_expr_zone_, module, enabled_features_, &detected, body,
                module);

    uint32_t offset = this->pc_offset();

    decoder.DecodeFunctionBody();

    if (tracer_) {
      tracer_->InitializerExpression(pc_, decoder.end(), expected);
    }
    this->pc_ = decoder.end();

    if (decoder.failed()) {
      error(decoder.error().offset(), decoder.error().message().c_str());
      return {};
    }

    if (!decoder.interface().end_found()) {
      error("constant expression is missing 'end'");
      return {};
    }

    return ConstantExpression::WireBytes(
        offset, static_cast<uint32_t>(decoder.end() - decoder.start()));
  }

  // Read a mutability flag
  bool consume_mutability() {
    if (tracer_) tracer_->Bytes(pc_, 1);
    byte val = consume_u8("mutability");
    if (tracer_) {
      tracer_->Description(val == 0   ? " immutable"
                           : val == 1 ? " mutable"
                                      : " invalid");
    }
    if (val > 1) error(pc_ - 1, "invalid mutability");
    return val != 0;
  }

  ValueType consume_value_type() {
    auto [result, length] =
        value_type_reader::read_value_type<FullValidationTag>(
            this, pc_,
            module_->origin == kWasmOrigin ? enabled_features_
                                           : WasmFeatures::None());
    value_type_reader::ValidateValueType<FullValidationTag>(
        this, pc_, module_.get(), result);
    if (tracer_) {
      tracer_->Bytes(pc_, length);
      tracer_->Description(result);
    }
    consume_bytes(length, "value type");
    return result;
  }

  ValueType consume_storage_type() {
    uint8_t opcode = read_u8<FullValidationTag>(this->pc());
    switch (opcode) {
      case kI8Code:
        consume_bytes(1, " i8", tracer_);
        return kWasmI8;
      case kI16Code:
        consume_bytes(1, " i16", tracer_);
        return kWasmI16;
      default:
        // It is not a packed type, so it has to be a value type.
        return consume_value_type();
    }
  }

  const FunctionSig* consume_sig(Zone* zone) {
    if (tracer_) tracer_->NextLine();
    // Parse parameter types.
    uint32_t param_count =
        consume_count("param count", kV8MaxWasmFunctionParams);
    if (failed()) return nullptr;
    std::vector<ValueType> params;
    for (uint32_t i = 0; ok() && i < param_count; ++i) {
      params.push_back(consume_value_type());
      if (tracer_) tracer_->NextLineIfFull();
    }
    if (tracer_) tracer_->NextLineIfNonEmpty();
    if (failed()) return nullptr;

    // Parse return types.
    std::vector<ValueType> returns;
    uint32_t return_count =
        consume_count("return count", kV8MaxWasmFunctionReturns);
    if (failed()) return nullptr;
    for (uint32_t i = 0; ok() && i < return_count; ++i) {
      returns.push_back(consume_value_type());
      if (tracer_) tracer_->NextLineIfFull();
    }
    if (tracer_) tracer_->NextLineIfNonEmpty();
    if (failed()) return nullptr;

    // FunctionSig stores the return types first.
    ValueType* buffer = zone->NewArray<ValueType>(param_count + return_count);
    uint32_t b = 0;
    for (uint32_t i = 0; i < return_count; ++i) buffer[b++] = returns[i];
    for (uint32_t i = 0; i < param_count; ++i) buffer[b++] = params[i];

    return zone->New<FunctionSig>(return_count, param_count, buffer);
  }

  const StructType* consume_struct(Zone* zone) {
    uint32_t field_count =
        consume_count(", field count", kV8MaxWasmStructFields);
    if (failed()) return nullptr;
    ValueType* fields = zone->NewArray<ValueType>(field_count);
    bool* mutabilities = zone->NewArray<bool>(field_count);
    for (uint32_t i = 0; ok() && i < field_count; ++i) {
      fields[i] = consume_storage_type();
      mutabilities[i] = consume_mutability();
      if (tracer_) tracer_->NextLine();
    }
    if (failed()) return nullptr;
    uint32_t* offsets = zone->NewArray<uint32_t>(field_count);
    StructType* result =
        zone->New<StructType>(field_count, offsets, fields, mutabilities);
    result->InitializeOffsets();
    return result;
  }

  const ArrayType* consume_array(Zone* zone) {
    ValueType element_type = consume_storage_type();
    bool mutability = consume_mutability();
    if (tracer_) tracer_->NextLine();
    if (failed()) return nullptr;
    return zone->New<ArrayType>(element_type, mutability);
  }

  // Consume the attribute field of an exception.
  uint32_t consume_exception_attribute() {
    const byte* pos = pc_;
    uint32_t attribute = consume_u32v("exception attribute");
    if (tracer_) tracer_->Bytes(pos, static_cast<uint32_t>(pc_ - pos));
    if (attribute != kExceptionAttribute) {
      errorf(pos, "exception attribute %u not supported", attribute);
      return 0;
    }
    return attribute;
  }

  WasmElemSegment consume_element_segment_header() {
    const byte* pos = pc();

    // The mask for the bit in the flag which indicates if the segment is
    // active or not (0 is active).
    constexpr uint8_t kNonActiveMask = 1 << 0;
    // The mask for the bit in the flag which indicates:
    // - for active tables, if the segment has an explicit table index field.
    // - for non-active tables, whether the table is declarative (vs. passive).
    constexpr uint8_t kHasTableIndexOrIsDeclarativeMask = 1 << 1;
    // The mask for the bit in the flag which indicates if the functions of this
    // segment are defined as function indices (0) or constant expressions (1).
    constexpr uint8_t kExpressionsAsElementsMask = 1 << 2;
    constexpr uint8_t kFullMask = kNonActiveMask |
                                  kHasTableIndexOrIsDeclarativeMask |
                                  kExpressionsAsElementsMask;

    uint32_t flag = consume_u32v("flag: ", tracer_);
    if ((flag & kFullMask) != flag) {
      errorf(pos, "illegal flag value %u. Must be between 0 and 7", flag);
      return {};
    }

    const WasmElemSegment::Status status =
        (flag & kNonActiveMask) ? (flag & kHasTableIndexOrIsDeclarativeMask)
                                      ? WasmElemSegment::kStatusDeclarative
                                      : WasmElemSegment::kStatusPassive
                                : WasmElemSegment::kStatusActive;
    const bool is_active = status == WasmElemSegment::kStatusActive;
    if (tracer_) {
      tracer_->Description(status == WasmElemSegment::kStatusActive ? "active"
                           : status == WasmElemSegment::kStatusPassive
                               ? "passive,"
                               : "declarative,");
    }

    WasmElemSegment::ElementType element_type =
        flag & kExpressionsAsElementsMask
            ? WasmElemSegment::kExpressionElements
            : WasmElemSegment::kFunctionIndexElements;

    const bool has_table_index =
        is_active && (flag & kHasTableIndexOrIsDeclarativeMask);
    uint32_t table_index = 0;
    if (has_table_index) {
      table_index = consume_u32v(", table index", tracer_);
      if (tracer_) tracer_->Description(table_index);
    }
    if (V8_UNLIKELY(is_active && table_index >= module_->tables.size())) {
      errorf(pos, "out of bounds%s table index %u",
             has_table_index ? " implicit" : "", table_index);
      return {};
    }
    ValueType table_type =
        is_active ? module_->tables[table_index].type : kWasmBottom;

    ConstantExpression offset;
    if (is_active) {
      if (tracer_) {
        tracer_->Description(", offset:");
        tracer_->NextLine();
      }
      offset = consume_init_expr(module_.get(), kWasmI32);
      // Failed to parse offset initializer, return early.
      if (failed()) return {};
    }

    // Denotes an active segment without table index, type, or element kind.
    const bool backwards_compatible_mode =
        is_active && !(flag & kHasTableIndexOrIsDeclarativeMask);
    ValueType type;
    if (element_type == WasmElemSegment::kExpressionElements) {
      if (backwards_compatible_mode) {
        type = kWasmFuncRef;
      } else {
        if (tracer_) tracer_->Description(" element type:");
        type = consume_value_type();
        if (failed()) return {};
      }
      if (V8_UNLIKELY(is_active &&
                      !IsSubtypeOf(type, table_type, this->module_.get()))) {
        errorf(pos,
               "Element segment of type %s is not a subtype of referenced "
               "table %u (of type %s)",
               type.name().c_str(), table_index, table_type.name().c_str());
        return {};
      }
    } else {
      if (!backwards_compatible_mode) {
        // We have to check that there is an element kind of type Function. All
        // other element kinds are not valid yet.
        uint8_t val = consume_u8(" element type: function", tracer_);
        if (V8_UNLIKELY(static_cast<ImportExportKindCode>(val) !=
                        kExternalFunction)) {
          errorf(pos, "illegal element kind 0x%x. Must be 0x%x", val,
                 kExternalFunction);
          return {};
        }
      }
      if (!is_active) {
        // Declarative and passive segments without explicit type are funcref.
        type = kWasmFuncRef;
      } else {
        type = table_type;
        // Active segments with function indices must reference a function
        // table. (Using struct / array indices doesn't provide any value
        // as such an index doesn't refer to a unique object instance unlike
        // functions.)
        if (V8_UNLIKELY(
                !IsSubtypeOf(table_type, kWasmFuncRef, this->module_.get()))) {
          errorf(pos,
                 "An active element segment with function indices as elements "
                 "must reference a table of %s. Instead, table %u of type %s "
                 "is referenced.",
                 enabled_features_.has_typed_funcref()
                     ? "a subtype of type funcref"
                     : "type funcref",
                 table_index, table_type.name().c_str());
          return {};
        }
      }
    }

    uint32_t num_elem =
        consume_count("number of elements", max_table_init_entries());

    if (is_active) {
      return {type,         table_index, std::move(offset),
              element_type, num_elem,    pc_offset()};
    } else {
      return {type, status, element_type, num_elem, pc_offset()};
    }
  }

  void consume_data_segment_header(bool* is_active, uint32_t* index,
                                   ConstantExpression* offset) {
    const byte* pos = pc();
    uint32_t flag = consume_u32v("flag: ", tracer_);
    if (tracer_) {
      tracer_->Description(
          flag == SegmentFlags::kActiveNoIndex     ? "active no index"
          : flag == SegmentFlags::kPassive         ? "passive"
          : flag == SegmentFlags::kActiveWithIndex ? "active with index"
                                                   : "unknown");
      tracer_->NextLine();
    }

    // Some flag values are only valid for specific proposals.
    if (flag != SegmentFlags::kActiveNoIndex &&
        flag != SegmentFlags::kPassive &&
        flag != SegmentFlags::kActiveWithIndex) {
      errorf(pos, "illegal flag value %u. Must be 0, 1, or 2", flag);
      return;
    }

    // We know now that the flag is valid. Time to read the rest.
    ValueType expected_type = module_->is_memory64 ? kWasmI64 : kWasmI32;
    if (flag == SegmentFlags::kActiveNoIndex) {
      *is_active = true;
      *index = 0;
      *offset = consume_init_expr(module_.get(), expected_type);
      return;
    }
    if (flag == SegmentFlags::kPassive) {
      *is_active = false;
      return;
    }
    if (flag == SegmentFlags::kActiveWithIndex) {
      *is_active = true;
      *index = consume_u32v("memory index", tracer_);
      if (tracer_) tracer_->Description(*index);
      *offset = consume_init_expr(module_.get(), expected_type);
    }
  }

  uint32_t consume_element_func_index(WasmModule* module, ValueType expected) {
    WasmFunction* func = nullptr;
    const byte* initial_pc = pc();
    uint32_t index = consume_func_index(module, &func);
    if (tracer_) tracer_->NextLine();
    if (failed()) return index;
    DCHECK_NOT_NULL(func);
    DCHECK_EQ(index, func->func_index);
    ValueType entry_type = ValueType::Ref(func->sig_index);
    if (V8_UNLIKELY(!IsSubtypeOf(entry_type, expected, module))) {
      errorf(initial_pc,
             "Invalid type in element entry: expected %s, got %s instead.",
             expected.name().c_str(), entry_type.name().c_str());
      return index;
    }
    func->declared = true;
    return index;
  }

  const WasmFeatures enabled_features_;
  const std::shared_ptr<WasmModule> module_;
  const byte* module_start_ = nullptr;
  const byte* module_end_ = nullptr;
  ITracer* tracer_;
  // The type section is the first section in a module.
  uint8_t next_ordered_section_ = kFirstSectionInModule;
  // We store next_ordered_section_ as uint8_t instead of SectionCode so that
  // we can increment it. This static_assert should make sure that SectionCode
  // does not get bigger than uint8_t accidentally.
  static_assert(sizeof(ModuleDecoderImpl::next_ordered_section_) ==
                    sizeof(SectionCode),
                "type mismatch");
  uint32_t seen_unordered_sections_ = 0;
  static_assert(kBitsPerByte *
                        sizeof(ModuleDecoderImpl::seen_unordered_sections_) >
                    kLastKnownModuleSection,
                "not enough bits");
  AccountingAllocator allocator_;
  Zone init_expr_zone_{&allocator_, "constant expr. zone"};

  // Instruction traces are decoded in DecodeInstTraceSection as a 3-tuple
  // of the function index, function offset, and mark_id. In DecodeCodeSection,
  // after the functions have been decoded this is translated to pairs of module
  // offsets and mark ids.
  std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> inst_traces_;
};

}  // namespace v8::internal::wasm

#undef TRACE

#endif  // V8_WASM_MODULE_DECODER_IMPL_H_
