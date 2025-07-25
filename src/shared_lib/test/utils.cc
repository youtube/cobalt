/*
 * Copyright (C) 2023 The Android Open Source Project
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

#include "src/shared_lib/test/utils.h"

#include "perfetto/public/abi/heap_buffer.h"
#include "perfetto/public/pb_msg.h"
#include "perfetto/public/pb_utils.h"
#include "perfetto/public/protos/config/data_source_config.pzc.h"
#include "perfetto/public/protos/config/trace_config.pzc.h"
#include "perfetto/public/tracing_session.h"

namespace perfetto {
namespace shlib {
namespace test_utils {
namespace {

std::string ToHexChars(uint8_t val) {
  std::string ret;
  uint8_t high_nibble = (val & 0xF0) >> 4;
  uint8_t low_nibble = (val & 0xF);
  static const char hex_chars[] = "0123456789ABCDEF";
  ret.push_back(hex_chars[high_nibble]);
  ret.push_back(hex_chars[low_nibble]);
  return ret;
}

}  // namespace

TracingSession TracingSession::Builder::Build() {
  struct PerfettoPbMsgWriter writer;
  struct PerfettoHeapBuffer* hb = PerfettoHeapBufferCreate(&writer.writer);

  struct perfetto_protos_TraceConfig cfg;
  PerfettoPbMsgInit(&cfg.msg, &writer);

  {
    struct perfetto_protos_TraceConfig_BufferConfig buffers;
    perfetto_protos_TraceConfig_begin_buffers(&cfg, &buffers);

    perfetto_protos_TraceConfig_BufferConfig_set_size_kb(&buffers, 1024);

    perfetto_protos_TraceConfig_end_buffers(&cfg, &buffers);
  }

  {
    struct perfetto_protos_TraceConfig_DataSource data_sources;
    perfetto_protos_TraceConfig_begin_data_sources(&cfg, &data_sources);

    {
      struct perfetto_protos_DataSourceConfig ds_cfg;
      perfetto_protos_TraceConfig_DataSource_begin_config(&data_sources,
                                                          &ds_cfg);

      perfetto_protos_DataSourceConfig_set_cstr_name(&ds_cfg,
                                                     data_source_name_.c_str());

      perfetto_protos_TraceConfig_DataSource_end_config(&data_sources, &ds_cfg);
    }

    perfetto_protos_TraceConfig_end_data_sources(&cfg, &data_sources);
  }
  size_t cfg_size = PerfettoStreamWriterGetWrittenSize(&writer.writer);
  std::unique_ptr<uint8_t[]> ser(new uint8_t[cfg_size]);
  PerfettoHeapBufferCopyInto(hb, &writer.writer, ser.get(), cfg_size);
  PerfettoHeapBufferDestroy(hb, &writer.writer);

  struct PerfettoTracingSessionImpl* ts =
      PerfettoTracingSessionCreate(PERFETTO_BACKEND_IN_PROCESS);

  PerfettoTracingSessionSetup(ts, ser.get(), cfg_size);

  PerfettoTracingSessionStartBlocking(ts);

  TracingSession ret;
  ret.session_ = ts;
  return ret;
}

TracingSession::TracingSession(TracingSession&& other) noexcept {
  session_ = other.session_;
  other.session_ = nullptr;
}

TracingSession::~TracingSession() {
  if (!session_) {
    return;
  }
  if (!stopped_) {
    PerfettoTracingSessionStopBlocking(session_);
  }
  PerfettoTracingSessionDestroy(session_);
}

void TracingSession::StopBlocking() {
  stopped_ = true;
  PerfettoTracingSessionStopBlocking(session_);
}

std::vector<uint8_t> TracingSession::ReadBlocking() {
  std::vector<uint8_t> data;
  PerfettoTracingSessionReadTraceBlocking(
      session_,
      [](struct PerfettoTracingSessionImpl*, const void* trace_data,
         size_t size, bool, void* user_arg) {
        auto& dst = *static_cast<std::vector<uint8_t>*>(user_arg);
        auto* src = static_cast<const uint8_t*>(trace_data);
        dst.insert(dst.end(), src, src + size);
      },
      &data);
  return data;
}

}  // namespace test_utils
}  // namespace shlib
}  // namespace perfetto

void PrintTo(const PerfettoPbDecoderField& field, std::ostream* pos) {
  std::ostream& os = *pos;
  PerfettoPbDecoderStatus status =
      static_cast<PerfettoPbDecoderStatus>(field.status);
  switch (status) {
    case PERFETTO_PB_DECODER_ERROR:
      os << "MALFORMED PROTOBUF";
      break;
    case PERFETTO_PB_DECODER_DONE:
      os << "DECODER DONE";
      break;
    case PERFETTO_PB_DECODER_OK:
      switch (field.wire_type) {
        case PERFETTO_PB_WIRE_TYPE_DELIMITED:
          os << "\"";
          for (size_t i = 0; i < field.value.delimited.len; i++) {
            os << perfetto::shlib::test_utils::ToHexChars(
                      field.value.delimited.start[i])
               << " ";
          }
          os << "\"";
          break;
        case PERFETTO_PB_WIRE_TYPE_VARINT:
          os << "varint: " << field.value.integer64;
          break;
        case PERFETTO_PB_WIRE_TYPE_FIXED32:
          os << "fixed32: " << field.value.integer32;
          break;
        case PERFETTO_PB_WIRE_TYPE_FIXED64:
          os << "fixed64: " << field.value.integer64;
          break;
      }
      break;
  }
}
