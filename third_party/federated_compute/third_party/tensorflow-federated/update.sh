#!/bin/bash
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e
shopt -s extglob dotglob
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

TF_FEDERATED_REPO="https://chromium.googlesource.com/external/github.com/google-parfait/tensorflow-federated"
# Get the hash from README.chromium.
TF_FEDERATED_REV=$(sed -n 's/^Revision: \(.*\)/\1/p' "${SCRIPT_DIR}/README.chromium")

if [[ -z "${TF_FEDERATED_REV}" ]]; then
  echo "Error: Could not extract Revision from README.chromium"
  exit 1
fi

echo "Using repo ${TF_FEDERATED_REPO}"
echo "Using revision ${TF_FEDERATED_REV}"

TF_FEDERATED_FILES=(
  "tensorflow_federated/cc/core/impl/aggregation/core/tensor.proto"
  "tensorflow_federated/cc/core/impl/aggregation/base/base_name.cc"
  "tensorflow_federated/cc/core/impl/aggregation/base/base_name.h"
  "tensorflow_federated/cc/core/impl/aggregation/base/monitoring.cc"
  "tensorflow_federated/cc/core/impl/aggregation/base/monitoring.h"
  "tensorflow_federated/cc/core/impl/aggregation/base/move_to_lambda.h"
  "tensorflow_federated/cc/core/impl/aggregation/base/platform.cc"
  "tensorflow_federated/cc/core/impl/aggregation/base/platform.h"
  "tensorflow_federated/cc/core/impl/aggregation/core/agg_vector.h"
  "tensorflow_federated/cc/core/impl/aggregation/core/agg_vector_iterator.h"
  "tensorflow_federated/cc/core/impl/aggregation/core/datatype.cc"
  "tensorflow_federated/cc/core/impl/aggregation/core/datatype.h"
  "tensorflow_federated/cc/core/impl/aggregation/core/input_tensor_list.cc"
  "tensorflow_federated/cc/core/impl/aggregation/core/input_tensor_list.h"
  "tensorflow_federated/cc/core/impl/aggregation/core/mutable_vector_data.h"
  "tensorflow_federated/cc/core/impl/aggregation/core/scalar_data.h"
  "tensorflow_federated/cc/core/impl/aggregation/core/tensor.cc"
  "tensorflow_federated/cc/core/impl/aggregation/core/tensor.h"
  "tensorflow_federated/cc/core/impl/aggregation/core/tensor_data.cc"
  "tensorflow_federated/cc/core/impl/aggregation/core/tensor_data.h"
  "tensorflow_federated/cc/core/impl/aggregation/core/tensor_shape.cc"
  "tensorflow_federated/cc/core/impl/aggregation/core/tensor_shape.h"
  "tensorflow_federated/cc/core/impl/aggregation/core/tensor_slice_data.cc"
  "tensorflow_federated/cc/core/impl/aggregation/core/tensor_slice_data.h"
  "tensorflow_federated/cc/core/impl/aggregation/core/tensor_spec.cc"
  "tensorflow_federated/cc/core/impl/aggregation/core/tensor_spec.h"
  "tensorflow_federated/cc/core/impl/aggregation/core/vector_data.h"
  "tensorflow_federated/cc/core/impl/aggregation/protocol/checkpoint_builder.h"
  "tensorflow_federated/cc/core/impl/aggregation/protocol/checkpoint_header.h"
  "tensorflow_federated/cc/core/impl/aggregation/protocol/federated_compute_checkpoint_builder.cc"
  "tensorflow_federated/cc/core/impl/aggregation/protocol/federated_compute_checkpoint_builder.h"
  "LICENSE"
)

function check_out() {
  git init
  git remote add upstream ${TF_FEDERATED_REPO}
  git sparse-checkout init --no-cone
  git sparse-checkout set ${TF_FEDERATED_FILES[*]}
  git fetch --depth 1 upstream "${TF_FEDERATED_REV}"
  git checkout FETCH_HEAD 2>/dev/null
}

pushd "${SCRIPT_DIR}/"

mkdir .tmp-checkout
pushd .tmp-checkout

check_out

popd # .tmp-checkout

rm -rf src/
mkdir src/
cp -r .tmp-checkout/!(.git) src/
rm -rf .tmp-checkout/

popd # ${SCRIPT_DIR}/
