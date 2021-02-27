// RUN: tf-opt %s -tf-shape-inference -verify-diagnostics | FileCheck %s

module attributes {tf.versions = {bad_consumers = [], min_consumer = 0 : i32, producer = 130 : i32}} {
  // CHECK-LABEL: func @main(%arg0: tensor<1xi32>, %arg1: tensor<1xi32>) -> tensor<1xi32>
  func @main(%arg0: tensor<1xi32>, %arg1: tensor<1xi32>) -> tensor<*xi32> {
    // CHECK: %[[RESULT:.*]] = "tf.AddV2"
    // CHECK-SAME: (tensor<1xi32>, tensor<1xi32>) -> tensor<1xi32>
    // CHECK: return %[[RESULT]] : tensor<1xi32>
    %0 = "tf.Cast"(%arg0) : (tensor<1xi32>) -> tensor<*xi32>
    %1 = "tf.Cast"(%arg1) : (tensor<1xi32>) -> tensor<*xi32>
    %2 = "tf.AddV2"(%0, %1) : (tensor<*xi32>, tensor<*xi32>) -> tensor<*xi32>
    return %2 : tensor<*xi32>
  }

  // CHECK-LABEL: func @simple_chain
  func @simple_chain(%arg0: tensor<1xf32>) -> tensor<*xf32> {
    // CHECK: %[[MUL:.*]] = "tf.Mul"{{.*}} (tensor<1xf32>, tensor<1xf32>) -> tensor<1xf32>
    // CHECK: %[[ADD:.*]] = "tf.Add"(%[[MUL]], %[[MUL]]) : (tensor<1xf32>, tensor<1xf32>) -> tensor<1xf32>
    // CHECK: return %[[ADD]] : tensor<1xf32>
    %0 = "tf.Mul"(%arg0, %arg0) : (tensor<1xf32>, tensor<1xf32>) -> tensor<*xf32>
    %1 = "tf.Add"(%0, %0) : (tensor<*xf32>, tensor<*xf32>) -> tensor<*xf32>
    return %1 : tensor<*xf32>
  }

  // CHECK-LABEL: func @simple_chain_with_broadcast
  func @simple_chain_with_broadcast(%arg0: tensor<1xf32>, %arg1: tensor<10xf32>) -> tensor<*xf32> {
    // CHECK: %[[MUL:.*]] = "tf.Mul"{{.*}} (tensor<1xf32>, tensor<10xf32>) -> tensor<10xf32>
    // CHECK: %[[ADD:.*]] = "tf.Add"(%[[MUL]], %[[MUL]]) : (tensor<10xf32>, tensor<10xf32>) -> tensor<10xf32>
    // CHECK: %[[CAST:.*]] = "tf.Cast"(%[[ADD]]) {{.*}} : (tensor<10xf32>) -> tensor<*xf32>
    // CHECK: %[[UNKNOWN:.*]] = addf %[[CAST]], %[[CAST]] : tensor<*xf32>
    // CHECK: return %[[UNKNOWN]] : tensor<*xf32>
    %0 = "tf.Mul"(%arg0, %arg1) : (tensor<1xf32>, tensor<10xf32>) -> tensor<*xf32>
    %1 = "tf.Add"(%0, %0) : (tensor<*xf32>, tensor<*xf32>) -> tensor<*xf32>
    %2 = addf %1, %1 : tensor<*xf32>
    return %2 : tensor<*xf32>
  }

  // CHECK-LABEL: func @unknown_op
  func @unknown_op(%arg0: tensor<1xf32>) -> tensor<*xf32> {
    // CHECK: %[[MUL:.*]] = "tf.Mul"{{.*}} (tensor<1xf32>, tensor<1xf32>) -> tensor<1xf32>
    // CHECK: %[[UNKNOWN:.*]] = "tf.Unknown"(%[[MUL]], %[[MUL]]) : (tensor<1xf32>, tensor<1xf32>) -> tensor<*xf32>
    // CHECK: return %[[UNKNOWN]] : tensor<*xf32>
    %0 = "tf.Mul"(%arg0, %arg0) : (tensor<1xf32>, tensor<1xf32>) -> tensor<*xf32>
    %1 = "tf.Unknown"(%0, %0) : (tensor<*xf32>, tensor<*xf32>) -> tensor<*xf32>
    return %1 : tensor<*xf32>
  }

  // CHECK-LABEL: func @multiple_blocks_one_return(%arg0: tensor<?xf32>) -> tensor<?xf32>
  func @multiple_blocks_one_return(%arg0: tensor<?xf32>) -> tensor<*xf32> {
    br ^bb1
  ^bb1:
  // CHECK: %[[IDENTITY:.*]] = "tf.Identity"(%arg0) : (tensor<?xf32>) -> tensor<?xf32>
  // CHECK: return %[[IDENTITY]] : tensor<?xf32>
    %ret = "tf.Identity"(%arg0) : (tensor<?xf32>) -> tensor<*xf32>
    return %ret : tensor<*xf32>
  }


  // Tests the case where an inference opportunity relies on folding.

  // CHECK-LABEL: func @simple_folding
  func @simple_folding(%arg0: tensor<1x1x1x1xi32>, %arg1: tensor<1x1x1x1xf32>) -> tensor<?x?x?x?xf32> {
    // CHECK: %[[SHAPE:.*]] = "tf.Shape"
    // CHECK: %[[CONV:.*]] = "tf.Conv2DBackpropInput"(%[[SHAPE]]
    // CHECK-SAME: (tensor<4xi32>, tensor<1x1x1x1xf32>, tensor<1x1x1x1xf32>) -> tensor<1x1x1x1xf32>
    // CHECK: return %[[CONV]] : tensor<1x1x1x1xf32>
    %0 = "tf.Shape"(%arg0) : (tensor<1x1x1x1xi32>) -> tensor<4xi32>
    %1 = "tf.Conv2DBackpropInput"(%0, %arg1, %arg1) {
      padding = "VALID", strides = [1, 1, 1, 1]
    } : (tensor<4xi32>, tensor<1x1x1x1xf32>, tensor<1x1x1x1xf32>) -> tensor<?x?x?x?xf32>
    return %1 : tensor<?x?x?x?xf32>
  }

  // Tests where tf.Const's value needs to be refined.

  func @const_refine() -> tensor<*xi32> {
    %0 = "tf.Const"() {value = dense<[3, 2]> : tensor<2xi32>} : () -> tensor<*xi32>
    // CHECK: "tf.Const"
    // CHECK-SAME: -> tensor<2xi32>
    return %0 : tensor<*xi32>
  }

  // Tests the case where an op's shape function returns non-fully-defined shapes.

  // CHECK-LABEL: func @op_non_fully_defined_shape_fn
  func @op_non_fully_defined_shape_fn(%arg0: tensor<*xi32>, %arg1: tensor<0xi32>) -> tensor<?xi32> {
    // CHECK: tf.BroadcastGradientArgs
    // CHECK-SAME: (tensor<*xi32>, tensor<0xi32>) -> (tensor<?xi32>, tensor<?xi32>)
    %2:2 = "tf.BroadcastGradientArgs"(%arg0, %arg1) {T = "tfdtype$DT_INT32", name = "BroadcastGradientArgs"} : (tensor<*xi32>, tensor<0xi32>) -> (tensor<?xi32>, tensor<?xi32>)
    return %2#0 : tensor<?xi32>
  }

  // CHECK-LABEL: func @shape_from_const_input
  func @shape_from_const_input(%arg0: tensor<3x3x32x64xf32>, %arg1: tensor<200x24x24x64xf32>) -> tensor<?x?x?x?xf32> {
    %0 = "tf.Const"() {value = dense<[200, 26, 26, 32]> : tensor<4xi32>} : () -> tensor<4xi32>
    // CHECK: tf.Conv2DBackpropInput
    // CHECK-SAME: (tensor<4xi32>, tensor<3x3x32x64xf32>, tensor<200x24x24x64xf32>) -> tensor<200x26x26x32xf32>
    %1 = "tf.Conv2DBackpropInput"(%0, %arg0, %arg1) {data_format = "NHWC", dilations = [1, 1, 1, 1], explicit_paddings = [], padding = "VALID", strides = [1, 1, 1, 1], use_cudnn_on_gpu = true} : (tensor<4xi32>, tensor<3x3x32x64xf32>, tensor<200x24x24x64xf32>) -> tensor<?x?x?x?xf32>
    return %1 : tensor<?x?x?x?xf32>
  }

  // CHECK-LABEL: func @shape_from_if_to_branch_functions_to_results
  // CHECK-SAME: (%arg0: tensor<i1>, %arg1: tensor<1x2x3xf32>) -> tensor<1x2x3xf32>
  func @shape_from_if_to_branch_functions_to_results(%arg0: tensor<i1>, %arg1: tensor<1x2x3xf32>) -> tensor<*xf32> {
    %0 = "tf.If"(%arg0, %arg1) {Tcond = i1, Tin = ["tfdtype$DT_FLOAT"], Tout = ["tfdtype$DT_FLOAT"], else_branch = @if_else_branch, is_stateless = true, name = "if", then_branch = @if_then_branch} : (tensor<i1>, tensor<1x2x3xf32>) -> tensor<*xf32>
    return %0 : tensor<*xf32>
  }

  // CHECK-LABEL: func @if_then_branch
  // CHECK-SAME: (%arg0: tensor<1x2x3xf32>) -> tensor<1x2x3xf32>
  func @if_then_branch(%arg0: tensor<*xf32>) -> tensor<*xf32> {
    // CHECK: return
    // CHECK-SAME: tensor<1x2x3xf32>
    return %arg0 : tensor<*xf32>
  }

  // CHECK-LABEL: func @if_else_branch
  // CHECK-SAME: (%arg0: tensor<1x2x3xf32>) -> tensor<1x2x3xf32>
  func @if_else_branch(%arg0: tensor<*xf32>) -> tensor<*xf32> {
    // CHECK: "tf.Identity"(%arg0) : (tensor<1x2x3xf32>) -> tensor<1x2x3xf32>
    %0 = "tf.Identity"(%arg0) : (tensor<*xf32>) -> (tensor<*xf32>)
    // CHECK: return
    // CHECK-SAME: tensor<1x2x3xf32>
    return %0 : tensor<*xf32>
  }

  // Verify shape propagation from function arg -> if region body -> if region output -> function return type
  // CHECK-LABEL: shape_from_if_to_region_bodies_to_output
  // CHECK-SAME: -> tensor<1x2x3xf32>
  func @shape_from_if_to_region_bodies_to_output(%arg0: tensor<i1>, %arg1: tensor<1x2x3xf32>) -> tensor<*xf32> {
    %unshaped = "tf.Cast"(%arg1) : (tensor<1x2x3xf32>) -> tensor<*xf32>
    %0 = "tf.IfRegion"(%arg0) ({
      // CHECK: "tf.Add"{{.+}}(tensor<1x2x3xf32>, tensor<1x2x3xf32>) -> tensor<1x2x3xf32>
      // CHECK: "tf.Yield"{{.+}}(tensor<1x2x3xf32>) -> ()
      %1 = "tf.Add"(%unshaped, %unshaped) : (tensor<*xf32>,  tensor<*xf32>) -> tensor<*xf32>
      "tf.Yield"(%1) : (tensor<*xf32>) -> ()
     }, {
      // CHECK: "tf.Sub"{{.+}}(tensor<1x2x3xf32>, tensor<1x2x3xf32>) -> tensor<1x2x3xf32>
      // CHECK: "tf.Yield"{{.+}}(tensor<1x2x3xf32>) -> ()
      %2 = "tf.Sub"(%unshaped, %unshaped) : (tensor<*xf32>,  tensor<*xf32>) -> tensor<*xf32>
      "tf.Yield"(%2) : (tensor<*xf32>) -> ()
      // CHECK: {is_stateless = true} : (tensor<i1>) -> tensor<1x2x3xf32>
     }) {is_stateless = true} : (tensor<i1>) -> tensor<*xf32>
    // CHECK: return {{.*}} :  tensor<1x2x3xf32>
    return %0 : tensor<*xf32>
  }

