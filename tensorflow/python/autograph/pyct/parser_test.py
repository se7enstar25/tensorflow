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
"""Tests for parser module."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

from tensorflow.python.autograph.pyct import parser
from tensorflow.python.platform import test


class ParserTest(test.TestCase):

  def test_parse_entity(self):

    def f(x):
      return x + 1

    node, _, _ = parser.parse_entity(f, future_imports=())
    self.assertEqual('f', node.name)

  def test_parse_entity_print_function(self):
    def f(x):
      print(x)
    node, _, _ = parser.parse_entity(
        f, future_imports=['print_function'])
    self.assertEqual('f', node.name)

  def test_parse_comments(self):
    def f():
# unindented comment
      pass
    with self.assertRaises(ValueError):
      parser.parse_entity(f, future_imports=())

  def test_parse_multiline_strings(self):
    def f():
      print("""
some
multiline
string""")
    with self.assertRaises(ValueError):
      parser.parse_entity(f, future_imports=())

  def test_parse_expression(self):
    node = parser.parse_expression('a.b')
    self.assertEqual('a', node.value.id)
    self.assertEqual('b', node.attr)


if __name__ == '__main__':
  test.main()
