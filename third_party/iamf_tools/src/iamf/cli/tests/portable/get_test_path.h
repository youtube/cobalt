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
#ifndef CLI_TESTS_PORTABLE_GET_TEST_PATH_H_
#define CLI_TESTS_PORTABLE_GET_TEST_PATH_H_

#include <string>

#include "absl/strings/string_view.h"

/*!\brief Provides an interface for constructing test data paths.
 *
 * This file provides an interface for external clients which might have a
 * different way of constructing test data paths.
 */
namespace iamf_tools {

/*!\brief Gets the runfiles path for a given path.
 *
 * \param path Path to get the runfiles path for.
 * \return Runfiles path for the given path.
 */
std::string GetRunfilesPath(absl::string_view path);

/*!\brief Gets the runfiles path for a given path and filename.
 *
 * \param path Path to get the runfiles path for.
 * \param filename Filename to join with the path.
 * \return Runfiles path for the given path and filename.
 */
std::string GetRunfilesFile(absl::string_view path, std::string_view filename);

}  // namespace iamf_tools

#endif  // CLI_TESTS_PORTABLE_GET_TEST_PATH_H_
