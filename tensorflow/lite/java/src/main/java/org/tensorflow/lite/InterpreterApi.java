/* Copyright 2017 The TensorFlow Authors. All Rights Reserved.

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

package org.tensorflow.lite;

import java.util.Map;
import org.checkerframework.checker.nullness.qual.NonNull;

/**
 * Interface to TensorFlow Lite model interpreter, excluding experimental methods.
 *
 * <p>An {@code InterpreterApi} instance encapsulates a pre-trained TensorFlow Lite model, in which
 * operations are executed for model inference.
 *
 * <p>For example, if a model takes only one input and returns only one output:
 *
 * <pre>{@code
 * try (InterpreterApi interpreter = new Interpreter(file_of_a_tensorflowlite_model)) {
 *   interpreter.run(input, output);
 * }
 * }</pre>
 *
 * <p>If a model takes multiple inputs or outputs:
 *
 * <pre>{@code
 * Object[] inputs = {input0, input1, ...};
 * Map<Integer, Object> map_of_indices_to_outputs = new HashMap<>();
 * FloatBuffer ith_output = FloatBuffer.allocateDirect(3 * 2 * 4);  // Float tensor, shape 3x2x4.
 * ith_output.order(ByteOrder.nativeOrder());
 * map_of_indices_to_outputs.put(i, ith_output);
 * try (InterpreterApi interpreter = new Interpreter(file_of_a_tensorflowlite_model)) {
 *   interpreter.runForMultipleInputsOutputs(inputs, map_of_indices_to_outputs);
 * }
 * }</pre>
 *
 * <p>If a model takes or produces string tensors:
 *
 * <pre>{@code
 * String[] input = {"foo", "bar"};  // Input tensor shape is [2].
 * String[] output = new String[3][2];  // Output tensor shape is [3, 2].
 * try (InterpreterApi interpreter = new Interpreter(file_of_a_tensorflowlite_model)) {
 *   interpreter.runForMultipleInputsOutputs(input, output);
 * }
 * }</pre>
 *
 * <p>Orders of inputs and outputs are determined when converting TensorFlow model to TensorFlowLite
 * model with Toco, as are the default shapes of the inputs.
 *
 * <p>When inputs are provided as (multi-dimensional) arrays, the corresponding input tensor(s) will
 * be implicitly resized according to that array's shape. When inputs are provided as {@link
 * java.nio.Buffer} types, no implicit resizing is done; the caller must ensure that the {@link
 * java.nio.Buffer} byte size either matches that of the corresponding tensor, or that they first
 * resize the tensor via {@link #resizeInput(int, int[])}. Tensor shape and type information can be
 * obtained via the {@link Tensor} class, available via {@link #getInputTensor(int)} and {@link
 * #getOutputTensor(int)}.
 *
 * <p><b>WARNING:</b>{@code InterpreterApi} instances are <b>not</b> thread-safe.
 *
 * <p><b>WARNING:</b>An {@code InterpreterApi} instance owns resources that <b>must</b> be
 * explicitly freed by invoking {@link #close()}
 *
 * <p>The TFLite library is built against NDK API 19. It may work for Android API levels below 19,
 * but is not guaranteed.
 */
public interface InterpreterApi extends AutoCloseable {

  /** An options class for controlling runtime interpreter behavior. */
  public static class Options {
    public Options() {}

    /**
     * Sets the number of threads to be used for ops that support multi-threading.
     *
     * <p>{@code numThreads} should be >= -1. Setting {@code numThreads} to 0 has the effect to
     * disable multithreading, which is equivalent to setting {@code numThreads} to 1. If
     * unspecified, or set to the value -1, the number of threads used will be
     * implementation-defined and platform-dependent.
     */
    public Options setNumThreads(int numThreads) {
      this.numThreads = numThreads;
      return this;
    }

    /** Sets whether to use NN API (if available) for op execution. Defaults to false (disabled). */
    public Options setUseNNAPI(boolean useNNAPI) {
      this.useNNAPI = useNNAPI;
      return this;
    }

    /**
     * Advanced: Set if the interpreter is able to be cancelled.
     *
     * @see Interpreter#setCancelled(boolean).
     */
    public Options setCancellable(boolean allow) {
      this.allowCancellation = allow;
      return this;
    }

    int numThreads = -1;
    Boolean useNNAPI;
    Boolean allowCancellation;
  }

