/* Copyright 2021 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow/compiler/xla/python/lru_cache.h"

#include <random>

#include "tensorflow/compiler/xla/test.h"
#include "tensorflow/core/platform/logging.h"

namespace xla {
namespace {

TEST(LRUCache, Basics) {
  LRUCache<int, int> cache(3);
  EXPECT_EQ(3, cache.Capacity());
  EXPECT_EQ(0, cache.Size());
  EXPECT_EQ(0, cache.GetOrCreateIfAbsent(0, [](int) { return 0; }));
  EXPECT_EQ(1, cache.Size());
  EXPECT_EQ(1, cache.GetOrCreateIfAbsent(1, [](int) { return 1; }));
  EXPECT_EQ(2, cache.Size());
  EXPECT_EQ(2, cache.GetOrCreateIfAbsent(2, [](int) { return 2; }));
  EXPECT_EQ(3, cache.Size());
  EXPECT_EQ(0, cache.GetOrCreateIfAbsent(0, [](int) { return 3; }));
  EXPECT_EQ(3, cache.Size());
  EXPECT_EQ(4, cache.GetOrCreateIfAbsent(3, [](int) { return 4; }));
  EXPECT_EQ(3, cache.Size());
  EXPECT_EQ(2, cache.GetOrCreateIfAbsent(2, [](int) { return 5; }));
  EXPECT_EQ(3, cache.Size());
  EXPECT_EQ(6, cache.GetOrCreateIfAbsent(1, [](int) { return 6; }));
  EXPECT_EQ(3, cache.Size());
  cache.Clear();
  EXPECT_EQ(0, cache.Size());
  EXPECT_EQ(6, cache.GetOrCreateIfAbsent(1, [](int) { return 6; }));
  EXPECT_EQ(1, cache.Size());
}

TEST(LRUCache, RandomInsertions) {
  LRUCache<int, int> cache(7);
  std::random_device rng;
  std::uniform_int_distribution<int> dist(0, 100);

  for (int i = 0; i < 1000; ++i) {
    EXPECT_LE(cache.Size(), std::min(cache.Capacity(), i));
    int key = dist(rng);
    int k = -1;
    int v = cache.GetOrCreateIfAbsent(key, [&](int k_arg) {
      CHECK_EQ(k_arg, key);
      k = k_arg;
      return k_arg * 37;
    });
    EXPECT_TRUE(k == -1 || k == key);
    EXPECT_EQ(v, key * 37);
  }
}

}  // namespace
}  // namespace xla
