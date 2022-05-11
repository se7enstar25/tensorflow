/* Copyright 2022 The TensorFlow Authors. All Rights Reserved.

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

#ifndef TENSORFLOW_DTENSOR_CC_CONSTANTS_H_
#define TENSORFLOW_DTENSOR_CC_CONSTANTS_H_

namespace tensorflow {
namespace dtensor {
// Constants used within dtensor scope.

// Qualified attribute without `_` prefix.
// Used in Ops attribute registration.
static constexpr char kQualifiedLayoutAttr[] = "layout";

// Internal attribute to DTensor MLIR passes and Graph nodes.
// Prefixed with `_` so that it doesn't require op attribute registration.
static constexpr char kLayoutAttr[] = "_layout";

// Indicates a non-binding layout hint provided by the user.
// `tf` prefix attached in MLIR importer for dialect requirements.
static constexpr char kCustomDefaultLayoutAttr[] = "tf._default_layout";

// Indicates a non-binding layout hint provided by the user.
static constexpr char kDefaultLayoutAttr[] = "_default_layout";

// Attribute carries layout information from Custom Device Arguments.
// `tf` prefix attached in MLIR importer for dialect requirements.
static constexpr char kCustomDeviceAttr[] = "tf._layout";

// Attribute attached on _Arg node for the mesh config.
static constexpr char kMeshAttr[] = "_mesh";

// Attribute carries mesh information from Custom Device Arguments.
// `tf` prefix attached in MLIR importer for dialect requirements.
static constexpr char kCustomDeviceMeshAttr[] = "tf._mesh";

// Attribute carries argument indices for newly inferred layout of resource
// handle.
static constexpr char kNewResourceLayoutIndices[] =
    "_inferred_resource_indices";

// Attribute carries layout for newly inferred layout of resource handle.
static constexpr char kNewResourceArgLayouts[] = "_inferred_resource_layouts";

// Attribute carries input layout information for shape op.
static constexpr char kShapeOpInputLayout[] = "_shape_input_layout";

// Attribute carries input layout index for shape op. This forms a 1 -> 1
// mapping for kShapeOpInputLayout above.
static constexpr char kShapeOpInputLayoutIndices[] = "_shape_input_indices";

// Attribute that carries global shape of operation. Used to preserve global
// shape to be used during SPMD expansion.
static constexpr char kGlobalShape[] = "_global_shape";

// Global shape attribute with `tf.` dialect to be used for annotating func op
// arguments/return values.
static constexpr char kGlobalShapeDialectAttr[] = "tf._global_shape";

// Attribute attached to resource-type function arguments containing the local
// shape of the tensor that is being assigned to it.
static constexpr char kAssignedResourceLocalShape[] =
    "tf._assigned_resource_local_shape";

// Tensor handles smaller than this is considered as small tensor. We perform
// some optimizations around it. For example, will be transformed into constant
// values during graph building, instead of being passed as inputs. In addition,
// we allow automatical broadcasting small non-DTensor to DTensor device, which
// is very useful for shape/axis info tensor in eager mode (eliminating the need
// forcing users to do explicit copy-to-mesh).
static constexpr int kSmallTensorThreshold = 20;

// Contains a serialized mesh. Will be attached to a FloorMod op to denote which
// mesh the output of the FloorMod op is giving coordinates for.
static constexpr char kMeshCoordinatesAttr[] = "_mesh_coordinates";

// Attribute used to determine if a module pass should log long form information
// such as IR dumps etc.
static constexpr char kDoNotLog[] = "dtensor.do_not_log";

// The number of TPU cores in a donut.
static constexpr int kTpuDonutSize = 8;

// An attribute used to cache the computation of device seeds, so that we don't
// constantly recompute device seeds in a cluster for a given layout.
static constexpr char kDeviceSeedForMeshDims[] =
    "dtensor.device_seed_for_mesh_dims";

// Attribute that determines whether to skip XlA compilation. There are some ops
// that run on a TPU mesh but are not expected to be compiled by XLA, e.g.
// VarHandleOp, DestroyResourceOp, etc. For such an case, set this attribute
// to true on the StatefulPartitionedCallOp generated by MLIR lowering.
static constexpr char kSkipXlaCompilation[] = "_skip_xla_compilation";

// Prefix of pipelining mesh name (kPipelineMeshNamePrefix + composite device
// name).
static constexpr char kPipelineMeshNamePrefix[] = "pipe_cluster:";

// An attribute which stores the cache_key for the graph in the module. Used
// to uniquely name functions.
static constexpr char kCacheKey[] = "dtensor.cache_key";

// An attribute that determines whether a tensor is a sparse tensor. If this
// attribute exists in a tensor, then this tensor is a sparse tensor.
static constexpr char kSparseValue[] = "tf._sparse";

// TPUEmbedding configuration attribute with `tf.` dialect to be used for
// annotating func op that contains tpu embedding configuration ops.
static constexpr char kTPUEmbeddingConfiguration[] =
    "tf._tpu_embedding_configuration";

// Attribute mapping table_id to func op arguments using as TPUEmbedding tables
// `tf` prefix attached in MLIR importer for dialect requirements.
static constexpr char kTPUEmbeddingTableID[] = "tf._tpu_embedding_table_id";

// Name of dtensor load embedding function.
static constexpr char kLoadEmbeddingFn[] = "load_embedding_fn";
}  // namespace dtensor
}  // namespace tensorflow

#endif  // TENSORFLOW_DTENSOR_CC_CONSTANTS_H_
