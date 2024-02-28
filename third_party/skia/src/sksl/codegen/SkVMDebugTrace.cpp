/*
 * Copyright 2021 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/core/SkStreamPriv.h"
#include "src/sksl/codegen/SkVMDebugTrace.h"
#include "src/utils/SkJSON.h"
#include "src/utils/SkJSONWriter.h"

#include <sstream>

namespace SkSL {

void SkVMDebugTrace::setTraceCoord(const SkIPoint& coord) {
    fTraceCoord = coord;
}

void SkVMDebugTrace::setSource(std::string source) {
    fSource.clear();
    std::stringstream stream{std::move(source)};
    while (stream.good()) {
        fSource.push_back({});
        std::getline(stream, fSource.back(), '\n');
    }
}

void SkVMDebugTrace::dump(SkWStream* o) const {
    for (size_t index = 0; index < fSlotInfo.size(); ++index) {
        const SkVMSlotInfo& info = fSlotInfo[index];

        o->writeText("$");
        o->writeDecAsText(index);
        o->writeText(" = ");
        o->writeText(info.name.c_str());
        o->writeText(" (");
        switch (info.numberKind) {
            case Type::NumberKind::kFloat:      o->writeText("float"); break;
            case Type::NumberKind::kSigned:     o->writeText("int"); break;
            case Type::NumberKind::kUnsigned:   o->writeText("uint"); break;
            case Type::NumberKind::kBoolean:    o->writeText("bool"); break;
            case Type::NumberKind::kNonnumeric: o->writeText("???"); break;
        }
        if (info.rows * info.columns > 1) {
            o->writeDecAsText(info.columns);
            if (info.rows != 1) {
                o->writeText("x");
                o->writeDecAsText(info.rows);
            }
            o->writeText(" : ");
            o->writeText("slot ");
            o->writeDecAsText(info.componentIndex + 1);
            o->writeText("/");
            o->writeDecAsText(info.rows * info.columns);
        }
        o->writeText(", L");
        o->writeDecAsText(info.line);
        o->writeText(")");
        o->newline();
    }

    for (size_t index = 0; index < fFuncInfo.size(); ++index) {
        const SkVMFunctionInfo& info = fFuncInfo[index];

        o->writeText("F");
        o->writeDecAsText(index);
        o->writeText(" = ");
        o->writeText(info.name.c_str());
        o->newline();
    }

    o->newline();

    if (!fTraceInfo.empty()) {
        std::string indent = "";
        for (const SkSL::SkVMTraceInfo& traceInfo : fTraceInfo) {
            int data0 = traceInfo.data[0];
            int data1 = traceInfo.data[1];
            switch (traceInfo.op) {
                case SkSL::SkVMTraceInfo::Op::kLine:
                    o->writeText(indent.c_str());
                    o->writeText("line ");
                    o->writeDecAsText(data0);
                    break;

                case SkSL::SkVMTraceInfo::Op::kVar: {
                    const SkVMSlotInfo& slot = fSlotInfo[data0];
                    o->writeText(indent.c_str());
                    o->writeText(slot.name.c_str());
                    if (slot.rows > 1) {
                        o->writeText("[");
                        o->writeDecAsText(slot.componentIndex / slot.rows);
                        o->writeText("][");
                        o->writeDecAsText(slot.componentIndex % slot.rows);
                        o->writeText("]");
                    } else if (slot.columns > 1) {
                        switch (slot.componentIndex) {
                            case 0:  o->writeText(".x"); break;
                            case 1:  o->writeText(".y"); break;
                            case 2:  o->writeText(".z"); break;
                            case 3:  o->writeText(".w"); break;
                            default: o->writeText("[???]"); break;
                        }
                    }
                    o->writeText(" = ");
                    switch (slot.numberKind) {
                        case SkSL::Type::NumberKind::kSigned:
                        case SkSL::Type::NumberKind::kUnsigned:
                        default:
                            o->writeDecAsText(data1);
                            break;
                        case SkSL::Type::NumberKind::kBoolean:
                            o->writeText(data1 ? "true" : "false");
                            break;
                        case SkSL::Type::NumberKind::kFloat: {
                            float floatVal;
                            static_assert(sizeof(floatVal) == sizeof(data1));
                            memcpy(&floatVal, &data1, sizeof(floatVal));
                            o->writeScalarAsText(floatVal);
                            break;
                        }
                    }
                    break;
                }
                case SkSL::SkVMTraceInfo::Op::kEnter:
                    o->writeText(indent.c_str());
                    o->writeText("enter ");
                    o->writeText(fFuncInfo[data0].name.c_str());
                    indent += "  ";
                    break;

                case SkSL::SkVMTraceInfo::Op::kExit:
                    indent.resize(indent.size() - 2);
                    o->writeText(indent.c_str());
                    o->writeText("exit ");
                    o->writeText(fFuncInfo[data0].name.c_str());
                    break;
            }
            o->newline();
        }
    }
}

void SkVMDebugTrace::writeTrace(SkWStream* w) const {
    SkJSONWriter json(w);

    json.beginObject(); // root
    json.beginArray("source");

    for (const std::string& line : fSource) {
        json.appendString(line.c_str());
    }

    json.endArray(); // code
    json.beginArray("slots");

    for (size_t index = 0; index < fSlotInfo.size(); ++index) {
        const SkVMSlotInfo& info = fSlotInfo[index];

        json.beginObject();
        json.appendS32("slot", index);
        json.appendString("name", info.name.c_str());
        json.appendS32("columns", info.columns);
        json.appendS32("rows", info.rows);
        json.appendS32("index", info.componentIndex);
        json.appendS32("kind", (int)info.numberKind);
        json.appendS32("line", info.line);
        json.endObject();
    }

    json.endArray(); // slots
    json.beginArray("functions");

    for (size_t index = 0; index < fFuncInfo.size(); ++index) {
        const SkVMFunctionInfo& info = fFuncInfo[index];

        json.beginObject();
        json.appendS32("slot", index);
        json.appendString("name", info.name.c_str());
        json.endObject();
    }

    json.endArray(); // functions
    json.beginArray("trace");

    for (size_t index = 0; index < fTraceInfo.size(); ++index) {
        const SkVMTraceInfo& trace = fTraceInfo[index];
        json.beginArray();
        json.appendS32((int)trace.op);

        // Skip trailing zeros in the data (since most ops only use one value).
        int lastDataIdx = SK_ARRAY_COUNT(trace.data) - 1;
        while (lastDataIdx >= 0 && !trace.data[lastDataIdx]) {
            --lastDataIdx;
        }
        for (int dataIdx = 0; dataIdx <= lastDataIdx; ++dataIdx) {
            json.appendS32(trace.data[dataIdx]);
        }
        json.endArray();
    }

    json.endArray(); // trace
    json.endObject(); // root
    json.flush();
}

bool SkVMDebugTrace::readTrace(SkStream* r) {
    sk_sp<SkData> data = SkCopyStreamToData(r);
    skjson::DOM json(reinterpret_cast<const char*>(data->bytes()), data->size());
    const skjson::ObjectValue* root = json.root();
    if (!root) {
        return false;
    }

    const skjson::ArrayValue* source = (*root)["source"];
    if (!source) {
        return false;
    }

    fSource.clear();
    for (const skjson::StringValue* line : *source) {
        if (!line) {
            return false;
        }
        fSource.push_back(line->begin());
    }

    const skjson::ArrayValue* slots = (*root)["slots"];
    if (!slots) {
        return false;
    }

    fSlotInfo.clear();
    for (const skjson::ObjectValue* element : *slots) {
        if (!element) {
            return false;
        }

        // Grow the slot array to hold this element. (But don't shrink it if we somehow get our
        // slots out of order!)
        const skjson::NumberValue* slot = (*element)["slot"];
        if (!slot) {
            return false;
        }
        fSlotInfo.resize(std::max(fSlotInfo.size(), (size_t)(**slot + 1)));
        SkVMSlotInfo& info = fSlotInfo[(size_t)(**slot)];

        // Populate the SlotInfo with our JSON data.
        const skjson::StringValue* name    = (*element)["name"];
        const skjson::NumberValue* columns = (*element)["columns"];
        const skjson::NumberValue* rows    = (*element)["rows"];
        const skjson::NumberValue* index   = (*element)["index"];
        const skjson::NumberValue* kind    = (*element)["kind"];
        const skjson::NumberValue* line    = (*element)["line"];
        if (!name || !columns || !rows || !index || !kind || !line) {
            return false;
        }

        info.name = name->begin();
        info.columns = **columns;
        info.rows = **rows;
        info.componentIndex = **index;
        info.numberKind = (SkSL::Type::NumberKind)(int)**kind;
        info.line = **line;
    }

    const skjson::ArrayValue* functions = (*root)["functions"];
    if (!functions) {
        return false;
    }

    fFuncInfo.clear();
    for (const skjson::ObjectValue* element : *functions) {
        if (!element) {
            return false;
        }

        // Grow the function array to hold this element. (But don't shrink it if we somehow get our
        // functions out of order!)
        const skjson::NumberValue* slot = (*element)["slot"];
        if (!slot) {
            return false;
        }
        fFuncInfo.resize(std::max(fFuncInfo.size(), (size_t)(**slot + 1)));
        SkVMFunctionInfo& info = fFuncInfo[(size_t)(**slot)];

        // Populate the FunctionInfo with our JSON data.
        const skjson::StringValue* name = (*element)["name"];
        if (!name) {
            return false;
        }

        info.name = name->begin();
    }

    const skjson::ArrayValue* trace = (*root)["trace"];
    if (!trace) {
        return false;
    }

    fTraceInfo.clear();
    fTraceInfo.reserve(trace->size());
    for (const skjson::ArrayValue* element : *trace) {
        fTraceInfo.push_back(SkVMTraceInfo{});
        SkVMTraceInfo& info = fTraceInfo.back();

        if (!element || element->size() < 1 || element->size() > (1 + SK_ARRAY_COUNT(info.data))) {
            return false;
        }
        const skjson::NumberValue* opVal = (*element)[0];
        if (!opVal) {
            return false;
        }
        info.op = (SkVMTraceInfo::Op)(int)**opVal;
        for (size_t elemIdx = 1; elemIdx < element->size(); ++elemIdx) {
            const skjson::NumberValue* dataVal = (*element)[elemIdx];
            if (!dataVal) {
                return false;
            }
            info.data[elemIdx - 1] = **dataVal;
        }
    }

    return true;
}

}  // namespace SkSL
