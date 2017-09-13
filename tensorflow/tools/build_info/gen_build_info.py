#!/usr/bin/env python
# Copyright 2017 The TensorFlow Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ==============================================================================
"""Generates a Python module containing information about the build."""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
import argparse


def write_build_info(filename, build_config):
  """Writes a Python that describes the build.

  Args:
    filename: filename to write to.
    build_config: A string containinggit_version: the result of a git describe.
  """
  module_docstring = "\"\"\"Generates a Python module containing information "
  module_docstring += "about the build.\"\"\""
  if build_config == "cuda":
    build_config_bool = "True"
  else:
    build_config_bool = "False"

  contents = """
# Copyright 2017 The TensorFlow Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ==============================================================================
%s
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

is_cuda_build = %s
""" % (module_docstring, build_config_bool)
  open(filename, "w").write(contents)


parser = argparse.ArgumentParser(
    description="""Build info injection into the PIP package.""")

parser.add_argument(
    "--build_config",
    type=str,
    help="Either 'cuda' for GPU builds or 'cpu' for CPU builds.")

parser.add_argument("--raw_generate", type=str, help="Generate build_info.py")

args = parser.parse_args()

if args.raw_generate is not None and args.build_config is not None:
  write_build_info(args.raw_generate, args.build_config)
else:
  raise RuntimeError("--raw_generate and --build_config must be used")
