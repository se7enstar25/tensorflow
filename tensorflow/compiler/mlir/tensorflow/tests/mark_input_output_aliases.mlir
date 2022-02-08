// RUN: tf-opt %s -tf-device-mark-input-output-aliases | FileCheck %s

// The following tests check if the aliasing pairs are conservatively marked
// correctly. In the following tests tf_device.cluster_func has inputs
// coming from ReadVariableOp and outputs written to a resource using
// AssignVariableOp. If a pair of input-output (say input at index `a` and
// output at index `b`) read and write to the same resource, then those
// input-output pairs alias each other.
// This is shown on the device function by setting `tf.aliasing_output`
// attribute of argument `a` to `b`.

!tf_res_i32 = type tensor<*x!tf_type.resource<tensor<i32>>>
!tf_res_f32 = type tensor<*x!tf_type.resource<tensor<f32>>>

// CHECK-LABEL: func @simple_input_output_pairs
func @simple_input_output_pairs(%arg0: !tf_res_i32, %arg1: !tf_res_f32, %arg2: !tf_res_f32) {
  %0 = "tf.ReadVariableOp"(%arg0) : (!tf_res_i32) -> tensor<i32>
  %1 = "tf.ReadVariableOp"(%arg1) : (!tf_res_f32) -> tensor<f32>
  %2 = "tf.ReadVariableOp"(%arg2) : (!tf_res_f32) -> tensor<f32>
  %device_output:2 = "tf_device.cluster_func"(%0, %1, %2) {_tpu_replicate = "tpu", func = @device_func_0} : (tensor<i32>, tensor<f32>, tensor<f32>) -> (tensor<f32>, tensor<i32>)
  "tf.AssignVariableOp"(%arg1, %device_output#0) : (!tf_res_f32, tensor<f32>) -> ()
  "tf.AssignVariableOp"(%arg0, %device_output#1) : (!tf_res_i32, tensor<i32>) -> ()
  return
}

// CHECK-LABEL: func @device_func_0
// CHECK-SAME: [[ARG0:%.*]]: tensor<i32> {tf.aliasing_output = 1 : i64},
// CHECK-SAME: [[ARG1:%.*]]: tensor<f32> {tf.aliasing_output = 0 : i64},
// CHECK-SAME: [[ARG2:%.*]]: tensor<f32>
func @device_func_0(%arg0: tensor<i32>, %arg1: tensor<f32>, %arg2: tensor<f32>) -> (tensor<f32>, tensor<i32>) {
  return %arg1, %arg0 : tensor<f32>, tensor<i32>
}

// CHECK-LABEL: func @skip_outputs_with_multiple_use
func @skip_outputs_with_multiple_use(%arg0: !tf_res_i32) {
  %0 = "tf.ReadVariableOp"(%arg0) : (!tf_res_i32) -> tensor<i32>
  %device_output = "tf_device.cluster_func"(%0) {_tpu_replicate = "tpu", func = @device_func_1} : (tensor<i32>) -> tensor<i32>
  "tf.AssignVariableOp"(%arg0, %device_output) : (!tf_res_i32, tensor<i32>) -> ()
  "tf.SomeOp"(%device_output) : (tensor<i32>) -> ()
  return
}

// CHECK-LABEL: func @device_func_1
// CHECK-NOT: tf.aliasing_output
func @device_func_1(%arg0: tensor<i32>) -> tensor<i32> {
  return %arg0 : tensor<i32>
}

// CHECK-LABEL: func @skip_inputs_with_multiple_use
func @skip_inputs_with_multiple_use(%arg0: !tf_res_i32) {
  %0 = "tf.ReadVariableOp"(%arg0) : (!tf_res_i32) -> tensor<i32>
  %device_output = "tf_device.cluster_func"(%0) {_tpu_replicate = "tpu", func = @device_func_2} : (tensor<i32>) -> tensor<i32>
  "tf.AssignVariableOp"(%arg0, %device_output) : (!tf_res_i32, tensor<i32>) -> ()
  "tf.SomeOp"(%0) : (tensor<i32>) -> ()
  return
}

// CHECK-LABEL: func @device_func_2
// CHECK-NOT: tf.aliasing_output
func @device_func_2(%arg0: tensor<i32>) -> tensor<i32> {
  return %arg0 : tensor<i32>
}

// CHECK-LABEL: func @skip_multiple_assigns_to_resource
func @skip_multiple_assigns_to_resource(%arg0: !tf_res_f32, %arg1: !tf_res_f32) {
  %0 = "tf.ReadVariableOp"(%arg0) : (!tf_res_f32) -> tensor<f32>
  %1 = "tf.ReadVariableOp"(%arg1) : (!tf_res_f32) -> tensor<f32>
  %device_output:2 = "tf_device.cluster_func"(%0, %1) {_tpu_replicate = "tpu", func = @device_func_3} : (tensor<f32>, tensor<f32>) -> (tensor<f32>, tensor<f32>)
  "tf.AssignVariableOp"(%arg0, %device_output#0) : (!tf_res_f32, tensor<f32>) -> ()
  "tf.AssignVariableOp"(%arg0, %device_output#1) : (!tf_res_f32, tensor<f32>) -> ()
  return
}

// CHECK-LABEL: func @device_func_3
// CHECK-NOT: tf.aliasing_output
func @device_func_3(%arg0: tensor<f32>, %arg1: tensor<f32>) -> (tensor<f32>, tensor<f32>) {
  return %arg1, %arg0 : tensor<f32>, tensor<f32>
}

// CHECK-LABEL: func @skip_multiple_reads_of_resource
func @skip_multiple_reads_of_resource(%arg0: !tf_res_f32, %arg1: !tf_res_f32) {
  %0 = "tf.ReadVariableOp"(%arg0) : (!tf_res_f32) -> tensor<f32>
  %1 = "tf.ReadVariableOp"(%arg0) : (!tf_res_f32) -> tensor<f32>
  %device_output:2 = "tf_device.cluster_func"(%0, %1) {_tpu_replicate = "tpu", func = @device_func_4} : (tensor<f32>, tensor<f32>) -> (tensor<f32>, tensor<f32>)
  "tf.AssignVariableOp"(%arg0, %device_output#0) : (!tf_res_f32, tensor<f32>) -> ()
  "tf.AssignVariableOp"(%arg1, %device_output#1) : (!tf_res_f32, tensor<f32>) -> ()
  return
}

// CHECK-LABEL: func @device_func_4
// CHECK-NOT: tf.aliasing_output
func @device_func_4(%arg0: tensor<f32>, %arg1: tensor<f32>) -> (tensor<f32>, tensor<f32>) {
  return %arg1, %arg0 : tensor<f32>, tensor<f32>
}

// CHECK-LABEL: func @device_func_5
// CHECK-SAME: [[ARG0:%.*]]: tensor<f32> {tf.aliasing_output = 1 : i64}
// CHECK-SAME: [[ARG1:%.*]]: tensor<f32> {tf.aliasing_output = 0 : i64}
func @device_func_5(%arg0: tensor<f32>, %arg1: tensor<f32>) -> (tensor<f32>, tensor<f32>) {
  return %arg1, %arg0 : tensor<f32>, tensor<f32>
}

// CHECK-LABEL: func @device_func_6
// CHECK-SAME: [[ARG2:%.*]]: tensor<f32> {tf.aliasing_output = 1 : i64}
// CHECK-SAME: [[ARG3:%.*]]: tensor<f32> {tf.aliasing_output = 0 : i64}
func @device_func_6(%arg0: tensor<f32>, %arg1: tensor<f32>) -> (tensor<f32>, tensor<f32>) {
  return %arg1, %arg0 : tensor<f32>, tensor<f32>
}

// Test that aliasing works if `cluster_func` is wrapped in `parallel_execute`.
// CHECK-LABEL: func @parallel_execute
func @parallel_execute(%arg0: !tf_res_f32, %arg1: !tf_res_f32, %arg2: !tf_res_f32, %arg3: !tf_res_f32) {
  %0 = "tf.ReadVariableOp"(%arg0) : (!tf_res_f32) -> tensor<f32>
  %1 = "tf.ReadVariableOp"(%arg1) : (!tf_res_f32) -> tensor<f32>
  %2 = "tf.ReadVariableOp"(%arg2) : (!tf_res_f32) -> tensor<f32>
  %3 = "tf.ReadVariableOp"(%arg3) : (!tf_res_f32) -> tensor<f32>
  %4:4 = "tf_device.parallel_execute"() ({
    %5:2 = "tf_device.cluster_func"(%0, %2)  {_tpu_replicate = "tpu", func = @device_func_5} : (tensor<f32>, tensor<f32>) -> (tensor<f32>, tensor<f32>)
    tf_device.return %5#0, %5#1 : tensor<f32>, tensor<f32>
  }, {
    %6:2 = "tf_device.cluster_func"(%1, %3)  {_tpu_replicate = "tpu", func = @device_func_6} : (tensor<f32>, tensor<f32>) -> (tensor<f32>, tensor<f32>)
    tf_device.return %6#0, %6#1 : tensor<f32>, tensor<f32>
  }) : () -> (tensor<f32>, tensor<f32>, tensor<f32>, tensor<f32>)
  "tf.AssignVariableOp"(%arg2, %4#0) : (!tf_res_f32, tensor<f32>) -> ()
  "tf.AssignVariableOp"(%arg0, %4#1) : (!tf_res_f32, tensor<f32>) -> ()
  "tf.AssignVariableOp"(%arg3, %4#2) : (!tf_res_f32, tensor<f32>) -> ()
  "tf.AssignVariableOp"(%arg1, %4#3) : (!tf_res_f32, tensor<f32>) -> ()

  return
}