  // CHECK-LABEL: func @shape_from_while_to_cond_body_functions
  func @shape_from_while_to_cond_body_functions(%arg0: tensor<4xf32>, %arg1: tensor<!tf.resource<tensor<4xf32>>>, %arg2: tensor<!tf.resource<tensor<*xf32>>>) -> tensor<4xf32> {
    // CHECK: "tf.While"
    // CHECK-SAME: (tensor<4xf32>, tensor<!tf.resource<tensor<4xf32>>>, tensor<!tf.resource<tensor<*xf32>>>) -> (tensor<4xf32>, tensor<!tf.resource<tensor<4xf32>>>, tensor<!tf.resource<tensor<*xf32>>>)
    %0:3 = "tf.While"(%arg0, %arg1, %arg2) {cond = @while_cond_func, body = @while_body_func, is_stateless = true} : (tensor<4xf32>, tensor<!tf.resource<tensor<4xf32>>>, tensor<!tf.resource<tensor<*xf32>>>) -> (tensor<4xf32>, tensor<*x!tf.resource>, tensor<!tf.resource<tensor<*xf32>>>)
    return %0#0 : tensor<4xf32>
  }

  // CHECK-LABEL: func @while_cond_func
  // CHECK-SAME: (%arg0: tensor<4xf32>, %arg1: tensor<!tf.resource<tensor<4xf32>>>, %arg2: tensor<!tf.resource<tensor<*xf32>>>) -> tensor<i1>
  func @while_cond_func(%arg0: tensor<*xf32>, %arg1: tensor<*x!tf.resource>, %arg2: tensor<!tf.resource<tensor<*xf32>>>) -> tensor<i1> {
    %0 = "tf.Const"() {value = dense<[1.000000e-04,2.000000e-04,3.000000e-04,4.000000e-04]> : tensor<4xf32>} : () -> tensor<4xf32>
    %1 = "tf.Const"() {value = dense<0> : tensor<i32>} : () -> tensor<i32>
    // CHECK: tf.Equal
    // CHECK-SAME: (tensor<4xf32>, tensor<4xf32>) -> tensor<*xi1>
    // TODO(ycao): Investigate why result type of tf.Equal is not inferred.
    %2 = "tf.Equal"(%0, %arg0) : (tensor<4xf32>, tensor<*xf32>) -> tensor<*xi1>
    %3 = "tf.Any"(%2, %1) : (tensor<*xi1>, tensor<i32>) -> (tensor<i1>)
    return %3 : tensor<i1>
  }

  // CHECK-LABEL: func @while_body_func
  func @while_body_func(%arg0: tensor<*xf32>, %arg1: tensor<*x!tf.resource>, %arg2: tensor<!tf.resource<tensor<*xf32>>>) -> (tensor<*xf32>, tensor<*x!tf.resource>, tensor<!tf.resource<tensor<*xf32>>>) {
    %0 = "tf.Const"() {value = dense<1.000000e-04> : tensor<f32>} : () -> tensor<f32>
    // CHECK: tf.AddV2
    // CHECK-SAME: (tensor<4xf32>, tensor<f32>) -> tensor<4xf32>
    %1 = "tf.AddV2"(%arg0, %0) : (tensor<*xf32>, tensor<f32>) -> tensor<*xf32>
    // CHECK: "tf.Identity"
    // CHECK-SAME: (tensor<!tf.resource<tensor<4xf32>>>) -> tensor<!tf.resource<tensor<4xf32>>>
    %2 = "tf.Identity"(%arg1) : (tensor<*x!tf.resource>) -> tensor<*x!tf.resource>
    // CHECK: "tf.TPUReplicatedInput"
    // CHECK-SAME: (tensor<!tf.resource<tensor<4xf32>>>) -> tensor<!tf.resource<tensor<4xf32>>>
    %ri = "tf.TPUReplicatedInput"(%2) : (tensor<*x!tf.resource>) -> tensor<*x!tf.resource>
    // CHECK: "tf.ReadVariableOp"
    // CHECK-SAME: (tensor<!tf.resource<tensor<4xf32>>>) -> tensor<4xf32>
    %read = "tf.ReadVariableOp"(%ri) : (tensor<*x!tf.resource>) -> tensor<*xf32>
    // CHECK: "tf.ReadVariableOp"
    // CHECK-SAME: (tensor<!tf.resource<tensor<*xf32>>>) -> tensor<*xf32>
    %read1 = "tf.ReadVariableOp"(%arg2) : (tensor<!tf.resource<tensor<*xf32>>>) -> tensor<*xf32>
    // CHECK: return
    // CHECK-SAME: tensor<4xf32>
    // CHECK-SAME: tensor<!tf.resource<tensor<4xf32>>>
    return %1, %arg1, %arg2 : tensor<*xf32>, tensor<*x!tf.resource>, tensor<!tf.resource<tensor<*xf32>>>
  }

  // Verify shape propagation from function arg -> while region cond/body -> while region output -> function return type
  // CHECK-LABEL: func @shape_from_while_operands_to_cond_body_to_while_results
  // CHECK-SAME: -> tensor<1x2x3xf32>
  func @shape_from_while_operands_to_cond_body_to_while_results(%arg0: tensor<i32>, %arg1: tensor<1x2x3xf32>) ->  tensor<*xf32> {
    %unshaped = "tf.Cast"(%arg1) : (tensor<1x2x3xf32>) -> tensor<*xf32>
    // CHECK: "tf.WhileRegion"
    %0:2 = "tf.WhileRegion"(%arg0, %unshaped) ({
       // CHECK: {{.*}}({{.+}}: tensor<i32>, {{.+}}: tensor<1x2x3xf32>):
       ^bb0(%carg0: tensor<i32>, %carg1: tensor<*xf32>):
         %limit = constant dense<5> : tensor<i32>
         %cond = "tf.NotEqual"(%carg0, %limit) : (tensor<i32>, tensor<i32>) -> tensor<i1>
         "tf.Yield"(%cond) : (tensor<i1>) -> ()
      }, {
       // CHECK: {{.*}}({{.+}}: tensor<i32>, {{.+}}: tensor<1x2x3xf32>):
       ^bb0(%barg0: tensor<i32>, %barg1: tensor<*xf32>):
        %one = constant dense<1> : tensor<i32>
        %sub = "tf.Sub"(%barg0, %one) : (tensor<i32>, tensor<i32>) -> tensor<i32>
        // CHECK: "tf.Neg"({{.+}}) : (tensor<1x2x3xf32>) -> tensor<1x2x3xf32>
        %neg = "tf.Neg"(%barg1) : (tensor<*xf32>) -> tensor<*xf32>
        // CHECK: "tf.Yield"{{.+}}, {{.+}}) : (tensor<i32>, tensor<1x2x3xf32>) -> ()
        "tf.Yield"(%sub, %neg) : (tensor<i32>, tensor<*xf32>) -> ()
    // CHECK: {is_stateless = true} : (tensor<i32>, tensor<1x2x3xf32>) -> (tensor<i32>, tensor<1x2x3xf32>)
    }) {is_stateless = true} : (tensor<i32>, tensor<*xf32>) -> (tensor<i32>, tensor<*xf32>)
    // CHECK: return {{.+}}#1 : tensor<1x2x3xf32>
    return %0#1 : tensor<*xf32>
  }

  // CHECK-LABEL: func @shape_from_case_to_branch_functions(
  // CHECK-SAME:    %[[ARG_0:.*]]: tensor<i32>,
  // CHECK-SAME:    %[[ARG_1:.*]]: tensor<!tf.resource<tensor<1x2x3xf32>>>
  func @shape_from_case_to_branch_functions(%arg0: tensor<i32>, %arg1: tensor<!tf.resource<tensor<1x2x3xf32>>>) -> tensor<1x2x3xf32> {
    // CHECK: %[[CASE:.*]] = "tf.Case"(%[[ARG_0]], %[[ARG_1]])
    %0 = "tf.Case"(%arg0, %arg1) {branches = [@branch_0, @branch_1], is_stateless = false} : (tensor<i32>, tensor<!tf.resource<tensor<1x2x3xf32>>>) -> tensor<1x2x3xf32>
    // CHECK:           return %[[CASE]] : tensor<1x2x3xf32>
    return %0 : tensor<1x2x3xf32>
  }
  // CHECK-LABEL: func @branch_0
  // CHECK-SAME:    %[[ARG_0:.*]]: tensor<!tf.resource<tensor<1x2x3xf32>>>) -> tensor<1x2x3xf32>
  func @branch_0(%arg0: tensor<!tf.resource>) -> tensor<*xf32> {
    // CHECK: %[[READ:.*]] = "tf.ReadVariableOp"(%[[ARG_0]]) : (tensor<!tf.resource<tensor<1x2x3xf32>>>) -> tensor<1x2x3xf32>
    %0 = "tf.ReadVariableOp"(%arg0) : (tensor<!tf.resource>) -> (tensor<*xf32>)
    // CHECK: return %[[READ]] : tensor<1x2x3xf32>
  return %0 : tensor<*xf32>
  }
  // CHECK-LABEL: func @branch_1
  // CHECK-SAME:    %[[ARG_0:.*]]: tensor<!tf.resource<tensor<1x2x3xf32>>>) -> tensor<1x2x3xf32>
  func @branch_1(%arg0: tensor<!tf.resource>) -> tensor<*xf32> {
    // CHECK: %[[READ:.*]] = "tf.ReadVariableOp"(%[[ARG_0]]) : (tensor<!tf.resource<tensor<1x2x3xf32>>>) -> tensor<1x2x3xf32>
    %0 = "tf.ReadVariableOp"(%arg0) : (tensor<!tf.resource>) -> (tensor<*xf32>)
    // CHECK: return %[[READ]] : tensor<1x2x3xf32>
    return %0 : tensor<*xf32>
  }

  func @partitioned_call(%arg0: tensor<i32>) -> tensor<*xi32> {
    %0 = "tf.PartitionedCall"(%arg0) {config = "", config_proto = "", executor_type = "", f = @partitioned_call_func} : (tensor<i32>) -> (tensor<*xi32>)
    return %0 : tensor<*xi32>
  }

  // CHECK-LABEL: func @partitioned_call_func
  // CHECK-SAME: (%arg0: tensor<i32>) -> tensor<i32>
  func @partitioned_call_func(%arg0: tensor<*xi32>) -> tensor<*xi32> {
    // CHECK: return
    // CHECK-SAME: tensor<i32>
    return %arg0 : tensor<*xi32>
  }

  // CHECK-LABEL: func @invalid_function_reused_by_control_flows
  func @invalid_function_reused_by_control_flows(%arg0: tensor<i1>, %arg1: tensor<1x2x3xf32>, %arg2: tensor<3xf32>) -> (tensor<1x2x3xf32>, tensor<3xf32>) {
    // expected-warning @+1 {{unable to refine shape}}
    %0 = "tf.If"(%arg0, %arg1) {Tcond = i1, Tin = ["tfdtype$DT_FLOAT"], Tout = ["tfdtype$DT_FLOAT"], _xla_propagate_compile_time_consts = true, device = "", else_branch = @reused_if_else_branch, is_stateless = true, name = "if", then_branch = @reused_if_then_branch} : (tensor<i1>, tensor<1x2x3xf32>) -> tensor<1x2x3xf32>
    // expected-warning @+1 {{unable to refine shape}}
    %1 = "tf.If"(%arg0, %arg2) {Tcond = i1, Tin = ["tfdtype$DT_FLOAT"], Tout = ["tfdtype$DT_FLOAT"], _xla_propagate_compile_time_consts = true, device = "", else_branch = @reused_if_else_branch, is_stateless = true, name = "if", then_branch = @reused_if_then_branch} : (tensor<i1>, tensor<3xf32>) -> tensor<3xf32>
    return %0, %1 : tensor<1x2x3xf32>, tensor<3xf32>
  }

  // CHECK-LABEL: func @reused_if_then_branch
  // CHECK-SAME: (%arg0: tensor<*xf32>) -> tensor<*xf32>
  // expected-warning @+1 {{expected control flow function @reused_if_then_branch to have exactly 1 use}}
  func @reused_if_then_branch(%arg0: tensor<*xf32>) -> tensor<*xf32> {
    // CHECK: return
    // CHECK-SAME: tensor<*xf32>
    return %arg0 : tensor<*xf32>
  }

