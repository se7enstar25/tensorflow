/* Copyright 2015 The TensorFlow Authors. All Rights Reserved.

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

#ifndef TENSORFLOW_PLATFORM_DEFAULT_MUTEX_H_
#define TENSORFLOW_PLATFORM_DEFAULT_MUTEX_H_

// IWYU pragma: private, include "third_party/tensorflow/core/platform/mutex.h"
// IWYU pragma: friend third_party/tensorflow/core/platform/mutex.h

#include <chrono>
#include <condition_variable>
#include <mutex>
#include "nsync_cv.h"
#include "nsync_mu.h"
#include "tensorflow/core/platform/thread_annotations.h"
namespace tensorflow {

#undef mutex_lock

enum LinkerInitialized { LINKER_INITIALIZED };

class condition_variable;

// Mimic std::mutex + C++17's shared_mutex, adding a LinkerInitialized
// constructor interface.  This type is as fast as mutex, but is also a shared
// lock.
class LOCKABLE mutex {
 public:
  mutex() { nsync::nsync_mu_init(&mu_); }
  // The default implementation of nsync_mutex is safe to use after the linker
  // initializations
  explicit mutex(LinkerInitialized x) {}

  void lock() EXCLUSIVE_LOCK_FUNCTION() { nsync::nsync_mu_lock(&mu_); }
  bool try_lock() EXCLUSIVE_TRYLOCK_FUNCTION(true) {
    return nsync::nsync_mu_trylock(&mu_) != 0;
  };
  void unlock() UNLOCK_FUNCTION() { nsync::nsync_mu_unlock(&mu_); }

  void lock_shared() SHARED_LOCK_FUNCTION() { nsync::nsync_mu_rlock(&mu_); }
  bool try_lock_shared() SHARED_TRYLOCK_FUNCTION(true) {
    return nsync::nsync_mu_rtrylock(&mu_) != 0;
  };
  void unlock_shared() UNLOCK_FUNCTION() { nsync::nsync_mu_runlock(&mu_); }

 private:
  friend class condition_variable;
  nsync::nsync_mu mu_;
};

// Mimic a subset of the std::unique_lock<tensorflow::mutex> functionality.
class SCOPED_LOCKABLE mutex_lock {
 public:
  typedef ::tensorflow::mutex mutex_type;

  explicit mutex_lock(mutex_type& mu) EXCLUSIVE_LOCK_FUNCTION(mu) : mu_(&mu) {
    mu_->lock();
  }

  mutex_lock(mutex_type& mu, std::try_to_lock_t) EXCLUSIVE_LOCK_FUNCTION(mu)
      : mu_(&mu) {
    if (!mu.try_lock()) {
      mu_ = nullptr;
    }
  }

  // Manually nulls out the source to prevent double-free.
  // (std::move does not null the source pointer by default.)
  explicit mutex_lock(mutex_lock&& ml) noexcept : mu_(ml.mu_) {
    ml.mu_ = nullptr;
  }
  ~mutex_lock() UNLOCK_FUNCTION() {
    if (mu_ != nullptr) {
      mu_->unlock();
    }
  }
  mutex_type* mutex() { return mu_; }

  operator bool() const { return mu_ != nullptr; }

 private:
  mutex_type* mu_;
};

// Catch bug where variable name is omitted, e.g. mutex_lock (mu);
#define mutex_lock(x) static_assert(0, "mutex_lock_decl_missing_var_name");

// Mimic a subset of the std::shared_lock<tensorflow::mutex> functionality.
// Name chosen to minimise conflicts with the tf_shared_lock macro, below.
class SCOPED_LOCKABLE tf_shared_lock {
 public:
  typedef ::tensorflow::mutex mutex_type;

  explicit tf_shared_lock(mutex_type& mu) SHARED_LOCK_FUNCTION(mu) : mu_(&mu) {
    mu_->lock_shared();
  }

  tf_shared_lock(mutex_type& mu, std::try_to_lock_t) SHARED_LOCK_FUNCTION(mu)
      : mu_(&mu) {
    if (!mu.try_lock_shared()) {
      mu_ = nullptr;
    }
  }

  // Manually nulls out the source to prevent double-free.
  // (std::move does not null the source pointer by default.)
  explicit tf_shared_lock(tf_shared_lock&& ml) noexcept : mu_(ml.mu_) {
    ml.mu_ = nullptr;
  }
  ~tf_shared_lock() UNLOCK_FUNCTION() {
    if (mu_ != nullptr) {
      mu_->unlock_shared();
    }
  }
  mutex_type* mutex() { return mu_; }

  operator bool() const { return mu_ != nullptr; }

 private:
  mutex_type* mu_;
};

// Catch bug where variable name is omitted, e.g. tf_shared_lock (mu);
#define tf_shared_lock(x) \
  static_assert(0, "tf_shared_lock_decl_missing_var_name");

// Mimic std::condition_variable.
class condition_variable {
 public:
  condition_variable() { nsync::nsync_cv_init(&cv_); }

  void wait(mutex_lock& lock) {
    nsync::nsync_cv_wait(&cv_, &lock.mutex()->mu_);
  }
  template <class Rep, class Period>
  std::cv_status wait_for(mutex_lock& lock,
                          std::chrono::duration<Rep, Period> dur) {
    int r = nsync::nsync_cv_wait_with_deadline(
        &cv_, &lock.mutex()->mu_, std::chrono::system_clock::now() + dur,
        nullptr);
    return r ? std::cv_status::timeout : std::cv_status::no_timeout;
  }
  void notify_one() { nsync::nsync_cv_signal(&cv_); }
  void notify_all() { nsync::nsync_cv_broadcast(&cv_); }

 private:
  friend ConditionResult WaitForMilliseconds(mutex_lock* mu,
                                             condition_variable* cv, int64 ms);
  nsync::nsync_cv cv_;
};

inline ConditionResult WaitForMilliseconds(mutex_lock* mu,
                                           condition_variable* cv, int64 ms) {
  std::cv_status s = cv->wait_for(*mu, std::chrono::milliseconds(ms));
  return (s == std::cv_status::timeout) ? kCond_Timeout : kCond_MaybeNotified;
}

}  // namespace tensorflow

#endif  // TENSORFLOW_PLATFORM_DEFAULT_MUTEX_H_
