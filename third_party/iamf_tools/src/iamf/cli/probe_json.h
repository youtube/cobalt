/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#ifndef CLI_PROBE_JSON_H_
#define CLI_PROBE_JSON_H_

#include <string>

#include "iamf/cli/probe.h"

namespace iamf_tools {

/*!\brief Serializes a `ProbeReport` to a JSON object string.
 *
 * The output is a single pretty-printed JSON object mirroring the
 * `ProbeReport` structure (the same shape `probe_main` prints), terminated
 * by a newline. Optional report fields are omitted when unset. The
 * serializer lives alongside `ProbeReport` so the JSON shape evolves in the
 * same commit as the struct; embedders should use it instead of hand-rolling
 * their own.
 *
 * \param report Report to serialize.
 * \return JSON object string.
 */
std::string ProbeReportToJson(const ProbeReport& report);

}  // namespace iamf_tools

#endif  // CLI_PROBE_JSON_H_
