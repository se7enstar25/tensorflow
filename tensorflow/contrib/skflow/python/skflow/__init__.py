"""Main Scikit Flow module."""
#  Copyright 2015-present Scikit Flow Authors. All Rights Reserved.
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

## Check existance of sklearn and it's version

try:
    import sklearn
except ImportError:
    raise ImportError("Please install sklearn (pip install sklearn) to use "
                      "skflow.")

if sklearn.__version__ < '0.16.0':
    raise ImportError("Your scikit-learn version needs to be at least 0.16. "
                      "Your current version is %s. " % sklearn.VERSION)

import numpy as np
import tensorflow as tf

from io import *
from estimators import *
import ops
import preprocessing
import models
from trainer import TensorFlowTrainer

