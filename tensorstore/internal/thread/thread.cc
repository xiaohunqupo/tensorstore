// Copyright 2022 The TensorStore Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#if defined(__has_include)
#if __has_include(<pthread.h>)
#define TENSORSTORE_INTERNAL_USE_PTHREAD
#include <pthread.h>
#endif
#endif

#include <thread>  // NOLINT
#include <type_traits>

namespace tensorstore {
namespace internal {

void TrySetCurrentThreadName(const char* name) {
  if (name == nullptr) return;
#if defined(TENSORSTORE_INTERNAL_USE_PTHREAD)
#if defined(__APPLE__)
  pthread_setname_np(name);
#else
  pthread_setname_np(pthread_self(), name);
#endif
#endif
  // TODO: Add windows via SetThreadDescription()
}

}  // namespace internal
}  // namespace tensorstore
