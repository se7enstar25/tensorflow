# Copyright 2018 The TensorFlow Authors. All Rights Reserved.
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
"""Function for interpolating formatted errors from the TensorFlow runtime.

Exposes the function `interpolate` to interpolate messages with tags of the form
^^type:name:format^^.
"""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import collections
import itertools
import os
import re
import string

import six

from tensorflow.python.util import tf_stack


_NAME_REGEX = r"[A-Za-z0-9.][A-Za-z0-9_.\-/]*?"
_FORMAT_REGEX = r"[A-Za-z0-9_.\-/${}:]+"
_TAG_REGEX = r"\^\^({name}):({name}):({fmt})\^\^".format(
    name=_NAME_REGEX, fmt=_FORMAT_REGEX)
_INTERPOLATION_REGEX = r"^(.*?)({tag})".format(tag=_TAG_REGEX)
_INTERPOLATION_PATTERN = re.compile(_INTERPOLATION_REGEX)

_ParseTag = collections.namedtuple("_ParseTag", ["type", "name", "format"])

_BAD_FILE_SUBSTRINGS = [
    os.path.join("tensorflow", "python"),
    "<embedded",
]


def _parse_message(message):
  """Parses the message.

  Splits the message into separators and tags. Tags are named tuples
  representing the string ^^type:name:format^^ and they are separated by
  separators. For example, in
  "123^^node:Foo:${file}^^456^^node:Bar:${line}^^789", there are two tags and
  three separators. The separators are the numeric characters.

  Supported tags after node:<node_name>
    file: Replaced with the filename in which the node was defined.
    line: Replaced by the line number at which the node was defined.

  Args:
    message: String to parse

  Returns:
    (list of separator strings, list of _ParseTags).

    For example, if message is "123^^node:Foo:${file}^^456" then this function
    returns (["123", "456"], [_ParseTag("node", "Foo", "${file}")])
  """
  seps = []
  tags = []
  pos = 0
  while pos < len(message):
    match = re.match(_INTERPOLATION_PATTERN, message[pos:])
    if match:
      seps.append(match.group(1))
      tags.append(_ParseTag(match.group(3), match.group(4), match.group(5)))
      pos += match.end()
    else:
      break
  seps.append(message[pos:])
  return seps, tags


def _get_field_dict_from_traceback(tf_traceback, frame_index):
  """Convert traceback elements into interpolation dictionary and return."""
  frame = tf_traceback[frame_index]
  return {
      "file": frame[tf_stack.TB_FILENAME],
      "line": frame[tf_stack.TB_LINENO],
  }


def _find_index_of_defining_frame_for_op(op):
  """Return index in op._traceback with first 'useful' frame.

  This method reads through the stack stored in op._traceback looking for the
  innermost frame which (hopefully) belongs to the caller.  It accomplishes this
  by rejecting frames whose filename appears to come from TensorFlow (see
  error_interpolation._BAD_FILE_SUBSTRINGS for the list of rejected substrings).

  Args:
    op: the Operation object for which we would like to find the defining
        location.

  Returns:
    Integer index into op._traceback where the first non-TF file was found
    (innermost to outermost), or 0 (for the outermost stack frame) if all files
    came from TensorFlow.
  """
  # pylint: disable=protected-access
  # Index 0 of tf_traceback is the outermost frame.
  tf_traceback = tf_stack.convert_stack(op._traceback)
  size = len(tf_traceback)
  # pylint: enable=protected-access
  filenames = [frame[tf_stack.TB_FILENAME] for frame in tf_traceback]
  # We process the filenames from the innermost frame to outermost.
  for idx, filename in enumerate(reversed(filenames)):
    contains_bad_substrings = [ss in filename for ss in _BAD_FILE_SUBSTRINGS]
    if not any(contains_bad_substrings):
      return size - idx - 1
  return 0


def interpolate(error_message, graph):
  """Interpolates an error message.

  The error message can contain tags of the form ^^type:name:format^^ which will
  be replaced.

  Args:
    error_message: A string to interpolate.
    graph: ops.Graph object containing all nodes referenced in the error
        message.

  Returns:
    The string with tags of the form ^^type:name:format^^ interpolated.
  """
  seps, tags = _parse_message(error_message)

  node_name_to_substitution_dict = {}
  for name in [t.name for t in tags]:
    try:
      op = graph.get_operation_by_name(name)
    except KeyError:
      op = None

    if op:
      frame_index = _find_index_of_defining_frame_for_op(op)
      # pylint: disable=protected-access
      field_dict = _get_field_dict_from_traceback(op._traceback, frame_index)
      # pylint: enable=protected-access
    else:
      field_dict = {
          "file": "<NA>",
          "line": "<NA>",
          "func": "<NA>",
          "code": None,
      }
    node_name_to_substitution_dict[name] = field_dict

  subs = [
      string.Template(tag.format).safe_substitute(
          node_name_to_substitution_dict[tag.name]) for tag in tags
  ]
  return "".join(
      itertools.chain(*six.moves.zip_longest(seps, subs, fillvalue="")))
