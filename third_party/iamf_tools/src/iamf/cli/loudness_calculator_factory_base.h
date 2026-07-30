/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#ifndef CLI_LOUDNESS_CALCULATOR_FACTORY_BASE_H_
#define CLI_LOUDNESS_CALCULATOR_FACTORY_BASE_H_

#include <cstdint>
#include <memory>

#include "iamf/cli/loudness_calculator_base.h"
#include "iamf/obu/mix_presentation.h"

namespace iamf_tools {

/*!\brief Abstract class to create loudness calculators.
 *
 * This class will be used when calculating the loudness of a mix presentation
 * layout. The mix presentation finalizer will take in a factory (or factories)
 * and use them to create a loudness calculator for each stream. By taking in a
 * factory the finalizer can be agnostic to the type of loudness calculator
 * being used which may depend on implementation details, or it may depend on
 * the specific layouts.
 */
class LoudnessCalculatorFactoryBase {
 public:
  /*!\brief Creates a loudness calculator.
   *
   * \param layout Layout to measure loudness on.
   * \param num_samples_per_frame Number of samples per frame for the calculator
   *        to process.
   * \param rendered_sample_rate Sample rate of the rendered audio.
   * \return Unique pointer to a loudness calculator.
   */
  virtual std::unique_ptr<LoudnessCalculatorBase> CreateLoudnessCalculator(
      const MixPresentationLayout& layout, uint32_t num_samples_per_frame,
      int32_t rendered_sample_rate) const = 0;

  /*!\brief Destructor. */
  virtual ~LoudnessCalculatorFactoryBase() = 0;
};

}  // namespace iamf_tools

#endif  // CLI_LOUDNESS_CALCULATOR_FACTORY_BASE_H_
