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

"""Platform-specific code for checking the integrity of the TensorFlow build."""
from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import os


try:
  from tensorflow.python.platform import build_info
except ImportError:
  raise ImportError("Could not import tensorflow. Do not import tensorflow "
                    "from its source directory; change directory to outside "
                    "the TensorFlow source tree, and relaunch your Python "
                    "interpreter from there.")


def preload_check():
  """Raises an exception if the environment is not correctly configured.

  Raises:
    ImportError: If the check detects that the environment is not correctly
      configured, and attempting to load the TensorFlow runtime will fail.
  """
  if os.name == "nt":
    # Attempt to load any DLLs that the Python extension depends on before
    # we load the Python extension, so that we can raise an actionable error
    # message if they are not found.
    import ctypes  # pylint: disable=g-import-not-at-top
    if hasattr(build_info, "msvcp_dll_names"):
      missing = []
      for dll_name in build_info.msvcp_dll_names.split(","):
        try:
          ctypes.WinDLL(dll_name)
        except OSError:
          missing.append(dll_name)
      if missing:
        raise ImportError(
            "Could not find the DLL(s) %r. TensorFlow requires that these DLLs "
            "be installed in a directory that is named in your %%PATH%% "
            "environment variable. You may install these DLLs by downloading "
            '"Microsoft C++ Redistributable for Visual Studio 2015, 2017 and '
            '2019" for your platform from this URL: '
            "https://support.microsoft.com/help/2977003/the-latest-supported-visual-c-downloads"
            % " or ".join(missing))
  else:
    # TODO(mrry): Consider adding checks for the Linux and Mac OS X builds.
    pass
