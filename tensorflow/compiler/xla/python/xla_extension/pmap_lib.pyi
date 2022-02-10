# Copyright 2021 The TensorFlow Authors. All Rights Reserved.
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

import inspect
import numpy as np
from typing import Any, Callable, List, Optional, Sequence, Iterable, Tuple

_AvalDimSharding = Any
_MeshDimAssignment = Any

class NoSharding:
  def __init__(self) -> None: ...
  def __repr__(self) -> str: ...
  def __eq__(self, __other: Any) -> bool: ...

class Chunked:
  @property
  def chunks(self) -> Sequence[int]: ...
  def __init__(self, __chunks: Sequence[int]) -> None: ...
  def __repr__(self) -> str: ...
  def __eq__(self, __other: Any) -> bool: ...

class Unstacked:
  @property
  def size(self) -> int: ...
  def __init__(self, __sz: int) -> None: ...
  def __repr__(self) -> str: ...
  def __eq__(self, __other: Any) -> bool: ...

class ShardedAxis:
  @property
  def axis(self) -> int: ...
  def __init__(self, __axis: int) -> None: ...
  def __repr__(self) -> str: ...
  def __eq__(self, __other: ShardedAxis) -> bool: ...

class Replicated:
  @property
  def replicas(self) -> int: ...
  def __init__(self, __replicas: int) -> None: ...
  def __repr__(self) -> str: ...
  def __eq__(self, __other: Replicated) -> bool: ...

class ShardingSpec:
  def __init__(self,
               sharding: Iterable[_AvalDimSharding],
               mesh_mapping: Iterable[_MeshDimAssignment]) -> None: ...
  @property
  def sharding(self) -> Tuple[_AvalDimSharding, ...]: ...
  @property
  def mesh_mapping(self) -> Tuple[_MeshDimAssignment]: ...
  def __eq__(self, __other: ShardingSpec) -> bool: ...
  def __hash__(self) -> int: ...

class ShardedDeviceArrayBase:
  ...

class ShardedDeviceArray(ShardedDeviceArrayBase):
  def __init__(self,
               aval: Any,
               sharding_spec: ShardingSpec,
               device_buffers: List[Any],
               indices: Any,
               weak_type: bool) -> None: ...
  aval: Any
  indices: Any
  sharding_spec: ShardingSpec
  @property
  def device_buffers(self) -> Optional[List[Any]]: ...
  _npy_value: Optional[np.ndarray]
  _one_replica_buffer_indices: Optional[Any]

  @property
  def shape(self) -> Tuple[int]: ...
  @property
  def dtype(self) -> np.dtype: ...
  @property
  def size(self) -> int: ...
  @property
  def ndim(self) -> int: ...

  def delete(self) -> None: ...

  @staticmethod
  def make(aval: Any, sharding_spec: ShardingSpec, device_buffers: List[Any],
           indices: Any, weak_type: bool) -> ShardedDeviceArray: ...

class PmapFunction:
  def __call__(self, *args, **kwargs) -> Any: ...
  def __getstate__(self) -> Any: ...
  def __setstate__(self, Any): ...
  __signature__: inspect.Signature
  def _cache_size(self) -> int: ...

def pmap(__fun: Callable[..., Any],
         __cache_miss: Callable[..., Any],
         __static_argnums: Sequence[int],
         __shard_arg_fallback: Callable[..., Any]) -> PmapFunction: ...
