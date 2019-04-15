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

from tensorflow.python.tools.api.generator import doc_srcs
from tensorflow.python.util import tf_decorator
from tensorflow.python.util import tf_export
from tensorflow.python.util import tf_inspect

API_ATTRS = tf_export.API_ATTRS
API_ATTRS_V1 = tf_export.API_ATTRS_V1

_API_VERSIONS = [1, 2]
_COMPAT_MODULE_TEMPLATE = 'compat.v%d'
_COMPAT_MODULE_PREFIX = 'compat.v'
_DEFAULT_PACKAGE = 'tensorflow.python'
_GENFILES_DIR_SUFFIX = 'genfiles/'
_SYMBOLS_TO_SKIP_EXPLICITLY = {
    # Overrides __getattr__, so that unwrapping tf_decorator
    # would have side effects.
    'tensorflow.python.platform.flags.FLAGS'
}
_GENERATED_FILE_HEADER = """# This file is MACHINE GENERATED! Do not edit.
# Generated by: tensorflow/python/tools/api/generator/create_python_api.py script.
\"\"\"%s
\"\"\"

from __future__ import print_function as _print_function

"""
_GENERATED_FILE_FOOTER = '\n\ndel _print_function\n'
_DEPRECATION_FOOTER = """
import sys as _sys
from tensorflow.python.util import deprecation_wrapper as _deprecation_wrapper

_DEPRECATED_TO_CANONICAL = {
%s
}

if not isinstance(_sys.modules[__name__], _deprecation_wrapper.DeprecationWrapper):
  _sys.modules[__name__] = _deprecation_wrapper.DeprecationWrapper(
      _sys.modules[__name__], "%s", _DEPRECATED_TO_CANONICAL)
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


def contains_deprecation_decorator(decorators):
  return any(
      d.decorator_name == 'deprecated' for d in decorators)


def has_deprecation_decorator(symbol, decorators):
  """Checks if given object has a deprecation decorator.

  We check if deprecation decorator is in decorators as well as
  whether symbol is a class whose __init__ method has a deprecation
  decorator.
  Args:
    symbol: Unwrapped (i.e. without decorators) Python object.
    decorators: Decorators originally wrapped around symbol.

  Returns:
    True if symbol has deprecation decorator.
  """
  if contains_deprecation_decorator(decorators):
    return True
  if tf_inspect.isfunction(symbol):
    return False
  if not tf_inspect.isclass(symbol):
    return False
  if not hasattr(symbol, '__init__'):
    return False
  init_decorators, _ = tf_decorator.unwrap(symbol.__init__)
  return contains_deprecation_decorator(init_decorators)


class _ModuleInitCodeBuilder(object):
  """Builds a map from module name to imports included in that module."""

  def __init__(self, output_package):
    self._output_package = output_package
    self._module_imports = collections.defaultdict(
        lambda: collections.defaultdict(set))
    self._deprecated_module_imports = collections.defaultdict(
        lambda: collections.defaultdict(set))
    # Maps deprecated names to canonical names for each module
    self._deprecation_to_canonical = collections.defaultdict(
        lambda: collections.defaultdict(dict))
    self._dest_import_to_id = collections.defaultdict(int)
    # Names that start with underscore in the root module.
    self._underscore_names_in_root = []

  def _check_already_imported(self, symbol_id, api_name):
    if (api_name in self._dest_import_to_id and
        symbol_id != self._dest_import_to_id[api_name] and
        symbol_id != -1):
      raise SymbolExposedTwiceError(
          'Trying to export multiple symbols with same name: %s.' %
          api_name)
    self._dest_import_to_id[api_name] = symbol_id

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
    self._check_already_imported(symbol_id, full_api_name)

    if not dest_module_name and dest_name.startswith('_'):
      self._underscore_names_in_root.append(dest_name)

    # The same symbol can be available in multiple modules.
    # We store all possible ways of importing this symbol and later pick just
    # one.
    self._module_imports[dest_module_name][full_api_name].add(import_str)

  def add_deprecated_endpoint(
      self, dest_module_name, dest_name, canonical_endpoint):
    """Adds deprecated alias to deprecated_module_imports.

    Args:
      dest_module_name: (string) Module name in generated API.
      dest_name: (string) Name in generated API.
      canonical_endpoint: (string) Preferred endpoint that should be used
        instead of the deprecated one.
    """
    self._deprecation_to_canonical[dest_module_name][dest_name] = (
        canonical_endpoint)

  def _import_submodules(self):
    """Add imports for all destination modules in self._module_imports."""
    # Import all required modules in their parent modules.
    # For e.g. if we import 'foo.bar.Value'. Then, we also
    # import 'bar' in 'foo'.
    imported_modules = set(self._module_imports.keys())
    imported_modules = imported_modules.union(
        set(self._deprecated_module_imports.keys()))
    for module in imported_modules:
      if not module:
        continue
      module_split = module.split('.')
      parent_module = ''  # we import submodules in their parent_module

      for submodule_index in range(len(module_split)):
        if submodule_index > 0:
          submodule = module_split[submodule_index-1]
          parent_module += '.' + submodule if parent_module else submodule
        import_from = self._output_package
        if submodule_index > 0:
          import_from += '.' + '.'.join(module_split[:submodule_index])
        self.add_import(
            -1, parent_module, import_from,
            module_split[submodule_index], module_split[submodule_index])

  def build(self):
    """Get a map from destination module to __init__.py code for that module.

    Returns:
      A dictionary where
        key: (string) destination module (for e.g. tf or tf.consts).
        value: (string) text that should be in __init__.py files for
          corresponding modules.
    """
    self._import_submodules()
    module_text_map = {}
    footer_text_map = {}
    for dest_module, dest_name_to_imports in self._module_imports.items():
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

    for dest_module, deprecated_name_to_canonical_name in (
        self._deprecation_to_canonical.items()):
      name_map_str = '\n'.join(
          '    "%s": "%s",' % (d, c)
          for d, c in deprecated_name_to_canonical_name.items())
      footer_text_map[dest_module] = _DEPRECATION_FOOTER % (
          name_map_str, dest_module)

    return module_text_map, footer_text_map


def _get_name_and_module(full_name):
  """Split full_name into module and short name.

  Args:
    full_name: Full name of symbol that includes module.

  Returns:
    Full module name and short symbol name.
  """
  name_segments = full_name.split('.')
  return '.'.join(name_segments[:-1]), name_segments[-1]


def _join_modules(module1, module2):
  """Concatenate 2 module components.

  Args:
    module1: First module to join.
    module2: Second module to join.

  Returns:
    Given two modules aaa.bbb and ccc.ddd, returns a joined
    module aaa.bbb.ccc.ddd.
  """
  if not module1:
    return module2
  if not module2:
    return module1
  return '%s.%s' % (module1, module2)


def add_imports_for_symbol(
    module_code_builder,
    symbol,
    source_module_name,
    source_name,
    api_name,
    api_version,
    output_module_prefix='',
    decorators=None):
  """Add imports for the given symbol to `module_code_builder`.

  Args:
    module_code_builder: `_ModuleInitCodeBuilder` instance.
    symbol: A symbol.
    source_module_name: Module that we can import the symbol from.
    source_name: Name we can import the symbol with.
    api_name: API name. Currently, must be either `tensorflow` or `estimator`.
    api_version: API version.
    output_module_prefix: Prefix to prepend to destination module.
    decorators: Tuple of symbol's decorators.
  """
  names_attr_v2 = API_ATTRS[api_name].names
  constants_attr_v2 = API_ATTRS[api_name].constants
  if api_version == 1:
    names_attr = API_ATTRS_V1[api_name].names
    constants_attr = API_ATTRS_V1[api_name].constants
  else:
    names_attr = names_attr_v2
    constants_attr = constants_attr_v2

  # If symbol is _tf_api_constants attribute, then add the constants.
  if source_name == constants_attr:
    for exports, name in symbol:
      for export in exports:
        dest_module, dest_name = _get_name_and_module(export)
        dest_module = _join_modules(output_module_prefix, dest_module)
        module_code_builder.add_import(
            -1, dest_module, source_module_name, name, dest_name)

  # If symbol has _tf_api_names attribute, then add import for it.
  if (hasattr(symbol, '__dict__') and names_attr in symbol.__dict__):
    # Get a list of all V2 names if we generate V1 API to check for
    # deprecations.
    exports_v2 = []
    if api_version == 1 and hasattr(symbol, names_attr_v2):
      exports_v2 = getattr(symbol, names_attr_v2)
    canonical_endpoint = None

    # Generate import statements for symbols.
    for export in getattr(symbol, names_attr):  # pylint: disable=protected-access
      dest_module, dest_name = _get_name_and_module(export)
      dest_module = _join_modules(output_module_prefix, dest_module)
      module_code_builder.add_import(
          id(symbol), dest_module, source_module_name, source_name, dest_name)
      # Export is deprecated if it is not in 2.0.
      if (export not in exports_v2 and
          not dest_module.startswith(_COMPAT_MODULE_PREFIX) and
          not has_deprecation_decorator(symbol, decorators)):
        if not canonical_endpoint:
          canonical_endpoint = tf_export.get_canonical_name_for_symbol(
              symbol, api_name, True)
        module_code_builder.add_deprecated_endpoint(
            dest_module, dest_name, canonical_endpoint)


def get_api_init_text(packages,
                      output_package,
                      api_name,
                      api_version,
                      compat_api_versions=None):
  """Get a map from destination module to __init__.py code for that module.

  Args:
    packages: Base python packages containing python with target tf_export
      decorators.
    output_package: Base output python package where generated API will be
      added.
    api_name: API you want to generate (e.g. `tensorflow` or `estimator`).
    api_version: API version you want to generate (1 or 2).
    compat_api_versions: Additional API versions to generate under compat/
      directory.

  Returns:
    A dictionary where
      key: (string) destination module (for e.g. tf or tf.consts).
      value: (string) text that should be in __init__.py files for
        corresponding modules.
  """
  if compat_api_versions is None:
    compat_api_versions = []
  module_code_builder = _ModuleInitCodeBuilder(output_package)
  # Traverse over everything imported above. Specifically,
  # we want to traverse over TensorFlow Python modules.

  def in_packages(m):
    return any(package in m for package in packages)

  for module in list(sys.modules.values()):
    # Only look at tensorflow modules.
    if (not module or not hasattr(module, '__name__') or
        module.__name__ is None or not in_packages(module.__name__)):
      continue
    # Do not generate __init__.py files for contrib modules for now.
    if (('.contrib.' in module.__name__ or module.__name__.endswith('.contrib'))
        and '.lite' not in module.__name__):
      continue

    for module_contents_name in dir(module):
      if (module.__name__ + '.' + module_contents_name
          in _SYMBOLS_TO_SKIP_EXPLICITLY):
        continue
      attr = getattr(module, module_contents_name)
      decorators, attr = tf_decorator.unwrap(attr)

      add_imports_for_symbol(
          module_code_builder, attr, module.__name__, module_contents_name,
          api_name, api_version,
          decorators=decorators)
      for compat_api_version in compat_api_versions:
        add_imports_for_symbol(
            module_code_builder, attr, module.__name__, module_contents_name,
            api_name, compat_api_version,
            _COMPAT_MODULE_TEMPLATE % compat_api_version,
            decorators=decorators)

  return module_code_builder.build()


def get_module(dir_path, relative_to_dir):
  """Get module that corresponds to path relative to relative_to_dir.

  Args:
    dir_path: Path to directory.
    relative_to_dir: Get module relative to this directory.

  Returns:
    Name of module that corresponds to the given directory.
  """
  dir_path = dir_path[len(relative_to_dir):]
  # Convert path separators to '/' for easier parsing below.
  dir_path = dir_path.replace(os.sep, '/')
  return dir_path.replace('/', '.').strip('.')


def get_module_docstring(module_name, package, api_name):
  """Get docstring for the given module.

  This method looks for docstring in the following order:
  1. Checks if module has a docstring specified in doc_srcs.
  2. Checks if module has a docstring source module specified
     in doc_srcs. If it does, gets docstring from that module.
  3. Checks if module with module_name exists under base package.
     If it does, gets docstring from that module.
  4. Returns a default docstring.

  Args:
    module_name: module name relative to tensorflow
      (excluding 'tensorflow.' prefix) to get a docstring for.
    package: Base python package containing python with target tf_export
      decorators.
    api_name: API you want to generate (e.g. `tensorflow` or `estimator`).

  Returns:
    One-line docstring to describe the module.
  """
  # Get the same module doc strings for any version. That is, for module
  # 'compat.v1.foo' we can get docstring from module 'foo'.
  for version in _API_VERSIONS:
    compat_prefix = _COMPAT_MODULE_TEMPLATE % version
    if module_name.startswith(compat_prefix):
      module_name = module_name[len(compat_prefix):].strip('.')

  # Module under base package to get a docstring from.
  docstring_module_name = module_name

  doc_sources = doc_srcs.get_doc_sources(api_name)

  if module_name in doc_sources:
    docsrc = doc_sources[module_name]
    if docsrc.docstring:
      return docsrc.docstring
    if docsrc.docstring_module_name:
      docstring_module_name = docsrc.docstring_module_name

  docstring_module_name = package + '.' + docstring_module_name
  if (docstring_module_name in sys.modules and
      sys.modules[docstring_module_name].__doc__):
    return sys.modules[docstring_module_name].__doc__

  return 'Public API for tf.%s namespace.' % module_name


def create_api_files(output_files, packages, root_init_template, output_dir,
                     output_package, api_name, api_version,
                     compat_api_versions, compat_init_templates):
  """Creates __init__.py files for the Python API.

  Args:
    output_files: List of __init__.py file paths to create.
    packages: Base python packages containing python with target tf_export
      decorators.
    root_init_template: Template for top-level __init__.py file.
      "# API IMPORTS PLACEHOLDER" comment in the template file will be replaced
      with imports.
    output_dir: output API root directory.
    output_package: Base output package where generated API will be added.
    api_name: API you want to generate (e.g. `tensorflow` or `estimator`).
    api_version: API version to generate (`v1` or `v2`).
    compat_api_versions: Additional API versions to generate in compat/
      subdirectory.
    compat_init_templates: List of templates for top level compat init files
      in the same order as compat_api_versions.

  Raises:
    ValueError: if output_files list is missing a required file.
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

  module_text_map, deprecation_footer_map = get_api_init_text(
      packages, output_package, api_name,
      api_version, compat_api_versions)

  # Add imports to output files.
  missing_output_files = []
  # Root modules are "" and "compat.v*".
  root_module = ''
  compat_module_to_template = {
      _COMPAT_MODULE_TEMPLATE % v: t
      for v, t in zip(compat_api_versions, compat_init_templates)
  }

  for module, text in module_text_map.items():
    # Make sure genrule output file list is in sync with API exports.
    if module not in module_name_to_file_path:
      module_file_path = '"%s/__init__.py"' %  (
          module.replace('.', '/'))
      missing_output_files.append(module_file_path)
      continue

    contents = ''
    if module == root_module and root_init_template:
      # Read base init file for root module
      with open(root_init_template, 'r') as root_init_template_file:
        contents = root_init_template_file.read()
        contents = contents.replace('# API IMPORTS PLACEHOLDER', text)
    elif module in compat_module_to_template:
      # Read base init file for compat module
      with open(compat_module_to_template[module], 'r') as init_template_file:
        contents = init_template_file.read()
        contents = contents.replace('# API IMPORTS PLACEHOLDER', text)
    else:
      contents = (
          _GENERATED_FILE_HEADER % get_module_docstring(
              module, packages[0], api_name) + text + _GENERATED_FILE_FOOTER)
    if module in deprecation_footer_map:
      contents += deprecation_footer_map[module]
    with open(module_name_to_file_path[module], 'w') as fp:
      fp.write(contents)

  if missing_output_files:
    raise ValueError(
        """Missing outputs for genrule:\n%s. Be sure to add these targets to
tensorflow/python/tools/api/generator/api_init_files_v1.bzl and
tensorflow/python/tools/api/generator/api_init_files.bzl (tensorflow repo), or
tensorflow_estimator/python/estimator/api/api_gen.bzl (estimator repo)"""
        % ',\n'.join(sorted(missing_output_files)))


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      'outputs', metavar='O', type=str, nargs='+',
      help='If a single file is passed in, then we we assume it contains a '
      'semicolon-separated list of Python files that we expect this script to '
      'output. If multiple files are passed in, then we assume output files '
      'are listed directly as arguments.')
  parser.add_argument(
      '--packages',
      default=_DEFAULT_PACKAGE,
      type=str,
      help='Base packages that import modules containing the target tf_export '
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
  parser.add_argument(
      '--apiname', required=True, type=str,
      choices=API_ATTRS.keys(),
      help='The API you want to generate.')
  parser.add_argument(
      '--apiversion', default=2, type=int,
      choices=_API_VERSIONS,
      help='The API version you want to generate.')
  parser.add_argument(
      '--compat_apiversions', default=[], type=int, action='append',
      help='Additional versions to generate in compat/ subdirectory. '
           'If set to 0, then no additional version would be generated.')
  parser.add_argument(
      '--compat_init_templates', default=[], type=str, action='append',
      help='Templates for top-level __init__ files under compat modules. '
           'The list of init file templates must be in the same order as '
           'list of versions passed with compat_apiversions.')
  parser.add_argument(
      '--output_package', default='tensorflow', type=str,
      help='Root output package.')
  args = parser.parse_args()

  if len(args.outputs) == 1:
    # If we only get a single argument, then it must be a file containing
    # list of outputs.
    with open(args.outputs[0]) as output_list_file:
      outputs = [line.strip() for line in output_list_file.read().split(';')]
  else:
    outputs = args.outputs

  # Populate `sys.modules` with modules containing tf_export().
  packages = args.packages.split(',')
  for package in packages:
    importlib.import_module(package)
  create_api_files(outputs, packages, args.root_init_template, args.apidir,
                   args.output_package, args.apiname, args.apiversion,
                   args.compat_apiversions, args.compat_init_templates)


if __name__ == '__main__':
  main()
