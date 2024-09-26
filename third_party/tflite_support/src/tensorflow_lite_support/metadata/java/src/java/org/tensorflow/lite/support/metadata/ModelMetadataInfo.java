/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

package org.tensorflow.lite.support.metadata;

import static org.tensorflow.lite.support.metadata.Preconditions.checkArgument;
import static org.tensorflow.lite.support.metadata.Preconditions.checkNotNull;

import org.checkerframework.checker.nullness.qual.Nullable;
import org.tensorflow.lite.support.metadata.schema.ModelMetadata;
import org.tensorflow.lite.support.metadata.schema.SubGraphMetadata;
import org.tensorflow.lite.support.metadata.schema.TensorMetadata;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Extracts model metadata information out of TFLite metadata FlatBuffer. */
final class ModelMetadataInfo {
    /** The root handler for the model metadata. */
    private final ModelMetadata modelMetadata;

    /** Metadata array of input tensors. */
    private final List</* @Nullable */ TensorMetadata> inputsMetadata;

    /** Metadata array of output tensors. */
    private final List</* @Nullable */ TensorMetadata> outputsMetadata;

    /** The minimum parser version required to fully understand the metadata flatbuffer. */
    private final String /* @Nullable */ minVersion;

    /**
     * Creates a {@link ModelMetadataInfo} with the metadata FlatBuffer, {@code buffer}.
     *
     * @param buffer the TFLite metadata FlatBuffer
     * @throws NullPointerException if {@code buffer} is null
     * @throws IllegalArgumentException if {@code buffer} does not contain any subgraph metadata, or
     *     it does not contain the expected identifier
     */
    ModelMetadataInfo(ByteBuffer buffer) {
        assertTFLiteMetadata(buffer);

        modelMetadata = ModelMetadata.getRootAsModelMetadata(buffer);
        checkArgument(modelMetadata.subgraphMetadataLength() > 0,
                "The metadata flatbuffer does not contain any subgraph metadata.");

        inputsMetadata = getInputsMetadata(modelMetadata);
        outputsMetadata = getOutputsMetadata(modelMetadata);
        minVersion = modelMetadata.minParserVersion();
    }

    /** Gets the count of input tensors with metadata in the metadata FlatBuffer. */
    int getInputTensorCount() {
        return inputsMetadata.size();
    }

    /**
     * Gets the metadata for the input tensor specified by {@code inputIndex}.
     *
     * @param inputIndex The index of the desired intput tensor.
     * @throws IllegalArgumentException if the inputIndex specified is invalid.
     */
    @Nullable
    TensorMetadata getInputTensorMetadata(int inputIndex) {
        checkArgument(inputIndex >= 0 && inputIndex < inputsMetadata.size(),
                "The inputIndex specified is invalid.");
        return inputsMetadata.get(inputIndex);
    }

    /**
     * Gets the minimum parser version of the metadata. It can be {@code null} if the version is not
     * populated.
     */
    @Nullable
    String getMininumParserVersion() {
        return minVersion;
    }

    /** Gets the root handler for the model metadata. */
    ModelMetadata getModelMetadata() {
        return modelMetadata;
    }

    /** Gets the count of output tensors with metadata in the metadata FlatBuffer. */
    int getOutputTensorCount() {
        return outputsMetadata.size();
    }

    /**
     * Gets the metadata for the output tensor specified by {@code outputIndex}.
     *
     * @param outputIndex The index of the desired output tensor.
     * @throws IllegalArgumentException if the outputIndex specified is invalid.
     */
    @Nullable
    TensorMetadata getOutputTensorMetadata(int outputIndex) {
        checkArgument(outputIndex >= 0 && outputIndex < outputsMetadata.size(),
                "The outputIndex specified is invalid.");
        return outputsMetadata.get(outputIndex);
    }

    /**
     * Verifies if the buffer is a valid TFLite metadata flatbuffer.
     *
     * @param buffer the TFLite metadata flatbuffer
     * @throws NullPointerException if {@code buffer} is null.
     * @throws IllegalArgumentException if {@code buffer} does not contain the expected identifier
     */
    private static void assertTFLiteMetadata(ByteBuffer buffer) {
        checkNotNull(buffer, "Metadata flatbuffer cannot be null.");
        checkArgument(ModelMetadata.ModelMetadataBufferHasIdentifier(buffer),
                "The identifier of the metadata is invalid. The buffer may not be a valid TFLite metadata"
                        + " flatbuffer.");
    }

    /** Gets metadata for all input tensors. */
    private static List<TensorMetadata> getInputsMetadata(ModelMetadata modelMetadata) {
        SubGraphMetadata subgraphMetadata = modelMetadata.subgraphMetadata(0);
        int tensorNum = subgraphMetadata.inputTensorMetadataLength();
        ArrayList<TensorMetadata> inputsMetadata = new ArrayList<>(tensorNum);
        for (int i = 0; i < tensorNum; i++) {
            inputsMetadata.add(subgraphMetadata.inputTensorMetadata(i));
        }
        return Collections.unmodifiableList(inputsMetadata);
    }

    /** Gets metadata for all output tensors. */
    private static List<TensorMetadata> getOutputsMetadata(ModelMetadata modelMetadata) {
        SubGraphMetadata subgraphMetadata = modelMetadata.subgraphMetadata(0);
        int tensorNum = subgraphMetadata.outputTensorMetadataLength();
        ArrayList<TensorMetadata> outputsMetadata = new ArrayList<>(tensorNum);
        for (int i = 0; i < tensorNum; i++) {
            outputsMetadata.add(subgraphMetadata.outputTensorMetadata(i));
        }
        return Collections.unmodifiableList(outputsMetadata);
    }
}
