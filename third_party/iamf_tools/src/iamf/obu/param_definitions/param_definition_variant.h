/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#ifndef OBU_PARAM_DEFINITIONS_PARAM_DEFINITION_VARIANT_H_
#define OBU_PARAM_DEFINITIONS_PARAM_DEFINITION_VARIANT_H_

#include <variant>

#include "iamf/obu/param_definitions/cart16_param_definition.h"
#include "iamf/obu/param_definitions/cart8_param_definition.h"
#include "iamf/obu/param_definitions/demixing_param_definition.h"
#include "iamf/obu/param_definitions/dual_cart16_param_definition.h"
#include "iamf/obu/param_definitions/dual_cart8_param_definition.h"
#include "iamf/obu/param_definitions/dual_polar_param_definition.h"
#include "iamf/obu/param_definitions/extended_param_definition.h"
#include "iamf/obu/param_definitions/mix_gain_param_definition.h"
#include "iamf/obu/param_definitions/polar_param_definition.h"
#include "iamf/obu/param_definitions/recon_gain_param_definition.h"

namespace iamf_tools {

/*!\brief Variants of parameter definition currently supported. */
using ParamDefinitionVariant =
    std::variant<MixGainParamDefinition, DemixingParamDefinition,
                 ReconGainParamDefinition, PolarParamDefinition,
                 Cart8ParamDefinition, Cart16ParamDefinition,
                 DualPolarParamDefinition, DualCart8ParamDefinition,
                 DualCart16ParamDefinition, ExtendedParamDefinition>;

}  // namespace iamf_tools

#endif  // OBU_PARAM_DEFINITIONS_PARAM_DEFINITION_VARIANT_H_
