// RUN: tf-opt %s -split-input-file -verify-diagnostics -tf-launch-to-device-attribute=legacy-graph-export=false | FileCheck %s


// Tests single TensorFlow op is hoisted out and has the correct device assigned
// by parent `tf_device.launch`.
// CHECK-LABEL: func @single_op_launch
func.func @single_op_launch() {
  tf_executor.graph {
    %0:5 = tf_executor.island {
      %a = "tf.opA"() : () -> tensor<i1>
      %launch:2 = "tf_device.launch"() ({
        %b:2 = "tf.opB"(%a) : (tensor<i1>) -> (tensor<i32>, tensor<f32>)
        tf_device.return %b#1, %b#0 : tensor<f32>, tensor<i32>
      }) {device = "CPU:0"} : () -> (tensor<f32>, tensor<i32>)
      %c = "tf.opC"() : () -> tensor<i1>
      tf_executor.yield %a, %launch#0, %launch#1, %c : tensor<i1>, tensor<f32>, tensor<i32>, tensor<i1>
    }
    tf_executor.fetch
  }
  func.return
}

// CHECK-NOT: tf_executor.island
// CHECK:      %[[A:.*]], {{.*}} = tf_executor.island wraps "tf.opA"
// CHECK:      %[[B:.*]]:2, {{.*}} = tf_executor.island wraps "tf.opB"(%[[A]])
// CHECK-SAME: device = "CPU:0"
// CHECK:      %[[C:.*]], {{.*}} = tf_executor.island wraps "tf.opC"
// CHECK-NOT:  "tf_device.launch"
// CHECK-NOT:      tf_executor.yield


// Tests multiple TensorFlow ops are hoisted out and all have the correct device
// assigned by parent `tf_device.launch`.
// CHECK-LABEL: func @multi_op_launch
func.func @multi_op_launch() {
  tf_executor.graph {
    %0:5 = tf_executor.island {
      %a = "tf.opA"() : () -> tensor<i1>
      %launch:2 = "tf_device.launch"() ({
        %b = "tf.opB"(%a) : (tensor<i1>) -> tensor<i32>
        %c = "tf.opC"(%b) : (tensor<i32>) -> tensor<f32>
        tf_device.return %c, %b : tensor<f32>, tensor<i32>
      }) {device = "CPU:0"} : () -> (tensor<f32>, tensor<i32>)
      %d = "tf.opD"() : () -> tensor<i1>
      tf_executor.yield %a, %launch#0, %launch#1, %d : tensor<i1>, tensor<f32>, tensor<i32>, tensor<i1>
    }
    tf_executor.fetch
  }
  func.return
}

// CHECK-NOT: tf_executor.island
// CHECK:      %[[A:.*]], {{.*}} = tf_executor.island wraps "tf.opA"
// CHECK:      %[[B:.*]], {{.*}} = tf_executor.island wraps "tf.opB"(%[[A]])
// CHECK-SAME: device = "CPU:0"
// CHECK:      %[[C:.*]], {{.*}} = tf_executor.island wraps "tf.opC"(%[[B]])
// CHECK-SAME: device = "CPU:0"
// CHECK:      %[[D:.*]], {{.*}} = tf_executor.island wraps "tf.opD"
// CHECK-NOT:  "tf_device.launch"
// CHECK-NOT:      tf_executor.yield %[[A]], %[[C]], %[[B]], %[[D]]


// Tests empty device string attributes are overwritten.
// CHECK-LABEL: func @empty_device_op
func.func @empty_device_op() {
  tf_executor.graph {
    %0:3 = tf_executor.island {
      %launch:2 = "tf_device.launch"() ({
        %a:2 = "tf.opA"() {device = ""} : () -> (tensor<i32>, tensor<f32>)
        tf_device.return %a#1, %a#0 : tensor<f32>, tensor<i32>
      }) {device = "CPU:0"} : () -> (tensor<f32>, tensor<i32>)
      tf_executor.yield %launch#0, %launch#1: tensor<f32>, tensor<i32>
    }
    tf_executor.fetch
  }
  func.return
}

// CHECK-NOT: tf_executor.island
// CHECK:      [[A:%.+]]:2, {{.*}} = tf_executor.island wraps "tf.opA"
// CHECK-SAME: device = "CPU:0"
// CHECK-NOT:  tf_device.launch
// CHECK-NOT:      tf_executor.yield [[A]]#1, [[A]]#0


// -----


// Tests TensorFlow op with conflicting `device` attribute compared to parent
// `tf_device.launch`.
func.func @conflicting_device() {
  tf_executor.graph {
    %0 = tf_executor.island {
      // expected-error@+1 {{'tf_device.launch' op inner op has conflicting 'device' attribute, got 'GPU:0' but expected 'CPU:0'}}
      "tf_device.launch"() ({
        "tf.opA"() {device = "GPU:0"} : () -> ()
        tf_device.return
      }) {device = "CPU:0"} : () -> ()
      tf_executor.yield
    }
    tf_executor.fetch
  }
  func.return
}


// -----


// Tests TensorFlow op with bad `device` attribute already set.
func.func @bad_tf_device_attr() {
  tf_executor.graph {
    %0 = tf_executor.island {
      // expected-error@+1 {{'tf_device.launch' op inner op has bad 'device' attribute}}
      "tf_device.launch"() ({
        "tf.opA"() {device = 0 : i32} : () -> ()
        tf_device.return
      }) {device = "CPU:0"} : () -> ()
      tf_executor.yield
    }
    tf_executor.fetch
  }
  func.return
}
