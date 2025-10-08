/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/trace_processor/forwarding_trace_parser.h"

<<<<<<< HEAD
#include <memory>
#include <optional>
#include <utility>

#include "perfetto/base/logging.h"
#include "perfetto/base/status.h"
#include "perfetto/ext/base/status_or.h"
#include "perfetto/trace_processor/basic_types.h"
#include "src/trace_processor/importers/common/chunked_trace_reader.h"
#include "src/trace_processor/importers/common/process_tracker.h"
#include "src/trace_processor/importers/common/trace_file_tracker.h"
#include "src/trace_processor/importers/proto/proto_trace_reader.h"
#include "src/trace_processor/sorter/trace_sorter.h"
#include "src/trace_processor/storage/stats.h"
#include "src/trace_processor/tables/metadata_tables_py.h"
#include "src/trace_processor/trace_reader_registry.h"
#include "src/trace_processor/types/trace_processor_context.h"
#include "src/trace_processor/util/status_macros.h"
#include "src/trace_processor/util/trace_type.h"

namespace perfetto::trace_processor {
namespace {

TraceSorter::SortingMode ConvertSortingMode(SortingMode sorting_mode) {
  switch (sorting_mode) {
    case SortingMode::kDefaultHeuristics:
=======
#include "perfetto/base/logging.h"
#include "perfetto/ext/base/string_utils.h"
#include "src/trace_processor/importers/common/process_tracker.h"
#include "src/trace_processor/importers/proto/proto_trace_parser.h"
#include "src/trace_processor/importers/proto/proto_trace_reader.h"
#include "src/trace_processor/sorter/trace_sorter.h"

namespace perfetto {
namespace trace_processor {
namespace {

const char kNoZlibErr[] =
    "Cannot open compressed trace. zlib not enabled in the build config";

inline bool isspace(unsigned char c) {
  return ::isspace(c);
}

std::string RemoveWhitespace(std::string str) {
  str.erase(std::remove_if(str.begin(), str.end(), isspace), str.end());
  return str;
}

TraceSorter::SortingMode ConvertSortingMode(SortingMode sorting_mode) {
  switch (sorting_mode) {
    case SortingMode::kDefaultHeuristics:
    case SortingMode::kForceFlushPeriodWindowedSort:
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
      return TraceSorter::SortingMode::kDefault;
    case SortingMode::kForceFullSort:
      return TraceSorter::SortingMode::kFullSort;
  }
  PERFETTO_FATAL("For GCC");
}

<<<<<<< HEAD
std::optional<TraceSorter::SortingMode> GetMinimumSortingMode(
    TraceType trace_type,
    const TraceProcessorContext& context) {
  switch (trace_type) {
    case kNinjaLogTraceType:
    case kSystraceTraceType:
    case kGzipTraceType:
    case kCtraceTraceType:
    case kArtHprofTraceType:
      return std::nullopt;

    case kPerfDataTraceType:
    case kInstrumentsXmlTraceType:
      return TraceSorter::SortingMode::kDefault;

    case kUnknownTraceType:
    case kJsonTraceType:
    case kFuchsiaTraceType:
    case kZipFile:
    case kTarTraceType:
    case kAndroidDumpstateTraceType:
    case kAndroidLogcatTraceType:
    case kGeckoTraceType:
    case kArtMethodTraceType:
    case kPerfTextTraceType:
      return TraceSorter::SortingMode::kFullSort;

    case kProtoTraceType:
    case kSymbolsTraceType:
      return ConvertSortingMode(context.config.sorting_mode);

    case kAndroidBugreportTraceType:
      PERFETTO_FATAL(
          "This trace type should be handled at the ZipParser level");
  }
  PERFETTO_FATAL("For GCC");
}

}  // namespace

ForwardingTraceParser::ForwardingTraceParser(TraceProcessorContext* context,
                                             tables::TraceFileTable::Id id)
    : context_(context), file_id_(id) {}

ForwardingTraceParser::~ForwardingTraceParser() = default;

base::Status ForwardingTraceParser::Init(const TraceBlobView& blob) {
  PERFETTO_CHECK(!reader_);

  {
    auto scoped_trace = context_->storage->TraceExecutionTimeIntoStats(
        stats::guess_trace_type_duration_ns);
    trace_type_ = GuessTraceType(blob.data(), blob.size());
  }
  if (trace_type_ == kUnknownTraceType) {
    // If renaming this error message don't remove the "(ERR:fmt)" part.
    // The UI's error_dialog.ts uses it to make the dialog more graceful.
    return base::ErrStatus("Unknown trace type provided (ERR:fmt)");
  }
  context_->trace_file_tracker->StartParsing(file_id_, trace_type_);
  ASSIGN_OR_RETURN(reader_,
                   context_->reader_registry->CreateTraceReader(trace_type_));

  PERFETTO_DLOG("%s trace detected", TraceTypeToString(trace_type_));
  UpdateSorterForTraceType(trace_type_);

  // TODO(b/334978369) Make sure kProtoTraceType and kSystraceTraceType are
  // parsed first so that we do not get issues with
  // SetPidZeroIsUpidZeroIdleProcess()
  if (trace_type_ == kProtoTraceType || trace_type_ == kSystraceTraceType) {
    context_->process_tracker->SetPidZeroIsUpidZeroIdleProcess();
  }
  return base::OkStatus();
}

void ForwardingTraceParser::UpdateSorterForTraceType(TraceType trace_type) {
  std::optional<TraceSorter::SortingMode> minimum_sorting_mode =
      GetMinimumSortingMode(trace_type, *context_);
  if (!minimum_sorting_mode.has_value()) {
    return;
  }

  if (!context_->sorter) {
    TraceSorter::EventHandling event_handling;
    switch (context_->config.parsing_mode) {
      case ParsingMode::kDefault:
        event_handling = TraceSorter::EventHandling::kSortAndPush;
        break;
      case ParsingMode::kTokenizeOnly:
        event_handling = TraceSorter::EventHandling::kDrop;
        break;
      case ParsingMode::kTokenizeAndSort:
        event_handling = TraceSorter::EventHandling::kSortAndDrop;
        break;
    }
    if (context_->config.enable_dev_features) {
      auto it = context_->config.dev_flags.find("drop-after-sort");
      if (it != context_->config.dev_flags.end() && it->second == "true") {
        event_handling = TraceSorter::EventHandling::kSortAndDrop;
      }
    }
    context_->sorter = std::make_shared<TraceSorter>(
        context_, *minimum_sorting_mode, event_handling);
  }

  switch (context_->sorter->sorting_mode()) {
    case TraceSorter::SortingMode::kDefault:
      PERFETTO_CHECK(minimum_sorting_mode ==
                     TraceSorter::SortingMode::kDefault);
      break;
    case TraceSorter::SortingMode::kFullSort:
      break;
  }
}

base::Status ForwardingTraceParser::Parse(TraceBlobView blob) {
  // If this is the first Parse() call, guess the trace type and create the
  // appropriate parser.
  if (!reader_) {
    RETURN_IF_ERROR(Init(blob));
  }
  trace_size_ += blob.size();
  return reader_->Parse(std::move(blob));
}

base::Status ForwardingTraceParser::NotifyEndOfFile() {
  if (reader_) {
    RETURN_IF_ERROR(reader_->NotifyEndOfFile());
  }
  if (trace_type_ != kUnknownTraceType) {
    context_->trace_file_tracker->DoneParsing(file_id_, trace_size_);
  }
  return base::OkStatus();
}

}  // namespace perfetto::trace_processor
=======
// Fuchsia traces have a magic number as documented here:
// https://fuchsia.googlesource.com/fuchsia/+/HEAD/docs/development/tracing/trace-format/README.md#magic-number-record-trace-info-type-0
constexpr uint64_t kFuchsiaMagicNumber = 0x0016547846040010;

}  // namespace

ForwardingTraceParser::ForwardingTraceParser(TraceProcessorContext* context)
    : context_(context) {}

ForwardingTraceParser::~ForwardingTraceParser() {}

util::Status ForwardingTraceParser::Parse(TraceBlobView blob) {
  // If this is the first Parse() call, guess the trace type and create the
  // appropriate parser.
  if (!reader_) {
    TraceType trace_type;
    {
      auto scoped_trace = context_->storage->TraceExecutionTimeIntoStats(
          stats::guess_trace_type_duration_ns);
      trace_type = GuessTraceType(blob.data(), blob.size());
      context_->trace_type = trace_type;
    }
    switch (trace_type) {
      case kJsonTraceType: {
        PERFETTO_DLOG("JSON trace detected");
        if (context_->json_trace_tokenizer && context_->json_trace_parser) {
          reader_ = std::move(context_->json_trace_tokenizer);

          // JSON traces have no guarantees about the order of events in them.
          context_->sorter.reset(
              new TraceSorter(context_, std::move(context_->json_trace_parser),
                              TraceSorter::SortingMode::kFullSort));
        } else {
          return util::ErrStatus("JSON support is disabled");
        }
        break;
      }
      case kProtoTraceType: {
        PERFETTO_DLOG("Proto trace detected");
        auto sorting_mode = ConvertSortingMode(context_->config.sorting_mode);
        reader_.reset(new ProtoTraceReader(context_));
        context_->sorter.reset(new TraceSorter(
            context_,
            std::unique_ptr<TraceParser>(new ProtoTraceParser(context_)),
            sorting_mode));
        context_->process_tracker->SetPidZeroIsUpidZeroIdleProcess();
        break;
      }
      case kNinjaLogTraceType: {
        PERFETTO_DLOG("Ninja log detected");
        if (context_->ninja_log_parser) {
          reader_ = std::move(context_->ninja_log_parser);
          break;
        }
        return util::ErrStatus("Ninja support is disabled");
      }
      case kFuchsiaTraceType: {
        PERFETTO_DLOG("Fuchsia trace detected");
        if (context_->fuchsia_trace_parser &&
            context_->fuchsia_trace_tokenizer) {
          reader_ = std::move(context_->fuchsia_trace_tokenizer);

          // Fuschia traces can have massively out of order events.
          context_->sorter.reset(new TraceSorter(
              context_, std::move(context_->fuchsia_trace_parser),
              TraceSorter::SortingMode::kFullSort));
        } else {
          return util::ErrStatus("Fuchsia support is disabled");
        }
        break;
      }
      case kSystraceTraceType:
        PERFETTO_DLOG("Systrace trace detected");
        context_->process_tracker->SetPidZeroIsUpidZeroIdleProcess();
        if (context_->systrace_trace_parser) {
          reader_ = std::move(context_->systrace_trace_parser);
          break;
        } else {
          return util::ErrStatus("Systrace support is disabled");
        }
      case kGzipTraceType:
      case kCtraceTraceType:
        if (trace_type == kGzipTraceType) {
          PERFETTO_DLOG("gzip trace detected");
        } else {
          PERFETTO_DLOG("ctrace trace detected");
        }
        if (context_->gzip_trace_parser) {
          reader_ = std::move(context_->gzip_trace_parser);
          break;
        } else {
          return util::ErrStatus(kNoZlibErr);
        }
      case kAndroidBugreportTraceType:
        if (context_->android_bugreport_parser) {
          reader_ = std::move(context_->android_bugreport_parser);
          break;
        }
        return util::ErrStatus("Android Bugreport support is disabled. %s",
                               kNoZlibErr);
      case kUnknownTraceType:
        // If renaming this error message don't remove the "(ERR:fmt)" part.
        // The UI's error_dialog.ts uses it to make the dialog more graceful.
        return util::ErrStatus("Unknown trace type provided (ERR:fmt)");
    }
  }

  return reader_->Parse(std::move(blob));
}

void ForwardingTraceParser::NotifyEndOfFile() {
  reader_->NotifyEndOfFile();
}

TraceType GuessTraceType(const uint8_t* data, size_t size) {
  if (size == 0)
    return kUnknownTraceType;
  std::string start(reinterpret_cast<const char*>(data),
                    std::min<size_t>(size, kGuessTraceMaxLookahead));
  if (size >= 8) {
    uint64_t first_word;
    memcpy(&first_word, data, sizeof(first_word));
    if (first_word == kFuchsiaMagicNumber)
      return kFuchsiaTraceType;
  }
  std::string start_minus_white_space = RemoveWhitespace(start);
  if (base::StartsWith(start_minus_white_space, "{\""))
    return kJsonTraceType;
  if (base::StartsWith(start_minus_white_space, "[{\""))
    return kJsonTraceType;

  // Systrace with header but no leading HTML.
  if (base::Contains(start, "# tracer"))
    return kSystraceTraceType;

  // Systrace with leading HTML.
  // Both: <!DOCTYPE html> and <!DOCTYPE HTML> have been observed.
  std::string lower_start = base::ToLower(start);
  if (base::StartsWith(lower_start, "<!doctype html>") ||
      base::StartsWith(lower_start, "<html>"))
    return kSystraceTraceType;

  // Traces obtained from atrace -z (compress).
  // They all have the string "TRACE:" followed by 78 9C which is a zlib header
  // for "deflate, default compression, window size=32K" (see b/208691037)
  if (base::Contains(start, "TRACE:\n\x78\x9c"))
    return kCtraceTraceType;

  // Traces obtained from atrace without -z (no compression).
  if (base::Contains(start, "TRACE:\n"))
    return kSystraceTraceType;

  // Ninja's build log (.ninja_log).
  if (base::StartsWith(start, "# ninja log"))
    return kNinjaLogTraceType;

  // Systrace with no header or leading HTML.
  if (base::StartsWith(start, " "))
    return kSystraceTraceType;

  // gzip'ed trace containing one of the other formats.
  if (base::StartsWith(start, "\x1f\x8b"))
    return kGzipTraceType;

  if (base::StartsWith(start, "\x0a"))
    return kProtoTraceType;

  // Android bugreport.zip
  // TODO(primiano). For now we assume any .zip file is a bugreport. In future,
  // if we want to support different trace formats based on a .zip arachive we
  // will need an extra layer similar to what we did kGzipTraceType.
  if (base::StartsWith(start, "PK\x03\x04"))
    return kAndroidBugreportTraceType;

  return kUnknownTraceType;
}

}  // namespace trace_processor
}  // namespace perfetto
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))
