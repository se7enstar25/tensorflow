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
# =============================================================================
"""Generates and prints out imports and constants for new TensorFlow python api.
"""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import argparse
import collections
import importlib
import os
import sys

from tensorflow.python.util import tf_decorator


_API_CONSTANTS_ATTR = '_tf_api_constants'
_API_NAMES_ATTR = '_tf_api_names'
_DEFAULT_PACKAGE = 'tensorflow.python'
_GENFILES_DIR_SUFFIX = 'genfiles/'
_SYMBOLS_TO_SKIP_EXPLICITLY = {
    # Overrides __getattr__, so that unwrapping tf_decorator
    # would have side effects.
    'tensorflow.python.platform.flags.FLAGS'
}
_GENERATED_FILE_HEADER = """\"\"\"Imports for Python API.

This file is MACHINE GENERATED! Do not edit.
Generated by: tensorflow/tools/api/generator/create_python_api.py script.
\"\"\"
"""


class SymbolExposedTwiceError(Exception):
  """Raised when different symbols are exported with the same name."""
  pass


def format_import(source_module_name, source_name, dest_name):
  """Formats import statement.

  Args:
    source_module_name: (string) Source module to import from.
    source_name: (string) Source symbol name to import.
    dest_name: (string) Destination alias name.

  Returns:
    An import statement string.
  """
  if source_module_name:
    if source_name == dest_name:
      return 'from %s import %s' % (source_module_name, source_name)
    else:
      return 'from %s import %s as %s' % (
          source_module_name, source_name, dest_name)
  else:
    if source_name == dest_name:
      return 'import %s' % source_name
    else:
      return 'import %s as %s' % (source_name, dest_name)


class _ModuleInitCodeBuilder(object):
  """Builds a map from module name to imports included in that module."""

  def __init__(self):
    self.module_imports = collections.defaultdict(
        lambda: collections.defaultdict(set))
    self._dest_import_to_id = collections.defaultdict(int)
    # Names that start with underscore in the root module.
    self._underscore_names_in_root = []

  def add_import(
      self, symbol_id, dest_module_name, source_module_name, source_name,
      dest_name):
    """Adds this import to module_imports.

    Args:
      symbol_id: (number) Unique identifier of the symbol to import.
      dest_module_name: (string) Module name to add import to.
      source_module_name: (string) Module to import from.
      source_name: (string) Name of the symbol to import.
      dest_name: (string) Import the symbol using this name.

    Raises:
      SymbolExposedTwiceError: Raised when an import with the same
        dest_name has already been added to dest_module_name.
    """
    import_str = format_import(source_module_name, source_name, dest_name)

    # Check if we are trying to expose two different symbols with same name.
    full_api_name = dest_name
    if dest_module_name:
      full_api_name = dest_module_name + '.' + full_api_name
    if (full_api_name in self._dest_import_to_id and
        symbol_id != self._dest_import_to_id[full_api_name] and
        symbol_id != -1):
      raise SymbolExposedTwiceError(
          'Trying to export multiple symbols with same name: %s.' %
          full_api_name)
    self._dest_import_to_id[full_api_name] = symbol_id

    if not dest_module_name and dest_name.startswith('_'):
      self._underscore_names_in_root.append(dest_name)

    # The same symbol can be available in multiple modules.
    # We store all possible ways of importing this symbol and later pick just
    # one.
    self.module_imports[dest_module_name][full_api_name].add(import_str)

  def build(self):
    """Get a map from destination module to __init__.py code for that module.

    Returns:
      A dictionary where
        key: (string) destination module (for e.g. tf or tf.consts).
        value: (string) text that should be in __init__.py files for
          corresponding modules.
    """
    module_text_map = {}
    for dest_module, dest_name_to_imports in self.module_imports.items():
      # Sort all possible imports for a symbol and pick the first one.
      imports_list = [
          sorted(imports)[0]
          for _, imports in dest_name_to_imports.items()]
      module_text_map[dest_module] = '\n'.join(sorted(imports_list))

    # Expose exported symbols with underscores in root module
    # since we import from it using * import.
    underscore_names_str = ', '.join(
        '\'%s\'' % name for name in self._underscore_names_in_root)
    # We will always generate a root __init__.py file to let us handle *
    # imports consistently. Be sure to have a root __init__.py file listed in
    # the script outputs.
    module_text_map[''] = module_text_map.get('', '') + '''
_names_with_underscore = [%s]
__all__ = [_s for _s in dir() if not _s.startswith('_')]
__all__.extend([_s for _s in _names_with_underscore])
''' % underscore_names_str

    return module_text_map