  /**
   * Runs model inference if the model takes only one input, and provides only one output.
   *
   * <p>Warning: The API is more efficient if a {@code Buffer} (preferably direct, but not required)
   * is used as the input/output data type. Please consider using {@code Buffer} to feed and fetch
   * primitive data for better performance. The following concrete {@code Buffer} types are
   * supported:
   *
   * <ul>
   *   <li>{@code ByteBuffer} - compatible with any underlying primitive Tensor type.
   *   <li>{@code FloatBuffer} - compatible with float Tensors.
   *   <li>{@code IntBuffer} - compatible with int32 Tensors.
   *   <li>{@code LongBuffer} - compatible with int64 Tensors.
   * </ul>
   *
   * Note that boolean types are only supported as arrays, not {@code Buffer}s, or as scalar inputs.
   *
   * @param input an array or multidimensional array, or a {@code Buffer} of primitive types
   *     including int, float, long, and byte. {@code Buffer} is the preferred way to pass large
   *     input data for primitive types, whereas string types require using the (multi-dimensional)
   *     array input path. When a {@code Buffer} is used, its content should remain unchanged until
   *     model inference is done, and the caller must ensure that the {@code Buffer} is at the
   *     appropriate read position. A {@code null} value is allowed only if the caller is using a
   *     {@link Delegate} that allows buffer handle interop, and such a buffer has been bound to the
   *     input {@link Tensor}.
   * @param output a multidimensional array of output data, or a {@code Buffer} of primitive types
   *     including int, float, long, and byte. When a {@code Buffer} is used, the caller must ensure
   *     that it is set the appropriate write position. A null value is allowed, and is useful for
   *     certain cases, e.g., if the caller is using a {@link Delegate} that allows buffer handle
   *     interop, and such a buffer has been bound to the output {@link Tensor} (see also {@link
   *     Interpreter.Options#setAllowBufferHandleOutput(boolean)}), or if the graph has dynamically
   *     shaped outputs and the caller must query the output {@link Tensor} shape after inference
   *     has been invoked, fetching the data directly from the output tensor (via {@link
   *     Tensor#asReadOnlyBuffer()}).
   * @throws IllegalArgumentException if {@code input} is null or empty, or if an error occurs when
   *     running inference.
   * @throws IllegalArgumentException (EXPERIMENTAL, subject to change) if the inference is
   *     interrupted by {@code setCancelled(true)}.
   */
  public void run(Object input, Object output);

  /**
   * Runs model inference if the model takes multiple inputs, or returns multiple outputs.
   *
   * <p>Warning: The API is more efficient if {@code Buffer}s (preferably direct, but not required)
   * are used as the input/output data types. Please consider using {@code Buffer} to feed and fetch
   * primitive data for better performance. The following concrete {@code Buffer} types are
   * supported:
   *
   * <ul>
   *   <li>{@code ByteBuffer} - compatible with any underlying primitive Tensor type.
   *   <li>{@code FloatBuffer} - compatible with float Tensors.
   *   <li>{@code IntBuffer} - compatible with int32 Tensors.
   *   <li>{@code LongBuffer} - compatible with int64 Tensors.
   * </ul>
   *
   * Note that boolean types are only supported as arrays, not {@code Buffer}s, or as scalar inputs.
   *
   * <p>Note: {@code null} values for invididual elements of {@code inputs} and {@code outputs} is
   * allowed only if the caller is using a {@link Delegate} that allows buffer handle interop, and
   * such a buffer has been bound to the corresponding input or output {@link Tensor}(s).
   *
   * @param inputs an array of input data. The inputs should be in the same order as inputs of the
   *     model. Each input can be an array or multidimensional array, or a {@code Buffer} of
   *     primitive types including int, float, long, and byte. {@code Buffer} is the preferred way
   *     to pass large input data, whereas string types require using the (multi-dimensional) array
   *     input path. When {@code Buffer} is used, its content should remain unchanged until model
   *     inference is done, and the caller must ensure that the {@code Buffer} is at the appropriate
   *     read position.
   * @param outputs a map mapping output indices to multidimensional arrays of output data or {@code
   *     Buffer}s of primitive types including int, float, long, and byte. It only needs to keep
   *     entries for the outputs to be used. When a {@code Buffer} is used, the caller must ensure
   *     that it is set the appropriate write position. The map may be empty for cases where either
   *     buffer handles are used for output tensor data (see {@link
   *     Interpreter.Options#setAllowBufferHandleOutput(boolean)}), or cases where the outputs are
   *     dynamically shaped and the caller must query the output {@link Tensor} shape after
   *     inference has been invoked, fetching the data directly from the output tensor (via {@link
   *     Tensor#asReadOnlyBuffer()}).
   * @throws IllegalArgumentException if {@code inputs} is null or empty, if {@code outputs} is
   *     null, or if an error occurs when running inference.
   */
  public void runForMultipleInputsOutputs(
      @NonNull Object[] inputs, @NonNull Map<Integer, Object> outputs);

