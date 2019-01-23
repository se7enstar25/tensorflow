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
"""Type resolution.

This analyzer uses known live values to further infer object types. This
may include for instance constructed objects and object member functions.

In addition, the analyzer also handles user annotations made in the code (for
example, the autograph.set_element_type function).

Requires annotations generated by LiveValuesResolver.
"""

# TODO(mdan): This would be more robust with a CFG.
# Situations with multiple reaching modifications (e.g. modified inside and
# outside a control flow statement) should be more robustly detected and
# analyzed.

# TODO(mdan): Look into using Python AST's type annotation fields instead.
# It would be desirable to use that mechanism if we can.
# Some caveats to consider: We may need to annotate other nodes like
# Attribute. It may also not be feasible for us to faithfully to replicate
# PY3's type annotations where it isn't available. It would also require us
# to design rigorous type definitions that can accommodate Python types
# as well as TensorFLow dtypes and shapes.


from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import gast

from tensorflow.python.autograph.pyct import anno
from tensorflow.python.autograph.pyct import ast_util
from tensorflow.python.autograph.pyct import transformer
from tensorflow.python.util import tf_inspect


# TODO(mdan): Remove the duplication between this and activity.py.
# In particular, the symbol definitions we track here could as well be tracked
# there because they follow the same rules for visibility.
# TODO(mdan): Use a CFG based Defined analysis instead.
class Scope(object):
  """Tracks symbol value references.

  Attributes:
    values: A dict mapping string to gast.Node, containing the value that was
        most recently assigned to the symbol.
  """

  def __init__(self, parent):
    """Create a new scope.

    Args:
      parent: A Scope or None.
    """
    self.parent = parent
    self.values = {}

  def __repr__(self):
    return 'Scope[%s]' % self.values.keys()

  def copy(self):
    s = Scope(self.parent)
    s.values = self.values.copy()
    return s

  def setval(self, name, value):
    self.values[name] = value

  def hasval(self, name):
    return (name in self.values or
            (self.parent is not None and self.parent.hasval(name)))

  def getval(self, name):
    if name in self.values:
      return self.values[name]
    if self.parent is not None:
      return self.parent.getval(name)
    raise KeyError(name)


class TypeInfoResolver(transformer.Base):
  """Annotates symbols with type information where possible.

  Nodes currently annotated:
    * Call (helps detect class constructors)
    * Attribute (helps resolve object methods)
  """

  def __init__(self, context):
    super(TypeInfoResolver, self).__init__(context)
    self.scope = Scope(None)

  def visit_FunctionDef(self, node):
    self.scope = Scope(self.scope)
    node = self.generic_visit(node)
    self.scope = self.scope.parent
    return node

  def _visit_block(self, block):
    self.scope = Scope(self.scope)
    block = self.visit_block(block)
    self.scope = self.scope.parent
    return block

  def visit_For(self, node):
    self.generic_visit(node.target)
    self.generic_visit(node.iter)
    node.body = self._visit_block(node.body)
    node.orelse = self._visit_block(node.orelse)
    return node

  def visit_While(self, node):
    self.generic_visit(node.test)
    node.body = self._visit_block(node.body)
    node.orelse = self._visit_block(node.orelse)
    return node

  def visit_If(self, node):
    self.generic_visit(node.test)
    node.body = self._visit_block(node.body)
    node.orelse = self._visit_block(node.orelse)
    return node

  def _process_function_arg(self, arg_node):
    qn = anno.getanno(arg_node, anno.Basic.QN)
    arg_name = str(qn)
    self.scope.setval(qn, arg_node)
    if (len(self.enclosing_entities) == 1 and
        arg_name in self.transformer_ctx.entity_info.arg_types):
      # Forge a node to hold the type information, so that method calls on
      # it can resolve the type.
      type_string, type_obj = self.transformer_ctx.entity_info.arg_types[
          arg_name]
      anno.setanno(arg_node, 'type', type_obj)
      anno.setanno(arg_node, 'type_fqn', tuple(type_string.split('.')))

  def visit_arg(self, node):
    self._process_function_arg(node.arg)
    return node

  def visit_Name(self, node):
    self.generic_visit(node)
    if isinstance(node.ctx, gast.Param):
      self._process_function_arg(node)
    elif isinstance(node.ctx, gast.Load):
      qn = anno.getanno(node, anno.Basic.QN)
      if self.scope.hasval(qn):
        # E.g. if we had
        # a = b
        # then for future references to `a` we should have definition = `b`
        definition = self.scope.getval(qn)
        anno.copyanno(definition, node, 'type')
        anno.copyanno(definition, node, 'type_fqn')

        # TODO(mdan): Remove this when the directives module is in.
        anno.copyanno(definition, node, 'element_type')
        anno.copyanno(definition, node, 'element_shape')
    return node

  def _process_variable_assignment(self, target, value):
    # Constructors
    if isinstance(value, gast.Call):
      func = value.func
      if anno.hasanno(func, 'live_val'):
        func_obj = anno.getanno(func, 'live_val')
        if tf_inspect.isclass(func_obj):
          anno.setanno(value, 'is_constructor', True)
          anno.setanno(value, 'type', func_obj)
          anno.setanno(value, 'type_fqn', anno.getanno(func, 'fqn'))
          # TODO(mdan): Raise an error if constructor has side effects.
          # We can have a whitelist of no-side-effects constructors.
          # We can also step inside the constructor and further analyze.

    if isinstance(target, (gast.Name, gast.Attribute)):
      target_symbol = anno.getanno(target, anno.Basic.QN)
      self.scope.setval(target_symbol, value)
    elif isinstance(target, gast.Subscript):
      pass
    else:
      raise ValueError('assignment target has unknown type: %s' % target)

  def visit_With(self, node):
    for item in node.items:
      if item.optional_vars is not None:
        ast_util.apply_to_single_assignments((item.optional_vars,),
                                             item.context_expr,
                                             self._process_variable_assignment)
    self.generic_visit(node)
    return node

  def visit_Assign(self, node):
    self.generic_visit(node)
    ast_util.apply_to_single_assignments(node.targets, node.value,
                                         self._process_variable_assignment)
    return node


def resolve(node, context):
  return TypeInfoResolver(context).visit(node)