  // CHECK-LABEL: func @reused_if_else_branch
  // CHECK-SAME: (%arg0: tensor<*xf32>) -> tensor<*xf32>
  // expected-warning @+1 {{expected control flow function @reused_if_else_branch to have exactly 1 use}}
  func @reused_if_else_branch(%arg0: tensor<*xf32>) -> tensor<*xf32> {
    // CHECK: "tf.Identity"(%arg0) : (tensor<*xf32>) -> tensor<*xf32>
    %0 = "tf.Identity"(%arg0) : (tensor<*xf32>) -> (tensor<*xf32>)
    // CHECK: return
    // CHECK-SAME: tensor<*xf32>
    return %0 : tensor<*xf32>
  }

  // CHECK-LABEL: func @with_graph_and_islands
  // CHECK-SAME: %[[ARG_0:.*]]: tensor<!tf.resource<tensor<4xf32>>>
  // CHECK-SAME: -> tensor<4xf32>
  func @with_graph_and_islands(%arg0: tensor<!tf.resource<tensor<4xf32>>>) -> tensor<*xf32> {
    %graph = tf_executor.graph {
      %island:2 = tf_executor.island {
        // CHECK: %[[ID_0:.*]] = "tf.IdentityN"(%[[ARG_0]])
        %id0 = "tf.IdentityN"(%arg0)
          : (tensor<!tf.resource<tensor<4xf32>>>) -> tensor<!tf.resource<tensor<4xf32>>>
        // CHECK-NEXT: %[[READ_0:.*]] = "tf.ReadVariableOp"(%[[ID_0]])
        // CHECK-SAME: (tensor<!tf.resource<tensor<4xf32>>>) -> tensor<4xf32>
        %read = "tf.ReadVariableOp"(%id0) : (tensor<!tf.resource<tensor<4xf32>>>) -> tensor<*xf32>
        // CHECK-NEXT: tf_executor.yield %[[READ_0]] : tensor<4xf32>
        tf_executor.yield %read : tensor<*xf32>
      }
      // CHECK: tf_executor.fetch
      // CHECK-SAME: tensor<4xf32>
      tf_executor.fetch %island#0 : tensor<*xf32>
    }
    // CHECK: return
    // CHECK-SAME: tensor<4xf32>
    return %graph : tensor<*xf32>
  }