def get_api_init_text(package):
  """Get a map from destination module to __init__.py code for that module.

  Args:
    package: Base python package containing python with target tf_export
      decorators.

  Returns:
    A dictionary where
      key: (string) destination module (for e.g. tf or tf.consts).
      value: (string) text that should be in __init__.py files for
        corresponding modules.
  """
  module_code_builder = _ModuleInitCodeBuilder()

  # Traverse over everything imported above. Specifically,
  # we want to traverse over TensorFlow Python modules.
  for module in list(sys.modules.values()):
    # Only look at tensorflow modules.
    if (not module or not hasattr(module, '__name__') or
        package not in module.__name__):
      continue
    # Do not generate __init__.py files for contrib modules for now.
    if '.contrib.' in module.__name__ or module.__name__.endswith('.contrib'):
      continue

    for module_contents_name in dir(module):
      if (module.__name__ + '.' + module_contents_name
          in _SYMBOLS_TO_SKIP_EXPLICITLY):
        continue
      attr = getattr(module, module_contents_name)

      # If attr is _tf_api_constants attribute, then add the constants.
      if module_contents_name == _API_CONSTANTS_ATTR:
        for exports, value in attr:
          for export in exports:
            names = export.split('.')
            dest_module = '.'.join(names[:-1])
            module_code_builder.add_import(
                -1, dest_module, module.__name__, value, names[-1])
        continue

      try:
        _, attr = tf_decorator.unwrap(attr)
      except Exception as e:
        print('5555: %s %s' % (module, module_contents_name), file=sys.stderr)
        raise e
      # If attr is a symbol with _tf_api_names attribute, then
      # add import for it.
      if hasattr(attr, '__dict__') and _API_NAMES_ATTR in attr.__dict__:
        for export in attr._tf_api_names:  # pylint: disable=protected-access
          names = export.split('.')
          dest_module = '.'.join(names[:-1])
          module_code_builder.add_import(
              id(attr), dest_module, module.__name__, module_contents_name,
              names[-1])

  # Import all required modules in their parent modules.
  # For e.g. if we import 'foo.bar.Value'. Then, we also
  # import 'bar' in 'foo'.
  imported_modules = set(module_code_builder.module_imports.keys())
  import_from = '.'
  for module in imported_modules:
    if not module:
      continue
    module_split = module.split('.')
    parent_module = ''  # we import submodules in their parent_module

    for submodule_index in range(len(module_split)):
      if submodule_index > 0:
        parent_module += ('.' + module_split[submodule_index-1] if parent_module
                          else module_split[submodule_index-1])
      module_code_builder.add_import(
          -1, parent_module, import_from,
          module_split[submodule_index], module_split[submodule_index])

  return module_code_builder.build()


def get_module(dir_path, relative_to_dir):
  """Get module that corresponds to path relative to relative_to_dir.

  Args:
    dir_path: Path to directory.
    relative_to_dir: Get module relative to this directory.

  Returns:
    module that corresponds to the given directory.
  """
  dir_path = dir_path[len(relative_to_dir):]
  # Convert path separators to '/' for easier parsing below.
  dir_path = dir_path.replace(os.sep, '/')
  return dir_path.replace('/', '.').strip('.')


def create_api_files(
    output_files, package, root_init_template, output_dir):
  """Creates __init__.py files for the Python API.

  Args:
    output_files: List of __init__.py file paths to create.
      Each file must be under api/ directory.
    package: Base python package containing python with target tf_export
      decorators.
    root_init_template: Template for top-level __init__.py file.
      "#API IMPORTS PLACEHOLDER" comment in the template file will be replaced
      with imports.
    output_dir: output API root directory.

  Raises:
    ValueError: if an output file is not under api/ directory,
      or output_files list is missing a required file.
  """
  module_name_to_file_path = {}
  for output_file in output_files:
    module_name = get_module(os.path.dirname(output_file), output_dir)
    module_name_to_file_path[module_name] = os.path.normpath(output_file)

  # Create file for each expected output in genrule.
  for module, file_path in module_name_to_file_path.items():
    if not os.path.isdir(os.path.dirname(file_path)):
      os.makedirs(os.path.dirname(file_path))
    open(file_path, 'a').close()

  module_text_map = get_api_init_text(package)

  # Add imports to output files.
  missing_output_files = []
  for module, text in module_text_map.items():
    # Make sure genrule output file list is in sync with API exports.
    if module not in module_name_to_file_path:
      module_file_path = '"%s/__init__.py"' %  (
          module.replace('.', '/'))
      missing_output_files.append(module_file_path)
      continue
    contents = ''
    if module or not root_init_template:
      contents = _GENERATED_FILE_HEADER + text
    else:
      # Read base init file
      with open(root_init_template, 'r') as root_init_template_file:
        contents = root_init_template_file.read()
        contents = contents.replace('# API IMPORTS PLACEHOLDER', text)
    with open(module_name_to_file_path[module], 'w') as fp:
      fp.write(contents)

  if missing_output_files:
    raise ValueError(
        'Missing outputs for python_api_gen genrule:\n%s.'
        'Make sure all required outputs are in the '
        'tensorflow/tools/api/generator/BUILD file.' %
        ',\n'.join(sorted(missing_output_files)))


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      'outputs', metavar='O', type=str, nargs='+',
      help='If a single file is passed in, then we we assume it contains a '
      'semicolon-separated list of Python files that we expect this script to '
      'output. If multiple files are passed in, then we assume output files '
      'are listed directly as arguments.')
  parser.add_argument(
      '--package', default=_DEFAULT_PACKAGE, type=str,
      help='Base package that imports modules containing the target tf_export '
           'decorators.')
  parser.add_argument(
      '--root_init_template', default='', type=str,
      help='Template for top level __init__.py file. '
           '"#API IMPORTS PLACEHOLDER" comment will be replaced with imports.')
  parser.add_argument(
      '--apidir', type=str, required=True,
      help='Directory where generated output files are placed. '
           'gendir should be a prefix of apidir. Also, apidir '
           'should be a prefix of every directory in outputs.')

  args = parser.parse_args()

  if len(args.outputs) == 1:
    # If we only get a single argument, then it must be a file containing
    # list of outputs.
    with open(args.outputs[0]) as output_list_file:
      outputs = [line.strip() for line in output_list_file.read().split(';')]
  else:
    outputs = args.outputs

  # Populate `sys.modules` with modules containing tf_export().
  importlib.import_module(args.package)
  create_api_files(
      outputs, args.package, args.root_init_template, args.apidir)


if __name__ == '__main__':
  main()
