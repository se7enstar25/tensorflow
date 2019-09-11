#!/bin/bash

# Copyright 2019 The MLIR Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Script for defining a new op using SPIR-V spec from the Internet.
#
# Run as:
# ./define_inst.sh <inst_category> (<opname>)*

# <inst_category> is required. It can be one of
# (Op|ArithmeticOp|LogicalOp|ControlFlowOp|StructureOp). Based on the
# inst_category the file SPIRV<inst_category>s.td is updated with the
# instruction definition. If <opname> is missing, this script updates existing
# ones in SPIRV<inst_category>s.td

# For example:
# ./define_inst.sh ArithmeticOp OpIAdd
# ./define_inst.sh LogicalOp OpFOrdEqual
set -e

inst_category=$1

case $inst_category in
  Op | ArithmeticOp | LogicalOp | ControlFlowOp | StructureOp)
  ;;
  *)
    echo "Usage : " $0 " <inst_category> (<opname>)*"
    echo "<inst_category> must be one of " \
      "(Op|ArithmeticOp|LogicalOp|ControlFlowOp|StructureOp)"
    exit 1;
  ;;
esac

shift

current_file="$(readlink -f "$0")"
current_dir="$(dirname "$current_file")"

python3 ${current_dir}/gen_spirv_dialect.py \
  --op-td-path \
  ${current_dir}/../../include/mlir/Dialect/SPIRV/SPIRV${inst_category}s.td \
  --inst-category $inst_category --new-inst "$@"