  /**
   * Explicitly updates allocations for all tensors, if necessary.
   *
   * <p>This will propagate shapes and memory allocations for dependent tensors using the input
   * tensor shape(s) as given.
   *
   * <p>Note: This call is *purely optional*. Tensor allocation will occur automatically during
   * execution if any input tensors have been resized. This call is most useful in determining the
   * shapes for any output tensors before executing the graph, e.g.,
   *
   * <pre>{@code
   * interpreter.resizeInput(0, new int[]{1, 4, 4, 3}));
   * interpreter.allocateTensors();
   * FloatBuffer input = FloatBuffer.allocate(interpreter.getInputTensor(0).numElements());
   * // Populate inputs...
   * FloatBuffer output = FloatBuffer.allocate(interpreter.getOutputTensor(0).numElements());
   * interpreter.run(input, output)
   * // Process outputs...
   * }</pre>
   *
   * <p>Note: Some graphs have dynamically shaped outputs, in which case the output shape may not
   * fully propagate until inference is executed.
   *
   * @throws IllegalStateException if the graph's tensors could not be successfully allocated.
   */
  public void allocateTensors();

  /**
   * Resizes idx-th input of the native model to the given dims.
   *
   * @throws IllegalArgumentException if {@code idx} is negative or is not smaller than the number
   *     of model inputs; or if error occurs when resizing the idx-th input.
   */
  public void resizeInput(int idx, @NonNull int[] dims);

  /**
   * Resizes idx-th input of the native model to the given dims.
   *
   * <p>When `strict` is True, only unknown dimensions can be resized. Unknown dimensions are
   * indicated as `-1` in the array returned by `Tensor.shapeSignature()`.
   *
   * @throws IllegalArgumentException if {@code idx} is negative or is not smaller than the number
   *     of model inputs; or if error occurs when resizing the idx-th input. Additionally, the error
   *     occurs when attempting to resize a tensor with fixed dimensions when `strict` is True.
   */
  public void resizeInput(int idx, @NonNull int[] dims, boolean strict);

  /** Gets the number of input tensors. */
  public int getInputTensorCount();

  /**
   * Gets index of an input given the op name of the input.
   *
   * @throws IllegalArgumentException if {@code opName} does not match any input in the model used
   *     to initialize the {@link Interpreter}.
   */
  public int getInputIndex(String opName);

  /**
   * Gets the Tensor associated with the provdied input index.
   *
   * @throws IllegalArgumentException if {@code inputIndex} is negative or is not smaller than the
   *     number of model inputs.
   */
  public Tensor getInputTensor(int inputIndex);

  /** Gets the number of output Tensors. */
  public int getOutputTensorCount();

  /**
   * Gets index of an output given the op name of the output.
   *
   * @throws IllegalArgumentException if {@code opName} does not match any output in the model used
   *     to initialize the {@link Interpreter}.
   */
  public int getOutputIndex(String opName);

  /**
   * Gets the Tensor associated with the provdied output index.
   *
   * <p>Note: Output tensor details (e.g., shape) may not be fully populated until after inference
   * is executed. If you need updated details *before* running inference (e.g., after resizing an
   * input tensor, which may invalidate output tensor shapes), use {@link #allocateTensors()} to
   * explicitly trigger allocation and shape propagation. Note that, for graphs with output shapes
   * that are dependent on input *values*, the output shape may not be fully determined until
   * running inference.
   *
   * @throws IllegalArgumentException if {@code outputIndex} is negative or is not smaller than the
   *     number of model outputs.
   */
  public Tensor getOutputTensor(int outputIndex);

  /**
   * Returns native inference timing.
   *
   * @throws IllegalArgumentException if the model is not initialized by the {@link Interpreter}.
   */
  public Long getLastNativeInferenceDurationNanoseconds();
}