  // CHECK-LABEL: func @next_iteration_user
  func @next_iteration_user(%arg0: tensor<32x?x256x4xf32>) -> tensor<?x?x?xf32> {
    %0 = tf_executor.graph {
      // CHECK: tf_executor.NextIteration.Source
      // CHECK-SAME: : tensor<32x?x4xf32>
      %1:3 = tf_executor.NextIteration.Source : tensor<?x?x?xf32>
      %out, %c_out = tf_executor.island {
        %dims = "tf.Const"() {value = dense<[32, -1, 4]> : tensor<3xi32>} : () -> tensor<3xi32>
        // CHECK: "tf.Reshape"
        // CHECK-SAME: -> tensor<32x?x4xf32>
        %reshape = "tf.Reshape"(%arg0, %dims) : (tensor<32x?x256x4xf32>, tensor<3xi32>) -> tensor<?x?x?xf32>
        // CHECK: tf_executor.yield
        // CHECK-SAME: : tensor<32x?x4xf32>
        tf_executor.yield %reshape : tensor<?x?x?xf32>
      }
      // CHECK: tf_executor.NextIteration.Sink
      // CHECK-SAME: : tensor<32x?x4xf32>
      tf_executor.NextIteration.Sink[%1#1] %out : tensor<?x?x?xf32>
      tf_executor.fetch %1#0 : tensor<?x?x?xf32>
    }
    return %0 : tensor<?x?x?xf32>
  }

  // Check that supported tf_executor ops can receive data from ops on which
  // shape inference has inferred the result types, without throwing any errors.
  // CHECK-LABEL: func @supported_tf_executor_users
  func @supported_tf_executor_users(%arg0: tensor<32x?x256x4xf32>, %arg1: tensor<?x?x?xf32>, %arg2: tensor<i1>, %arg3: tensor<i32>) -> tensor<?x?x?xf32> {
    %0 = tf_executor.graph {
      %island:3 = tf_executor.island {
        %dims = "tf.Const"() {value = dense<[32, -1, 4]> : tensor<3xi32>} : () -> tensor<3xi32>
        %reshape = "tf.Reshape"(%arg0, %dims) : (tensor<32x?x256x4xf32>, tensor<3xi32>) -> tensor<?x?x?xf32>
        %cast = "tf.Cast"(%arg2) : (tensor<i1>) -> tensor<*xi1>
        tf_executor.yield %reshape, %cast : tensor<?x?x?xf32>, tensor<*xi1>
      }
      // CHECK: tf_executor.Merge
      // CHECK-SAME: : (tensor<32x?x4xf32>, tensor<?x?x?xf32>) ->
      // CHECK: tf_executor.Switch
      // CHECK-SAME: : (tensor<32x?x4xf32>, tensor<i1>) ->
      // CHECK: tf_executor._SwitchN
      // CHECK-SAME: : tensor<?x?x?xf32>
      // CHECK: tf_executor.Enter
      // CHECK-SAME: : (tensor<32x?x4xf32>) ->
      // CHECK: tf_executor.Exit
      // CHECK-SAME: : tensor<?x?x?xf32>
      // CHECK: tf_executor.LoopCond
      // CHECK-SAME: tensor<i1>
      %merge:3 = "tf_executor.Merge"(%island#0, %arg1) : (tensor<?x?x?xf32>, tensor<?x?x?xf32>) -> (tensor<?x?x?xf32>, tensor<i32>, !tf_executor.control)
      %switch:3 = "tf_executor.Switch"(%island#0, %arg2) : (tensor<?x?x?xf32>, tensor<i1>) -> (tensor<?x?x?xf32>, tensor<?x?x?xf32>, !tf_executor.control)
      %switchn:3 = "tf_executor._SwitchN"(%island#0, %arg3) {num_outs = 2} : (tensor<?x?x?xf32>, tensor<i32>) -> (tensor<?x?x?xf32>, tensor<?x?x?xf32>, !tf_executor.control)
      %enter:2 = "tf_executor.Enter"(%island#0) { frame_name = "frame"} : (tensor<?x?x?xf32>) -> (tensor<?x?x?xf32>, !tf_executor.control)
      %exit:2 = "tf_executor.Exit"(%island#0) : (tensor<?x?x?xf32>) -> (tensor<?x?x?xf32>, !tf_executor.control)
      %loop_cond:2 = "tf_executor.LoopCond" (%island#1) : (tensor<*xi1>) -> (tensor<*xi1>, !tf_executor.control)
      tf_executor.fetch %enter#0 : tensor<?x?x?xf32>
    }
    return %0 : tensor<?x?x?xf32>
  }

  // Tests that tensor.cast result shapes are refined.
  // CHECK-LABEL: func @tensor_cast_refine
  func @tensor_cast_refine(%arg0: tensor<4xi32>) -> (tensor<*xi32>) {
    // CHECK: tensor.cast
    // CHECK-SAME: tensor<4xi32> to tensor<4xi32>
    %0 = tensor.cast %arg0 : tensor<4xi32> to tensor<*xi32>
    return %0 : tensor<*xi32>
  }

  // CHECK-LABEL: func @while_variant
  // CHECK-SAME: -> tensor<!tf.variant<tensor<16x1xf32>>>
  func @while_variant(%arg0: tensor<!tf.variant<tensor<16x1xf32>>>) -> tensor<!tf.variant> {
    // CHECK: tf.While
    // CHECK-SAME: -> tensor<!tf.variant<tensor<16x1xf32>>>
    %0 = "tf.While"(%arg0) {cond = @variant_cond_func, body = @variant_body_func, is_stateless = true} : (tensor<!tf.variant<tensor<16x1xf32>>>) -> tensor<!tf.variant>
    // CHECK: tf.ZerosLike
    // CHECK-SAME: -> tensor<!tf.variant<tensor<16x1xf32>>>
    %1 = "tf.ZerosLike"(%0) : (tensor<!tf.variant>) -> tensor<!tf.variant>
    // CHECK: tf.Identity
    // CHECK-SAME: -> tensor<!tf.variant<tensor<16x1xf32>>>
    %2 = "tf.Identity"(%1) : (tensor<!tf.variant>) -> tensor<!tf.variant>
    return %2 : tensor<!tf.variant>
  }
  // CHECK-LABEL: func @variant_cond_func
  func @variant_cond_func(%arg0: tensor<!tf.variant<tensor<16x1xf32>>>) -> tensor<i1> {
    %0 = "tf._SomeOp"() : () -> tensor<i1>
    return %0 : tensor<i1>
  }
  // CHECK-LABEL: func @variant_body_func
  func @variant_body_func(%arg0: tensor<!tf.variant<tensor<16x1xf32>>>) -> tensor<!tf.variant<tensor<16x1xf32>>> {
    return %arg0 : tensor<!tf.variant<tensor<16x1xf32>>>
  }

  // Test propagation from called functions to the call site.
  // CHECK-LABEL: func @stateful_partitioned_call(
  // CHECK-SAME: -> tensor<20xi32>
  func @stateful_partitioned_call(%arg0: tensor<20xi32>, %arg1: tensor<?xi32>) -> tensor<*xi32> {
    // CHECK: tf.PartitionedCall
    // CHECK-SAME: (tensor<20xi32>) -> tensor<20xi32>
    %0 = "tf.PartitionedCall"(%arg0) {config = "", config_proto = "", executor_type = "", f = @partitioned_called_func} : (tensor<20xi32>) -> tensor<*xi32>
    // CHECK: tf.StatefulPartitionedCall
    // CHECK-SAME: (tensor<20xi32>) -> tensor<20xi32>
    %1 = "tf.StatefulPartitionedCall"(%arg0) {config = "", config_proto = "", executor_type = "", f = @stateful_partitioned_call_func} : (tensor<20xi32>) -> tensor<*xi32>
    // CHECK: tf.TPUPartitionedCall
    // CHECK-SAME: (tensor<20xi32>, tensor<?xi32>) -> tensor<20xi32>
    %2 = "tf.TPUPartitionedCall"(%arg0, %arg1) {autotuner_thresh = 0 : i64, f = @tpu_partitioned_call_func} : (tensor<20xi32>, tensor<?xi32>) -> tensor<*xi32>
    return %0 : tensor<*xi32>
  }
  func @partitioned_called_func(%arg0: tensor<?xi32>) -> (tensor<?xi32>) {
    return %arg0 : tensor<?xi32>
  }
  func @stateful_partitioned_call_func(%arg0: tensor<?xi32>) -> (tensor<?xi32>) {
    return %arg0 : tensor<?xi32>
  }
  func @tpu_partitioned_call_func(%arg0: tensor<?xi32>) -> (tensor<?xi32>) {
    return %arg0 : tensor<?xi32>
  }

  // Test propagation involving const values across caller and callee.
  func @partitioned_call_const(%arg0 : tensor<6xf32>) -> tensor<*xf32> {
    %0 = "tf.Const"() {value = dense<[3, 2]> : tensor<2xi32>} : () -> tensor<2xi32>
    %1 = "tf.PartitionedCall"(%0) {config = "", config_proto = "", executor_type = "", f = @partitioned_call_func_const} : (tensor<2xi32>) -> (tensor<2xi32>)
    // CHECK: "tf.Reshape"
    // CHECK-SAME: tensor<3x2xf32>
    %2 = "tf.Reshape"(%arg0, %1) : (tensor<6xf32>, tensor<2xi32>) -> tensor<*xf32>
    return %2 : tensor<*xf32>
  }

  // CHECK-LABEL: func @partitioned_call_func_const
  func @partitioned_call_func_const(%arg0: tensor<2xi32>) -> tensor<2xi32> {
    return %arg0 : tensor<2xi32>
  }

  // Test iteratively updating call site if a std.call is used.
  // CHECK-LABEL: func @call_partitioned_call2(
  // CHECK-SAME: -> tensor<1xi32>
  func @call_partitioned_call2() -> tensor<*xi32> {
    // CHECK: () -> tensor<1xi32>
    %0 = call @partitioned_called_func2() : () -> tensor<*xi32>
    return %0 : tensor<*xi32>
  }
  // CHECK-LABEL: func @partitioned_called_func2(
  // CHECK-SAME: -> tensor<1xi32>
  func @partitioned_called_func2() -> (tensor<*xi32>) {
    %0 = "tf.Const"() {value = dense<-1> : tensor<1xi32>} : () -> tensor<1xi32>
    %1 = tensor.cast %0 : tensor<1xi32> to tensor<*xi32>
    return %1 : tensor<*xi32>
  }

  // CHECK-LABEL: func @tensor_list_refine
  func @tensor_list_refine() {
    tf_executor.graph {
      %control = tf_executor.island {
        %0 = "tf.Const"() {device = "", value = dense<2> : tensor<2xi32>} : () -> tensor<2xi32>
        %1 = "tf.Const"() {device = "", value = dense<3> : tensor<i32>} : () -> tensor<i32>
        // CHECK: TensorListReserve{{.*}}-> tensor<!tf.variant<tensor<2x2x!tf.variant>>>
        %2 = "tf.TensorListReserve"(%0, %1) {device = ""} : (tensor<2xi32>, tensor<i32>) -> tensor<!tf.variant<tensor<*x!tf.variant>>>
        // CHECK: TensorListReserve{{.*}}-> tensor<!tf.variant<tensor<2x2xf32>>>
        %3 = "tf.TensorListReserve"(%0, %1) {device = ""} : (tensor<2xi32>, tensor<i32>) -> tensor<!tf.variant<tensor<*xf32>>>
        %4 = "tf.Const"() {device = "", value = dense<0> : tensor<i32>} : () -> tensor<i32>
        %5 = "tf.Const"() {device = "", value = dense<[[1.000000e+00, 2.000000e+00], [3.000000e+00, 4.000000e+00]]> : tensor<2x2xf32>} : () -> tensor<2x2xf32>
        // CHECK: tf.TensorListSetItem{{.*}}: (tensor<!tf.variant<tensor<2x2xf32>>>, tensor<i32>, tensor<2x2xf32>) -> tensor<!tf.variant<tensor<2x2xf32>>>
        %6 = "tf.TensorListSetItem"(%3, %4, %5) {device = ""} : (tensor<!tf.variant<tensor<*xf32>>>, tensor<i32>, tensor<2x2xf32>)-> tensor<*x!tf.variant>
        %7 = "tf.Const"() {device = "", value = dense<-1> : tensor<i32>} : () -> tensor<i32>
        // CHECK: tf.TensorListStack{{.*}}: (tensor<!tf.variant<tensor<2x2xf32>>>, tensor<i32>) -> tensor<?x2x2xf32>
        %8 = "tf.TensorListStack"(%6, %7) {device = "", num_elements = -1 : i64} : (tensor<*x!tf.variant>, tensor<i32>) -> tensor<*xf32>
        tf_executor.yield
      }
      tf_executor.fetch
    }
    return
  }

  // CHECK-LABEL: single_mutation_same_element_shape
  func @single_mutation_same_element_shape() {
    %elem_shape = "tf.Const"() {value = dense<[-1, 1]> : tensor<2xi32>} : () -> tensor<2xi32>
    %size = "tf.Const"() {value = dense<10> : tensor<i32>} : () -> tensor<i32>
    %elem = "tf._SomeOp"() : () -> tensor<16x1xf32>
    // CHECK: EmptyTensorList
    // CHECK-SAME: tensor<!tf.variant<tensor<16x1xf32>>>
    %tl_0 = "tf.EmptyTensorList"(%elem_shape, %size) : (tensor<2xi32>, tensor<i32>) -> tensor<!tf.variant<tensor<?x1xf32>>>
    %tl_1 = "tf.TensorListPushBack"(%tl_0, %elem) : (tensor<!tf.variant<tensor<?x1xf32>>>, tensor<16x1xf32>) -> tensor<!tf.variant<tensor<?x1xf32>>>
    %tl_2 = "tf.TensorListPushBack"(%tl_1, %elem) : (tensor<!tf.variant<tensor<?x1xf32>>>, tensor<16x1xf32>) -> tensor<!tf.variant<tensor<?x1xf32>>>
    %stack = "tf.TensorListStack"(%tl_2, %elem_shape) {num_elements = -1 : i64} : (tensor<!tf.variant<tensor<?x1xf32>>>, tensor<2xi32>) -> tensor<*xf32>
    return
  }

  // CHECK-LABEL: single_mutation_multiple_element_shape
  func @single_mutation_multiple_element_shape() {
    %elem_shape = "tf.Const"() {value = dense<[-1, 1]> : tensor<2xi32>} : () -> tensor<2xi32>
    %size = "tf.Const"() {value = dense<10> : tensor<i32>} : () -> tensor<i32>
    %elem_0 = "tf._SomeOp"() : () -> tensor<16x1xf32>
    %elem_1 = "tf._SomeOtherOp"() : () -> tensor<8x1xf32>
    // CHECK: EmptyTensorList
    // CHECK-SAME: tensor<!tf.variant<tensor<?x1xf32>>>
    %tl_0 = "tf.EmptyTensorList"(%elem_shape, %size) : (tensor<2xi32>, tensor<i32>) -> tensor<!tf.variant<tensor<?x1xf32>>>
    %tl_1 = "tf.TensorListPushBack"(%tl_0, %elem_0) : (tensor<!tf.variant<tensor<?x1xf32>>>, tensor<16x1xf32>) -> tensor<!tf.variant<tensor<?x1xf32>>>
    %tl_2 = "tf.TensorListPushBack"(%tl_1, %elem_1) : (tensor<!tf.variant<tensor<?x1xf32>>>, tensor<8x1xf32>) -> tensor<!tf.variant<tensor<?x1xf32>>>
    return
  }

  // CHECK-LABEL: single_mutation_dynamic_element_shape
  func @single_mutation_dynamic_element_shape() {
    %elem_shape = "tf.Const"() {value = dense<[-1, 1]> : tensor<2xi32>} : () -> tensor<2xi32>
    %size = "tf.Const"() {value = dense<10> : tensor<i32>} : () -> tensor<i32>
    %elem_0 = "tf._SomeOp"() : () -> tensor<16x1xf32>
    %elem_1 = "tf._SomeOtherOp"() : () -> tensor<?x1xf32>
    // CHECK: EmptyTensorList
    // CHECK-SAME: tensor<!tf.variant<tensor<?x1xf32>>>
    %tl_0 = "tf.EmptyTensorList"(%elem_shape, %size) : (tensor<2xi32>, tensor<i32>) -> tensor<!tf.variant<tensor<?x1xf32>>>
    %tl_1 = "tf.TensorListPushBack"(%tl_0, %elem_0) : (tensor<!tf.variant<tensor<?x1xf32>>>, tensor<16x1xf32>) -> tensor<!tf.variant<tensor<?x1xf32>>>
    %tl_2 = "tf.TensorListPushBack"(%tl_1, %elem_1) : (tensor<!tf.variant<tensor<?x1xf32>>>, tensor<?x1xf32>) -> tensor<!tf.variant<tensor<?x1xf32>>>
    return
  }

  // CHECK-LABEL: multiple_mutation_same_element_shape
  func @multiple_mutation_same_element_shape() {
    %elem_shape = "tf.Const"() {value = dense<[-1, 1]> : tensor<2xi32>} : () -> tensor<2xi32>
    %size = "tf.Const"() {value = dense<10> : tensor<i32>} : () -> tensor<i32>
    %elem_0 = "tf._SomeOp"() : () -> tensor<16x1xf32>
    %elem_1 = "tf._SomeOtherOp"() : () -> tensor<16x1xf32>
    // CHECK: EmptyTensorList
    // CHECK-SAME: tensor<!tf.variant<tensor<16x1xf32>>>
    %tl_0 = "tf.EmptyTensorList"(%elem_shape, %size) : (tensor<2xi32>, tensor<i32>) -> tensor<!tf.variant<tensor<?x1xf32>>>
    %tl_1 = "tf.TensorListPushBack"(%tl_0, %elem_0) : (tensor<!tf.variant<tensor<?x1xf32>>>, tensor<16x1xf32>) -> tensor<!tf.variant<tensor<?x1xf32>>>
    %resize = "tf.Const"() {value = dense<20> : tensor<i32>} : () -> tensor<i32>
    %tl_2 = "tf.TensorListResize"(%tl_0, %resize) : (tensor<!tf.variant<tensor<?x1xf32>>>, tensor<i32>) -> tensor<!tf.variant<tensor<?x1xf32>>>
    return
  }

  // CHECK-LABEL: multiple_mutation_multiple_element_shape
  func @multiple_mutation_multiple_element_shape() {
    %elem_shape = "tf.Const"() {value = dense<[-1, 1]> : tensor<2xi32>} : () -> tensor<2xi32>
    %size = "tf.Const"() {value = dense<10> : tensor<i32>} : () -> tensor<i32>
    %elem_0 = "tf._SomeOp"() : () -> tensor<16x1xf32>
    // CHECK: EmptyTensorList
    // CHECK-SAME: tensor<!tf.variant<tensor<?x1xf32>>>
    %tl_0 = "tf.EmptyTensorList"(%elem_shape, %size) : (tensor<2xi32>, tensor<i32>) -> tensor<!tf.variant<tensor<?x1xf32>>>
    %tl_1 = "tf.TensorListPushBack"(%tl_0, %elem_0) : (tensor<!tf.variant<tensor<?x1xf32>>>, tensor<16x1xf32>) -> tensor<!tf.variant<tensor<?x1xf32>>>
    %tl_2 = "tf.TensorListPushBack"(%tl_1, %elem_0) : (tensor<!tf.variant<tensor<?x1xf32>>>, tensor<16x1xf32>) -> tensor<!tf.variant<tensor<?x1xf32>>>
    %zero = "tf.Const"() {value = dense<0> : tensor<i32>} : () -> tensor<i32>
    %elem_1 = "tf._SomeOtherOp"() : () -> tensor<8x1xf32>
    %tl_3 = "tf.TensorListSetItem"(%tl_1, %zero, %elem_1) : (tensor<!tf.variant<tensor<?x1xf32>>>, tensor<i32>, tensor<8x1xf32>) -> tensor<!tf.variant<tensor<?x1xf32>>>
    return
  }

  // CHECK-LABEL: multiple_mutation_dynamic_element_shape
  func @multiple_mutation_dynamic_element_shape() {
    %elem_shape = "tf.Const"() {value = dense<[-1, 1]> : tensor<2xi32>} : () -> tensor<2xi32>
    %size = "tf.Const"() {value = dense<10> : tensor<i32>} : () -> tensor<i32>
    %elem_0 = "tf._SomeOp"() : () -> tensor<16x1xf32>
    // CHECK: EmptyTensorList
    // CHECK-SAME: tensor<!tf.variant<tensor<?x1xf32>>>
    %tl_0 = "tf.EmptyTensorList"(%elem_shape, %size) : (tensor<2xi32>, tensor<i32>) -> tensor<!tf.variant<tensor<?x1xf32>>>
    %tl_1 = "tf.TensorListPushBack"(%tl_0, %elem_0) : (tensor<!tf.variant<tensor<?x1xf32>>>, tensor<16x1xf32>) -> tensor<!tf.variant<tensor<?x1xf32>>>
    %tl_2 = "tf.TensorListPushBack"(%tl_1, %elem_0) : (tensor<!tf.variant<tensor<?x1xf32>>>, tensor<16x1xf32>) -> tensor<!tf.variant<tensor<?x1xf32>>>
    %zero = "tf.Const"() {value = dense<0> : tensor<i32>} : () -> tensor<i32>
    %elem_1 = "tf._SomeOtherOp"() : () -> tensor<?x1xf32>
    %tl_3 = "tf.TensorListSetItem"(%tl_1, %zero, %elem_1) : (tensor<!tf.variant<tensor<?x1xf32>>>, tensor<i32>, tensor<?x1xf32>) -> tensor<!tf.variant<tensor<?x1xf32>>>
    return
  }

  // CHECK-LABEL: infer_subtype_from_scatter
  func @infer_subtype_from_scatter() {
    %elem_shape = "tf.Const"() {value = dense<[-1, 1]> : tensor<2xi32>} : () -> tensor<2xi32>
    %size = "tf.Const"() {value = dense<10> : tensor<i32>} : () -> tensor<i32>
    %tensors = "tf._SomeOp"() : () -> tensor<3x16x1xf32>
    %indices = "tf.Const"() {value = dense<[2, 5, 9]> : tensor<3xi32>} : () -> tensor<3xi32>
    // CHECK: TensorListReserve
    // CHECK-SAME: tensor<!tf.variant<tensor<16x1xf32>>>
    %tl_0 = "tf.TensorListReserve"(%elem_shape, %size) : (tensor<2xi32>, tensor<i32>) -> tensor<!tf.variant<tensor<?x1xf32>>>
    %tl_1 = "tf.TensorListScatterIntoExistingList"(%tl_0, %tensors, %indices) : (tensor<!tf.variant<tensor<?x1xf32>>>, tensor<3x16x1xf32>, tensor<3xi32>) -> tensor<!tf.variant<tensor<?x1xf32>>>
    return
  }

  // CHECK-LABEL: tensorlist_from_tensor
  func @tensorlist_from_tensor() {
    %elem_shape = "tf.Const"() {value = dense<[-1, 1]> : tensor<2xi32>} : () -> tensor<2xi32>
    %tensors = "tf._SomeOp"() : () -> tensor<10x16x1xf32>
    // CHECK: TensorListFromTensor
    // CHECK-SAME: tensor<!tf.variant<tensor<16x1xf32>>>
    %tl = "tf.TensorListFromTensor"(%tensors, %elem_shape) : (tensor<10x16x1xf32>, tensor<2xi32>) -> tensor<!tf.variant<tensor<?x1xf32>>>
    return
  }

  // CHECK-LABEL: infer_subtype_from_write_while
  func @infer_subtype_from_write_while() {
    %zero = "tf.Const"() {value = dense<0> : tensor<i32>} : () -> tensor<i32>
    %one = "tf.Const"() {value = dense<1> : tensor<i32>} : () -> tensor<i32>
    %elem_shape = "tf.Const"() {value = dense<[-1, 1]> : tensor<2xi32>} : () -> tensor<2xi32>
    %size = "tf.Const"() {value = dense<10> : tensor<i32>} : () -> tensor<i32>
    // CHECK: TensorListReserve
    // CHECK-SAME: tensor<!tf.variant<tensor<16x1xf32>>>
    %tl = "tf.TensorListReserve"(%elem_shape, %size) : (tensor<2xi32>, tensor<i32>) -> tensor<!tf.variant<tensor<?x1xf32>>>
    %while:2 = "tf.WhileRegion"(%zero, %tl) ({
      ^bb0(%barg0: tensor<i32>, %barg1: tensor<!tf.variant<tensor<?x1xf32>>>): // no predeceessors
        %cond = "tf.Less"(%barg0, %size) : (tensor<i32>, tensor<i32>) -> tensor<i1>
        "tf.Yield"(%cond) : (tensor<i1>) -> ()
    }, {
      ^bb0(%barg0: tensor<i32>, %barg1: tensor<!tf.variant<tensor<?x1xf32>>>): // no predeceessors
      %index = "tf.AddV2"(%barg0, %one) : (tensor<i32>, tensor<i32>) -> tensor<i32>
      %elem = "tf._SomeOp"() : () -> tensor<16x1xf32>
      %tl_loop = "tf.TensorListSetItem"(%barg1, %index, %elem) : (tensor<!tf.variant<tensor<?x1xf32>>>, tensor<i32>, tensor<16x1xf32>) -> tensor<!tf.variant<tensor<?x1xf32>>>
      "tf.Yield"(%index, %tl_loop) : (tensor<i32>, tensor<!tf.variant<tensor<?x1xf32>>>) -> ()
    }) {is_stateless = false} : (tensor<i32>, tensor<!tf.variant<tensor<?x1xf32>>>) -> (tensor<i32>, tensor<!tf.variant<tensor<?x1xf32>>>)
    return
  }

  // CHECK-LABEL: tensor_list_if_region_yield_multiple_elem_shape
  func @tensor_list_if_region_yield_multiple_elem_shape(%arg0: tensor<i1>) -> () {
    %zero = "tf.Const"() {value = dense<0> : tensor<i32>} : () -> tensor<i32>
    %one = "tf.Const"() {value = dense<1> : tensor<i32>} : () -> tensor<i32>
    %elem_shape = "tf.Const"() {value = dense<[-1, 1]> : tensor<2xi32>} : () -> tensor<2xi32>
    %size = "tf.Const"() {value = dense<10> : tensor<i32>} : () -> tensor<i32>
    // CHECK: TensorListReserve
    // CHECK-SAME: tensor<!tf.variant<tensor<?x1xf32>>>
    %tl_0 = "tf.TensorListReserve"(%elem_shape, %size) : (tensor<2xi32>, tensor<i32>) -> tensor<!tf.variant<tensor<?x1xf32>>>
    %elem_0 = "tf.SomeOp"() : () -> tensor<16x1xf32>
    %elem_1 = "tf.SomeOp"() : () -> tensor<16x1xf32>
    %elem_2 = "tf.SomeOp"() : () -> tensor<8x1xf32>
    %tl_1 = "tf.IfRegion"(%arg0) ({
      %tl_true = "tf.TensorListSetItem"(%tl_0, %zero, %elem_0) : (tensor<!tf.variant<tensor<?x1xf32>>>, tensor<i32>, tensor<16x1xf32>) -> tensor<!tf.variant<tensor<?x1xf32>>>
      "tf.Yield"(%tl_true) : (tensor<!tf.variant<tensor<?x1xf32>>>) -> ()
    }, {
      %tl_false = "tf.TensorListSetItem"(%tl_0, %zero, %elem_1) : (tensor<!tf.variant<tensor<?x1xf32>>>, tensor<i32>, tensor<16x1xf32>) -> tensor<!tf.variant<tensor<?x1xf32>>>
      "tf.Yield"(%tl_false) : (tensor<!tf.variant<tensor<?x1xf32>>>) -> ()
    }) {is_stateless = false} : (tensor<i1>) -> tensor<!tf.variant<tensor<?x1xf32>>>
    %tl_2 = "tf.TensorListSetItem"(%tl_1, %one, %elem_2) : (tensor<!tf.variant<tensor<?x1xf32>>>, tensor<i32>, tensor<8x1xf32>) -> tensor<!tf.variant<tensor<?x1xf32>>>
    return
  }

  // CHECK-LABEL: while_result_multiple_element_shape
  func @while_result_multiple_element_shape() {
    %zero = "tf.Const"() {value = dense<0> : tensor<i32>} : () -> tensor<i32>
    %one = "tf.Const"() {value = dense<1> : tensor<i32>} : () -> tensor<i32>
    %elem_shape = "tf.Const"() {value = dense<[-1, 1]> : tensor<2xi32>} : () -> tensor<2xi32>
    %size = "tf.Const"() {value = dense<10> : tensor<i32>} : () -> tensor<i32>
    // CHECK: TensorListReserve
    // CHECK-SAME: tensor<!tf.variant<tensor<?x1xf32>>>
    %tl = "tf.TensorListReserve"(%elem_shape, %size) : (tensor<2xi32>, tensor<i32>) -> tensor<!tf.variant<tensor<?x1xf32>>>
    %while:2 = "tf.WhileRegion"(%zero, %tl) ({
      ^bb0(%barg0: tensor<i32>, %barg1: tensor<!tf.variant<tensor<?x1xf32>>>): // no predeceessors
        %cond = "tf.Less"(%barg0, %size) : (tensor<i32>, tensor<i32>) -> tensor<i1>
        "tf.Yield"(%cond) : (tensor<i1>) -> ()
    }, {
      ^bb0(%barg0: tensor<i32>, %barg1: tensor<!tf.variant<tensor<?x1xf32>>>): // no predeceessors
      %index = "tf.AddV2"(%barg0, %one) : (tensor<i32>, tensor<i32>) -> tensor<i32>
      %elem_0 = "tf._SomeOp"() : () -> tensor<16x1xf32>
      %tl_loop = "tf.TensorListSetItem"(%barg1, %index, %elem_0) : (tensor<!tf.variant<tensor<?x1xf32>>>, tensor<i32>, tensor<16x1xf32>) -> tensor<!tf.variant<tensor<?x1xf32>>>
      "tf.Yield"(%index, %tl_loop) : (tensor<i32>, tensor<!tf.variant<tensor<?x1xf32>>>) -> ()
    }) {is_stateless = false} : (tensor<i32>, tensor<!tf.variant<tensor<?x1xf32>>>) -> (tensor<i32>, tensor<!tf.variant<tensor<?x1xf32>>>)
    %elem_1 = "tf._SomeOtherOp"() : () -> tensor<8x1xf32>
    %tl_set_item = "tf.TensorListSetItem"(%while#1, %one, %elem_1) : (tensor<!tf.variant<tensor<?x1xf32>>>, tensor<i32>, tensor<8x1xf32>) -> tensor<!tf.variant<tensor<?x1xf32>>>
    return
  }

  // CHECK-LABEL: do_not_refine_tensorlist_with_unknown_user
  func @do_not_refine_tensorlist_with_unknown_user() {
    %elem_shape = "tf.Const"() {value = dense<[-1, 1]> : tensor<2xi32>} : () -> tensor<2xi32>
    %size = "tf.Const"() {value = dense<10> : tensor<i32>} : () -> tensor<i32>
    %elem = "tf._SomeOp"() : () -> tensor<16x1xf32>
    // CHECK: EmptyTensorList
    // CHECK-SAME: tensor<!tf.variant<tensor<?x1xf32>>>
    %tl_0 = "tf.EmptyTensorList"(%elem_shape, %size) : (tensor<2xi32>, tensor<i32>) -> tensor<!tf.variant<tensor<?x1xf32>>>
    %tl_1 = "tf.TensorListPushBack"(%tl_0, %elem) : (tensor<!tf.variant<tensor<?x1xf32>>>, tensor<16x1xf32>) -> tensor<!tf.variant<tensor<?x1xf32>>>
    "tf.UnknownTensorListUser"(%tl_1) : (tensor<!tf.variant<tensor<?x1xf32>>>) -> ()
    return
  }

  // CHECK-LABEL: replace_tensor_list_element_shape
  func @replace_tensor_list_element_shape() {
    // CHECK: %[[ELEMENT_SHAPE:.*]] = "tf.Const"() {value = dense<[-1, 1]> : tensor<2xi32>}
    %elem_shape = "tf.Const"() {value = dense<[-1, 1]> : tensor<2xi32>} : () -> tensor<2xi32>
    %size = "tf.Const"() {value = dense<10> : tensor<i32>} : () -> tensor<i32>
    %elem = "tf._SomeOp"() : () -> tensor<16x1xf32>
    // CHECK: EmptyTensorList
    // CHECK-SAME: tensor<!tf.variant<tensor<16x1xf32>>>
    %tl_0 = "tf.EmptyTensorList"(%elem_shape, %size) : (tensor<2xi32>, tensor<i32>) -> tensor<!tf.variant<tensor<?x1xf32>>>
    %tl_1 = "tf.TensorListPushBack"(%tl_0, %elem) : (tensor<!tf.variant<tensor<?x1xf32>>>, tensor<16x1xf32>) -> tensor<!tf.variant<tensor<?x1xf32>>>
    %shape_32 = "tf.TensorListElementShape"(%tl_1) : (tensor<!tf.variant<tensor<?x1xf32>>>) -> tensor<?xi32>
    %shape_64 = "tf.TensorListElementShape"(%tl_1) : (tensor<!tf.variant<tensor<?x1xf32>>>) -> tensor<?xi64>
    // CHECK: %[[CAST:.*]] = "tf.Cast"(%[[ELEMENT_SHAPE]]){{.*}}: (tensor<2xi32>) -> tensor<2xi64>
    // CHECK: "tf._SomeOtherOp"(%[[ELEMENT_SHAPE]], %[[CAST]])
    "tf._SomeOtherOp"(%shape_32, %shape_64) : (tensor<?xi32>, tensor<?xi64>) -> ()
    return
  }

  // CHECK-LABEL: do_not_unrefine_fully_defined_subtypes
  func @do_not_unrefine_fully_defined_subtypes() {
    %elem_shape = "tf.Const"() {value = dense<[-1, 1]> : tensor<2xi32>} : () -> tensor<2xi32>
    %size = "tf.Const"() {value = dense<2> : tensor<i32>} : () -> tensor<i32>
    // CHECK: "tf.TensorListReserve"
    // CHECK-SAME: tensor<!tf.variant<tensor<16x1xf32>>>
    %tl_0 = "tf.TensorListReserve"(%elem_shape, %size) : (tensor<2xi32>, tensor<i32>) -> tensor<!tf.variant<tensor<16x1xf32>>>
    return
  }

  // CHECK-LABEL: dont_update_for_ref
  func @dont_update_for_ref() -> () {
    // CHECK: () -> tensor<4x!tf.f32ref>
    %11 = "tf.VariableV2"() {container = "", device = "", shape = #tf.shape<4>, shared_name = ""} : () -> tensor<4x!tf.f32ref>
    // CHECK: (tensor<4x!tf.f32ref>) -> tensor<4xf32>
    %12 = "tf.Identity"(%11) {device = ""} : (tensor<4x!tf.f32ref>) -> tensor<4xf32>
    // CHECK: (tensor<4xf32>) -> tensor<4xf32>
    %13 = "tf.Neg"(%12) {device = ""} : (tensor<4xf32>) -> tensor<4xf32>
    return
  }

  // CHECK-LABEL: operand_as_shape
  func @operand_as_shape(%18: tensor<i32>, %39: tensor<1x4x4x32xf32>) -> () {
    %cst_5 = constant dense<512> : tensor<i32>
    %19 = "tf.Pack"(%18, %cst_5) {N = 2 : i64, T = i32, axis = 0 : i64, device = ""} : (tensor<i32>, tensor<i32>) -> tensor<2xi32>
    // CHECK: -> tensor<1x512xf32>
    %40 = "tf.Reshape"(%39, %19) {T = f32, Tshape = i32, device = ""} : (tensor<1x4x4x32xf32>, tensor<2xi32>) -> tensor<?x?xf32>
   return
  }

  // CHECK-LABEL: const_fold
  func @const_fold() -> () {
    // CHECK: tf.Const
    // CHECK-SAME: () -> tensor<4xi32>
    %0 = "tf.Const"() {value = dense<[200, 26, 26, 32]> : tensor<4xi32>} : () -> tensor<*xi32>
    // CHECK: tf.Const
    // CHECK-SAME: () -> tensor<4xi32>
    %1 = "tf.Const"() {value = dense<[200, 26, 26, 32]> : tensor<4xi32>} : () -> tensor<*xi32>
    // CHECK: tf.Add
    // CHECK-SAME: (tensor<4xi32>, tensor<4xi32>) -> tensor<4xi32>
    %2 = "tf.Add"(%0, %1) : (tensor<*xi32>, tensor<*xi32>) -> tensor<*xi32>
    return
  }

  // CHECK-LABEL: cast_at_end(%arg0:
  // CHECK-SAME: tensor<16x194x199x4xui8>, tensor<16x194x199x4xi8>, tensor<*xi8>, tensor<*xi8>
  func @cast_at_end(%arg0: tensor<16x194x199x4xf32>, %arg1: tensor<16x194x199x4xi8>, %arg2: tensor<*xf32>) -> (tensor<*xui8>, tensor<*xi8>, tensor<*xi8>, tensor<*xi8>) {
    %27 = "tf.Cast"(%arg0) {Truncate = false, device = ""} : (tensor<16x194x199x4xf32>) -> tensor<*xui8>
    %28 = "tf.Cast"(%arg0) {Truncate = false, device = ""} : (tensor<16x194x199x4xf32>) -> tensor<*xi8>
    %29 = "tf.Cast"(%arg2) {Truncate = false, device = ""} : (tensor<*xf32>) -> tensor<*xi8>
    // CHECK: %[[CAST_RESULT_2:.*]] = "tf.Cast"(%arg0)
    // CHECK-SAME: (tensor<16x194x199x4xf32>) -> tensor<*xi8>
    // CHECK: %[[CAST_RESULT_3:.*]] = "tf.Cast"(%arg2)
    // CHECK-SAME: (tensor<*xf32>) -> tensor<*xi8>
    // CHECK: %[[ADDI:.*]] = addi %[[CAST_RESULT_2]], %[[CAST_RESULT_2]]
    %2 = addi %28, %28 : tensor<*xi8>
    // CHECK: %[[CAST_RESULT_0:.*]] = "tf.Cast"(%arg0)
    // CHECK-SAME: (tensor<16x194x199x4xf32>) -> tensor<16x194x199x4xui8>
    // CHECK: %[[CAST_RESULT_1:.*]] = "tf.Cast"(%arg0)
    // CHECK-SAME: (tensor<16x194x199x4xf32>) -> tensor<16x194x199x4xi8>
    // CHECK: return %[[CAST_RESULT_0]], %[[CAST_RESULT_1]], %[[CAST_RESULT_3]], %[[ADDI]]
    return %27, %28, %29, %2 : tensor<*xui8>, tensor<*xi8>, tensor<*xi8>, tensor<*xi8>
  }

  // CHECK-LABEL: infer_device_launch
  func @infer_device_launch(%arg0: tensor<1x8x2xi32>) -> (tensor<*xf32>, tensor<*xf32>) {
    %0 = "tf.Const"() {value = dense<-1> : tensor<i32>} : () -> tensor<i32>
    %1 = "tf_device.launch"() ({
      %2 = "tf.Cast"(%arg0) {Truncate = false} : (tensor<1x8x2xi32>) -> tensor<1x8x2xf32>
      tf_device.return %2 : tensor<1x8x2xf32>
    // CHECK: () -> tensor<1x8x2xf32>
    }) {device = "/device:CPU:0"} : () -> tensor<*xf32>
    // CHECK: "tf.Cast"(%{{.*}}) {Truncate = false} : (tensor<1x8x2xf32>) -> tensor<*xf32>
    // CHECK: (tensor<i32>, tensor<1x8x2xf32>) -> (tensor<1x8x1xf32>, tensor<1x8x1xf32>)
    %3:2 = "tf.Split"(%0, %1) {device = ""} : (tensor<i32>, tensor<*xf32>) -> (tensor<*xf32>, tensor<*xf32>)
    %4 = addf %1, %1 : tensor<*xf32>
    return %3#0, %3#1 : tensor<*xf32>, tensor<*xf32>
  }

  // CHECK-LABEL: infer_device_cluster
  func @infer_device_cluster(%arg0: tensor<1x8x2xi32>) -> (tensor<*xf32>, tensor<*xf32>) {
    %0 = "tf.Const"() {value = dense<-1> : tensor<i32>} : () -> tensor<i32>
    %1 = "tf_device.cluster"() ({
      %2 = "tf.Cast"(%arg0) {Truncate = false} : (tensor<1x8x2xi32>) -> tensor<1x8x2xf32>
      tf_device.return %2 : tensor<1x8x2xf32>
    // CHECK: () -> tensor<1x8x2xf32>
    }) : () -> tensor<*xf32>
    // CHECK: "tf.Cast"(%{{.*}}) {Truncate = false} : (tensor<1x8x2xf32>) -> tensor<*xf32>
    // CHECK: (tensor<i32>, tensor<1x8x2xf32>) -> (tensor<1x8x1xf32>, tensor<1x8x1xf32>)
    %3:2 = "tf.Split"(%0, %1) {device = ""} : (tensor<i32>, tensor<*xf32>) -> (tensor<*xf32>, tensor<*xf32>)
    %4 = addf %1, %1 : tensor<*xf32>
    return %3#0, %3#1 : tensor<*xf32>, tensor<*xf32>
  }

  // CHECK-LABEL: func @tensor_cast(%arg0: tensor<1xi32>) -> tensor<1xi32>
  func @tensor_cast(%arg0: tensor<1xi32>) -> tensor<*xi32> {
   // CHECK: %[[RESULT:.*]] = tensor.cast
   // CHECK-SAME: tensor<1xi32> to tensor<1xi32>
   // CHECK: return %[[RESULT]] : tensor<1xi32>
    %1 = tensor.cast %arg0 : tensor<1xi32> to tensor<*xi32>
    return %1 : tensor<*xi32>
  }

  // CHECK-LABEL: operand_pack_unranked
  // Verify fix: this only verifies that shape inference runs and completes on
  // this input, rather than refining any shapes.
  func @operand_pack_unranked(%arg0: tensor<*xf32>) -> () {
   // CHECK: tf.Pack
   %outputs_0 = "tf.Pack"(%arg0) {axis = 0 : i64, device = ""} : (tensor<*xf32>) -> tensor<*xf32>
   %outputs_2 = "tf.TensorSliceDataset"(%outputs_0) {device = "", output_shapes = [#tf.shape<>]} : (tensor<*xf32>) -> tensor<!tf.variant>
   return
  }

  // Test resource result subtypes are propagated to call op results.
  // CHECK-LABEL: func @pcall_resource_result
  func @pcall_resource_result(%arg0: tensor<*x!tf.resource<tensor<f32>>>) {
    // CHECK: "tf.StatefulPartitionedCall"
    // CHECK-SAME: (tensor<*x!tf.resource<tensor<f32>>>) -> tensor<*x!tf.resource<tensor<f32>>>
    %0 = "tf.StatefulPartitionedCall"(%arg0) {config = "", config_proto = "", executor_type = "", f = @pcall_resource_result_func} : (tensor<*x!tf.resource<tensor<f32>>>) -> tensor<*x!tf.resource>
    return
  }
  func @pcall_resource_result_func(%arg0: tensor<*x!tf.resource<tensor<f32>>>) -> tensor<*x!tf.resource<tensor<f32>>> {
    return %arg0 : tensor<*x!tf.resource<tensor<f32>>>
  }

  // Check that the fold for tf.Size does not crash with unranked output type.
  // CHECK-LABEL: func @unranked_tf_size
  func @unranked_tf_size() -> tensor<*xi32> {
    %0 = "tf.Const"() {value = dense<[-1, 26]> : tensor<2xi32>} : () -> tensor<2xi32>
    %add = "tf.AddV2"(%0, %0) : (tensor<2xi32>, tensor<2xi32>) -> tensor<*xi32>
    // CHECK: "tf.Size"
    // CHECK-SAME: (tensor<2xi32>) -> tensor<i32>
    %size = "tf.Size"(%add) {device = ""} : (tensor<*xi32>) -> tensor<*xi32>
    return %size : tensor<*xi32>
  }

  // Test no tf.Cast ops are inserted when refining tf_executor.graph results.
  // CHECK-LABEL: func @call_in_graph({{%.+}}: tensor<i32>) -> tensor<i32>
  func @call_in_graph(%arg0: tensor<i32>) -> tensor<*xi32> {
    // CHECK-NOT: tf.Cast
    %0 = tf_executor.graph {
      %1:2 = tf_executor.island wraps "tf.PartitionedCall"(%arg0) {config = "", config_proto = "", executor_type = "", f = @call_in_graph_func} : (tensor<i32>) -> tensor<*xi32>
      tf_executor.fetch %1#0 : tensor<*xi32>
    }
    return %0 : tensor<*xi32>
  }

  // CHECK-LABEL: func @call_in_graph_func({{%.+}}: tensor<i32>) -> tensor<i32>
  func @call_in_graph_func(%arg0: tensor<*xi32>) -> tensor<*xi32> {
    // CHECK-NOT: tf.Cast
    %0 = tf_executor.graph {
      %1:2 = tf_executor.island wraps "tf.Identity"(%arg0) : (tensor<*xi32>) -> tensor<*xi32>
      tf_executor.fetch %1#0 : tensor<*xi32>
    }
    return %0 : tensor<*xi32>
  }

  // Test shape invariant While only propagates operand handle types into
  // results and functions/regions.
  // CHECK-LABEL: func @while_shape_invariant_propagate
  // CHECK-SAME: ({{%.+}}: tensor<4xf32>, {{%.+}}: tensor<!tf.resource<tensor<4xf32>>>, {{%.+}}: tensor<!tf.resource<tensor<8xf32>>>, {{%.+}}: tensor<1xi32>)
  // CHECK-SAME: -> (tensor<*xf32>, tensor<*x!tf.resource<tensor<4xf32>>>, tensor<!tf.resource<tensor<8xf32>>>, tensor<?xi32>, tensor<*xf32>, tensor<*x!tf.resource<tensor<4xf32>>>, tensor<!tf.resource<tensor<8xf32>>>, tensor<?xi32>)
  func @while_shape_invariant_propagate(%arg0: tensor<4xf32>, %arg1: tensor<!tf.resource<tensor<4xf32>>>, %arg2: tensor<!tf.resource<tensor<8xf32>>>, %arg3: tensor<1xi32>) -> (tensor<*xf32>, tensor<*x!tf.resource>, tensor<!tf.resource>, tensor<?xi32>, tensor<*xf32>, tensor<*x!tf.resource>, tensor<!tf.resource>, tensor<?xi32>) {
    // CHECK: "tf.While"
    // CHECK-SAME: (tensor<4xf32>, tensor<!tf.resource<tensor<4xf32>>>, tensor<!tf.resource<tensor<8xf32>>>, tensor<1xi32>)
    // CHECK-SAME: -> (tensor<*xf32>, tensor<*x!tf.resource<tensor<4xf32>>>, tensor<!tf.resource<tensor<8xf32>>>, tensor<?xi32>)
    %0:4 = "tf.While"(%arg0, %arg1, %arg2, %arg3) {cond = @while_shape_invariant_cond_func_propagate, body = @while_shape_invariant_body_func_propagate, is_stateless = false, shape_invariant} : (tensor<4xf32>, tensor<!tf.resource<tensor<4xf32>>>, tensor<!tf.resource<tensor<8xf32>>>, tensor<1xi32>) -> (tensor<*xf32>, tensor<*x!tf.resource>, tensor<!tf.resource>, tensor<?xi32>)

    // CHECK: "tf.WhileRegion"
    %1:4 = "tf.WhileRegion"(%arg0, %arg1, %arg2, %arg3) ( {
    // CHECK-NEXT: ^{{.+}}({{%.+}}: tensor<*xf32>, {{%.+}}: tensor<*x!tf.resource<tensor<4xf32>>>, {{%.+}}: tensor<!tf.resource<tensor<8xf32>>>, {{%.+}}: tensor<?xi32>):
    ^cond(%carg0: tensor<*xf32>, %carg1: tensor<*x!tf.resource>, %carg2: tensor<!tf.resource>, %carg3: tensor<?xi32>):
      %2 = "tf.Const"() {value = dense<true> : tensor<i1>} : () -> tensor<i1>
      "tf.Yield"(%2) : (tensor<i1>) -> ()
    }, {
    // CHECK: ^{{.+}}({{%.+}}: tensor<*xf32>, {{%.+}}: tensor<*x!tf.resource<tensor<4xf32>>>, {{%.+}}: tensor<!tf.resource<tensor<8xf32>>>, {{%.+}}: tensor<?xi32>):
    ^body(%barg0: tensor<*xf32>, %barg1: tensor<*x!tf.resource>, %barg2: tensor<!tf.resource>, %barg3: tensor<?xi32>):
      %2 = "tf.SomeOp"(%barg3) : (tensor<?xi32>) -> tensor<?xi32>
      // CHECK: "tf.Yield"
      // CHECK-SAME: (tensor<*xf32>, tensor<*x!tf.resource<tensor<4xf32>>>, tensor<!tf.resource<tensor<8xf32>>>, tensor<?xi32>) -> ()
      "tf.Yield"(%barg0, %barg1, %barg2, %2) : (tensor<*xf32>, tensor<*x!tf.resource>, tensor<!tf.resource>, tensor<?xi32>) -> ()
    // CHECK-NEXT: shape_invariant
    // CHECK-SAME: (tensor<4xf32>, tensor<!tf.resource<tensor<4xf32>>>, tensor<!tf.resource<tensor<8xf32>>>, tensor<1xi32>)
    // CHECK-SAME: -> (tensor<*xf32>, tensor<*x!tf.resource<tensor<4xf32>>>, tensor<!tf.resource<tensor<8xf32>>>, tensor<?xi32>)
    }) {is_stateless = false, shape_invariant} : (tensor<4xf32>, tensor<!tf.resource<tensor<4xf32>>>, tensor<!tf.resource<tensor<8xf32>>>, tensor<1xi32>) -> (tensor<*xf32>, tensor<*x!tf.resource>, tensor<!tf.resource>, tensor<?xi32>)

    return %0#0, %0#1, %0#2, %0#3, %1#0, %1#1, %1#2, %1#3 : tensor<*xf32>, tensor<*x!tf.resource>, tensor<!tf.resource>, tensor<?xi32>, tensor<*xf32>, tensor<*x!tf.resource>, tensor<!tf.resource>, tensor<?xi32>
  }

  // CHECK-LABEL: func @while_shape_invariant_cond_func_propagate
  // CHECK-SAME: ({{%.+}}: tensor<*xf32>, {{%.+}}: tensor<*x!tf.resource<tensor<4xf32>>>, {{%.+}}: tensor<!tf.resource<tensor<8xf32>>>, {{%.+}}: tensor<?xi32>)
  // CHECK-SAME: -> tensor<i1>
  func @while_shape_invariant_cond_func_propagate(%arg0: tensor<*xf32>, %arg1: tensor<*x!tf.resource>, %arg2: tensor<!tf.resource>, %arg3: tensor<?xi32>) -> tensor<i1> {
    %0 = "tf.Const"() {value = dense<true> : tensor<i1>} : () -> tensor<i1>
    return %0 : tensor<i1>
  }

  // CHECK-LABEL: func @while_shape_invariant_body_func_propagate
  // CHECK-SAME: ({{%.+}}: tensor<*xf32>, {{%.+}}: tensor<*x!tf.resource<tensor<4xf32>>>, {{%.+}}: tensor<!tf.resource<tensor<8xf32>>>, {{%.+}}: tensor<?xi32>)
  // CHECK-SAME: -> (tensor<*xf32>, tensor<*x!tf.resource<tensor<4xf32>>>, tensor<!tf.resource<tensor<8xf32>>>, tensor<?xi32>)
  func @while_shape_invariant_body_func_propagate(%arg0: tensor<*xf32>, %arg1: tensor<*x!tf.resource>, %arg2: tensor<!tf.resource>, %arg3: tensor<?xi32>) -> (tensor<*xf32>, tensor<*x!tf.resource>, tensor<!tf.resource>, tensor<?xi32>) {
    %0 = "tf.SomeOp"(%arg3) : (tensor<?xi32>) -> tensor<?xi32>
    return %arg0, %arg1, %arg2, %0 : tensor<*xf32>, tensor<*x!tf.resource>, tensor<!tf.resource>, tensor<?xi32>
  }

  // Test shape invariant While replaces different dimensions with a dynamic
  // dimension when creating a shape for refining cond and body.
  // CHECK-LABEL: func @while_shape_invariant_different_dims
  // CHECK-SAME: ({{%.+}}: tensor<1x2x3xf32>)
  // CHECK-SAME: -> (tensor<1x8x3xf32>, tensor<1x8x3xf32>)
  func @while_shape_invariant_different_dims(%arg0: tensor<1x2x3xf32>) -> (tensor<1x8x3xf32>, tensor<1x8x3xf32>) {
    // CHECK: "tf.While"
    // CHECK-SAME: (tensor<1x2x3xf32>)
    // CHECK-SAME: -> tensor<1x8x3xf32>
    %0 = "tf.While"(%arg0) {cond = @while_shape_invariant_cond_func_different_dims, body = @while_shape_invariant_body_func_different_dims, is_stateless = false, shape_invariant} : (tensor<1x2x3xf32>) -> tensor<1x8x3xf32>

    // CHECK: "tf.WhileRegion"
    %1 = "tf.WhileRegion"(%arg0) ( {
    // CHECK-NEXT: ^{{.+}}({{%.+}}: tensor<1x?x3xf32>):
    ^cond(%carg0: tensor<*xf32>):
      %2 = "tf.Const"() {value = dense<true> : tensor<i1>} : () -> tensor<i1>
      "tf.Yield"(%2) : (tensor<i1>) -> ()
    }, {
    // CHECK: ^{{.+}}({{%.+}}: tensor<1x?x3xf32>):
    ^body(%barg0: tensor<*xf32>):
      %2 = "tf.Identity"(%barg0) : (tensor<*xf32>) -> tensor<*xf32>
      // CHECK: "tf.Yield"
      // CHECK-SAME: (tensor<1x?x3xf32>) -> ()
      "tf.Yield"(%2) : (tensor<*xf32>) -> ()
    // CHECK-NEXT: shape_invariant
    // CHECK-SAME: (tensor<1x2x3xf32>)
    // CHECK-SAME: -> tensor<1x8x3xf32>
    }) {is_stateless = false, shape_invariant} : (tensor<1x2x3xf32>) -> tensor<1x8x3xf32>

    return %0, %1 : tensor<1x8x3xf32>, tensor<1x8x3xf32>
  }

  // CHECK-LABEL: func @while_shape_invariant_cond_func_different_dims
  // CHECK-SAME: ({{%.+}}: tensor<1x?x3xf32>)
  // CHECK-SAME: -> tensor<i1>
  func @while_shape_invariant_cond_func_different_dims(%arg0: tensor<*xf32>) -> tensor<i1> {
    %0 = "tf.Const"() {value = dense<true> : tensor<i1>} : () -> tensor<i1>
    return %0 : tensor<i1>
  }

  // CHECK-LABEL: func @while_shape_invariant_body_func_different_dims
  // CHECK-SAME: ({{%.+}}: tensor<1x?x3xf32>)
  // CHECK-SAME: -> tensor<1x?x3xf32>
  func @while_shape_invariant_body_func_different_dims(%arg0: tensor<*xf32>) -> tensor<*xf32> {
    %0 = "tf.Identity"(%arg0) : (tensor<*xf32>) -> tensor<*xf32>
    return %0 : tensor<*xf32>
  }

  // Test shape invariant While can propagate handle type to result from body
  // result.
  // CHECK-LABEL: func @while_shape_invariant_body_result_propagate
  // CHECK-SAME: ({{%.+}}: tensor<*x!tf.resource<tensor<f32>>>)
  // CHECK-SAME: -> (tensor<*x!tf.resource<tensor<f32>>>, tensor<*x!tf.resource<tensor<f32>>>)
  func @while_shape_invariant_body_result_propagate(%arg0: tensor<*x!tf.resource<tensor<f32>>>) -> (tensor<*x!tf.resource>, tensor<*x!tf.resource>) {
    // CHECK: "tf.While"
    // CHECK-SAME: (tensor<*x!tf.resource<tensor<f32>>>)
    // CHECK-SAME: -> tensor<*x!tf.resource<tensor<f32>>>
    %0 = "tf.While"(%arg0) {cond = @while_shape_invariant_cond_func_body_result_propagate, body = @while_shape_invariant_body_func_body_result_propagate, is_stateless = false, shape_invariant} : (tensor<*x!tf.resource<tensor<f32>>>) -> tensor<*x!tf.resource>

    // CHECK: "tf.WhileRegion"
    %1 = "tf.WhileRegion"(%arg0) ( {
    // CHECK-NEXT: ^{{.+}}({{%.+}}: tensor<*x!tf.resource<tensor<f32>>>):
    ^cond(%carg0: tensor<*x!tf.resource<tensor<f32>>>):
      %2 = "tf.Const"() {value = dense<true> : tensor<i1>} : () -> tensor<i1>
      "tf.Yield"(%2) : (tensor<i1>) -> ()
    }, {
    // CHECK: ^{{.+}}({{%.+}}: tensor<*x!tf.resource<tensor<f32>>>):
    ^body(%barg0: tensor<*x!tf.resource<tensor<f32>>>):
      %2 = "tf.Identity"(%barg0) : (tensor<*x!tf.resource<tensor<f32>>>) -> tensor<*x!tf.resource<tensor<f32>>>
      // CHECK: "tf.Yield"
      // CHECK-SAME: (tensor<*x!tf.resource<tensor<f32>>>) -> ()
      "tf.Yield"(%2) : (tensor<*x!tf.resource<tensor<f32>>>) -> ()
    // CHECK-NEXT: shape_invariant
    // CHECK-SAME: (tensor<*x!tf.resource<tensor<f32>>>)
    // CHECK-SAME: -> tensor<*x!tf.resource<tensor<f32>>>
    }) {is_stateless = false, shape_invariant} : (tensor<*x!tf.resource<tensor<f32>>>) -> tensor<*x!tf.resource>

    return %0, %1 : tensor<*x!tf.resource>, tensor<*x!tf.resource>
  }

  // CHECK-LABEL: func @while_shape_invariant_cond_func_body_result_propagate
  // CHECK-SAME: ({{%.+}}: tensor<*x!tf.resource<tensor<f32>>>)
  // CHECK-SAME: -> tensor<i1>
  func @while_shape_invariant_cond_func_body_result_propagate(%arg0: tensor<*x!tf.resource<tensor<f32>>>) -> tensor<i1> {
    %0 = "tf.Const"() {value = dense<true> : tensor<i1>} : () -> tensor<i1>
    return %0 : tensor<i1>
  }

  // CHECK-LABEL: func @while_shape_invariant_body_func_body_result_propagate
  // CHECK-SAME: ({{%.+}}: tensor<*x!tf.resource<tensor<f32>>>)
  // CHECK-SAME: -> tensor<*x!tf.resource<tensor<f32>>>
  func @while_shape_invariant_body_func_body_result_propagate(%arg0: tensor<*x!tf.resource<tensor<f32>>>) -> tensor<*x!tf.resource<tensor<f32>>> {
    %0 = "tf.Identity"(%arg0) : (tensor<*x!tf.resource<tensor<f32>>>) -> tensor<*x!tf.resource<tensor<f32>>>
    return %0 : tensor<*x!tf.resource<tensor<f32>>>
  }

  // CHECK-LABEL: func @InferFromValueFolding
  func @InferFromValueFolding(%arg0 : tensor<f32>) -> tensor<*xf32> {
    %cst1 = "tf.Const"() {value = dense<1.000000e+00> : tensor<f32>} : () -> tensor<f32>
    %mul = "tf.Mul"(%arg0, %arg0) : (tensor<f32>, tensor<f32>) -> tensor<f32>
    // Folding will infer that: Pow(%mul, 1.0) -> %mul
    // However we don't have the actual value for the mul, but we can use the
    // mul type!
    // CHECK: tf.Pow
    // CHECK-SAME: -> tensor<f32>
    %pow = "tf.Pow"(%mul, %cst1) : (tensor<f32>, tensor<f32>) -> tensor<*xf32>
    return %pow : tensor<*xf32>
  }

  // Same as above, but don't infer when the type is "less" static.
  // CHECK-LABEL: func @DontInferFromValueFolding
  func @DontInferFromValueFolding(%arg0 : tensor<*xf32>) -> tensor<f32> {
    %cst1 = "tf.Const"() {value = dense<1.000000e+00> : tensor<f32>} : () -> tensor<f32>
    %mul = "tf.Mul"(%arg0, %arg0) : (tensor<*xf32>, tensor<*xf32>) -> tensor<*xf32>
    // Folding will infer that: Pow(%mul, 1.0) -> %mul
    // However we don't want to use the type of the mul as the type is less
    // static, it'd be lossy.
    // CHECK: tf.Pow
    // CHECK-SAME: -> tensor<f32>
    %pow = "tf.Pow"(%mul, %cst1) : (tensor<*xf32>, tensor<f32>) -> tensor<f32>
    return %pow : tensor<f32>
  }


  // Test propagation of multiple callers into a function when all the callers
  // have the same operand types.

  // CHECK-LABEL: func @multi_caller1
  func @multi_caller1(%arg0: tensor<i32>) -> tensor<*xi32> {
    // CHECK: tf.PartitionedCall
    // CHECK-SAME:  -> tensor<i32>
    %0 = "tf.PartitionedCall"(%arg0) {config = "", config_proto = "", executor_type = "", f = @multi_caller_callee} : (tensor<i32>) -> (tensor<*xi32>)
    return %0 : tensor<*xi32>
  }
  // CHECK-LABEL: func @multi_caller2
  func @multi_caller2(%arg0: tensor<i32>) -> tensor<*xi32> {
    // CHECK: tf.PartitionedCall
    // CHECK-SAME:  -> tensor<i32>
    %0 = "tf.PartitionedCall"(%arg0) {config = "", config_proto = "", executor_type = "", f = @multi_caller_callee} : (tensor<i32>) -> (tensor<*xi32>)
    return %0 : tensor<*xi32>
  }

  // CHECK-LABEL: func private @multi_caller_callee
  // CHECK-SAME: (%arg0: tensor<i32>) -> tensor<i32>
  func private @multi_caller_callee(%arg0: tensor<*xi32>) -> tensor<*xi32> {
    // CHECK: return
    // CHECK-SAME: tensor<i32>
    return %arg0 : tensor<*xi32>
  }

  // Test conv2d inferReturnTypes can infer some information when input or
  // filter does not have fully static shape.

  // CHECK-LABEL: func @conv2d_unranked_input_and_filter
  func @conv2d_unranked_input_and_filter(%arg0: tensor<*xf32>, %arg1: tensor<*xf32>) -> tensor<*xf32> {
    // CHECK: "tf.Conv2D"
    // CHECK-SAME: -> tensor<?x?x?x?xf32>
    %0 = "tf.Conv2D"(%arg0, %arg1) {padding = "SAME", strides = [1, 1, 1, 1]} : (tensor<*xf32>, tensor<*xf32>) -> tensor<*xf32>
    return %0 : tensor<*xf32>
  }

  // CHECK-LABEL: func @conv2d_unranked_filter
  func @conv2d_unranked_filter(%arg0: tensor<256x32x32x3xf32>, %arg1: tensor<*xf32>) -> tensor<*xf32> {
    // CHECK: "tf.Conv2D"
    // CHECK-SAME: -> tensor<256x?x?x?xf32>
    %0 = "tf.Conv2D"(%arg0, %arg1) {padding = "SAME", strides = [1, 1, 1, 1]} : (tensor<256x32x32x3xf32>, tensor<*xf32>) -> tensor<*xf32>
    return %0 : tensor<*xf32>
  }

  // CHECK-LABEL: func @conv2d_unranked_filter_and_dynamic_batch
  func @conv2d_unranked_filter_and_dynamic_batch(%arg0: tensor<?x32x32x3xf32>, %arg1: tensor<*xf32>) -> tensor<*xf32> {
    // CHECK: "tf.Conv2D"
    // CHECK-SAME: -> tensor<?x?x?x?xf32>
    %0 = "tf.Conv2D"(%arg0, %arg1) {padding = "SAME", strides = [1, 1, 1, 1]} : (tensor<?x32x32x3xf32>, tensor<*xf32>) -> tensor<*xf32>
    return %0 : tensor<*xf32>
  }

  // CHECK-LABEL: func @conv2d_unranked_input
  func @conv2d_unranked_input(%arg0: tensor<*xf32>, %arg1: tensor<3x3x3x16xf32>) -> tensor<*xf32> {
    // CHECK: "tf.Conv2D"
    // CHECK-SAME: -> tensor<?x?x?x16xf32>
    %0 = "tf.Conv2D"(%arg0, %arg1) {padding = "SAME", strides = [1, 1, 1, 1]} : (tensor<*xf32>, tensor<3x3x3x16xf32>) -> tensor<*xf32>
    return %0 : tensor<*xf32>
  }

  // CHECK-LABEL: func @conv2d_unranked_input_and_dynamic_channel
  func @conv2d_unranked_input_and_dynamic_channel(%arg0: tensor<*xf32>, %arg1: tensor<3x3x3x?xf32>) -> tensor<*xf32> {
    // CHECK: "tf.Conv2D"
    // CHECK-SAME: -> tensor<?x?x?x?xf32>
    %0 = "tf.Conv2D"(%arg0, %arg1) {padding = "SAME", strides = [1, 1, 1, 1]} : (tensor<*xf32>, tensor<3x3x3x?xf32>) -> tensor<*xf32>
    return %0 : tensor<*xf32>
  }

  // CHECK-LABEL: func @conv2d_dynamic_batch
  func @conv2d_dynamic_batch(%arg0: tensor<?x32x32x3xf32>, %arg1: tensor<3x3x3x16xf32>) -> tensor<*xf32> {
    // CHECK: "tf.Conv2D"
    // CHECK-SAME: -> tensor<?x32x32x16xf32>
    %0 = "tf.Conv2D"(%arg0, %arg1) {padding = "SAME", strides = [1, 1, 1, 1]} : (tensor<?x32x32x3xf32>, tensor<3x3x3x16xf32>) -> tensor<*xf32>
    return %0 : tensor<*xf32>
  }

  // CHECK-LABEL: func @conv2d_dynamic_channel
  func @conv2d_dynamic_channel(%arg0: tensor<256x32x32x3xf32>, %arg1: tensor<3x3x3x?xf32>) -> tensor<*xf32> {
    // CHECK: "tf.Conv2D"
    // CHECK-SAME: -> tensor<256x32x32x?xf32>
    %0 = "tf.Conv2D"(%arg0, %arg1) {padding = "SAME", strides = [1, 1, 1, 1]} : (tensor<256x32x32x3xf32>, tensor<3x3x3x?xf32>) -> tensor<*xf32>
    return %0 : tensor<*xf32>
  }

  // CHECK-LABEL: func @conv2d_fully_dynamic_spatial_dim
  func @conv2d_fully_dynamic_spatial_dim(%arg0: tensor<256x?x?x3xf32>, %arg1: tensor<3x3x3x16xf32>) -> tensor<*xf32> {
    // CHECK: "tf.Conv2D"
    // CHECK-SAME: -> tensor<256x?x?x16xf32>
    %0 = "tf.Conv2D"(%arg0, %arg1) {padding = "SAME", strides = [1, 1, 1, 1]} : (tensor<256x?x?x3xf32>, tensor<3x3x3x16xf32>) -> tensor<*xf32>
    return %0 : tensor<*xf32>
  }

  // CHECK-LABEL: func @conv2d_partially_dynamic_spatial_dim
  func @conv2d_partially_dynamic_spatial_dim(%arg0: tensor<256x?x32x3xf32>, %arg1: tensor<3x3x3x16xf32>) -> tensor<*xf32> {
    // CHECK: "tf.Conv2D"
    // CHECK-SAME: -> tensor<256x?x32x16xf32>
    %0 = "tf.Conv2D"(%arg0, %arg1) {padding = "SAME", strides = [1, 1, 1, 1]} : (tensor<256x?x32x3xf32>, tensor<3x3x3x16xf32>) -> tensor<*xf32>
    return %0 : tensor<*xf32>
  }

  // CHECK-LABEL: func @conv2d_dynamic_batch_and_partially_dynamic_spatial_dim
  func @conv2d_dynamic_batch_and_partially_dynamic_spatial_dim(%arg0: tensor<?x?x32x3xf32>, %arg1: tensor<3x3x3x16xf32>) -> tensor<*xf32> {
    // CHECK: "tf.Conv2D"
    // CHECK-SAME: -> tensor<?x?x32x16xf32>
    %0 = "tf.Conv2D"(%arg0, %arg1) {padding = "SAME", strides = [1, 1, 1, 1]} : (tensor<?x?x32x3xf32>, tensor<3x3x3x16xf32>) -> tensor<*xf32>
    return %0 : tensor<*xf32>
  }

  // CHECK-LABEL: func @conv2d_dynamic_batch_and_fully_dynamic_spatial_dim
  func @conv2d_dynamic_batch_and_fully_dynamic_spatial_dim(%arg0: tensor<?x?x?x3xf32>, %arg1: tensor<3x3x3x16xf32>) -> tensor<*xf32> {
    // CHECK: "tf.Conv2D"
    // CHECK-SAME: -> tensor<?x?x?x16xf32>
    %0 = "tf.Conv2D"(%arg0, %arg1) {padding = "SAME", strides = [1, 1, 1, 1]} : (tensor<?x?x?x3xf32>, tensor<3x3x3x16xf32>) -> tensor<*xf32>
    return %0 : tensor<*xf32>
  }

  // CHECK-LABEL: check_walking_identity
  func @check_walking_identity(%arg0 : tensor<1x192x256x128xf32>) {
    %0 = "tf.Const"() {value = dense<2> : tensor<2xi32>} : () -> tensor<2xi32>
    %1 = "tf.Const"() {value = dense<2> : tensor<2x2xi32>} : () -> tensor<2x2xi32>
    %2 = "tf.Identity"(%1) {device = ""} : (tensor<2x2xi32>) -> tensor<2x2xi32>
    // CHECK: SpaceToBatchND{{.*}}-> tensor<4x98x130x128xf32>
    %3 = "tf.SpaceToBatchND"(%arg0, %0, %2) {device = ""} : (tensor<1x192x256x128xf32>, tensor<2xi32>, tensor<2x2xi32>) -> tensor<4x?x?x128xf32>
    return
  }
}
