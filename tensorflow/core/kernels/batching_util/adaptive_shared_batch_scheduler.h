/* Copyright 2017 The TensorFlow Authors. All Rights Reserved.

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

#ifndef TENSORFLOW_CORE_KERNELS_BATCHING_UTIL_ADAPTIVE_SHARED_BATCH_SCHEDULER_H_
#define TENSORFLOW_CORE_KERNELS_BATCHING_UTIL_ADAPTIVE_SHARED_BATCH_SCHEDULER_H_

#include <algorithm>
#include <functional>
#include <memory>
#include <random>
#include <unordered_map>
#include <vector>

#include "tensorflow/core/kernels/batching_util/batch_scheduler.h"
#include "tensorflow/core/kernels/batching_util/periodic_function.h"
#include "tensorflow/core/lib/core/errors.h"
#include "tensorflow/core/lib/core/status.h"
#include "tensorflow/core/lib/core/threadpool.h"
#include "tensorflow/core/platform/byte_order.h"
#include "tensorflow/core/platform/cpu_info.h"
#include "tensorflow/core/platform/env.h"
#include "tensorflow/core/platform/thread_annotations.h"
#include "tensorflow/core/platform/threadpool_interface.h"
#include "tensorflow/core/platform/types.h"

namespace tensorflow {
namespace serving {
namespace internal {
template <typename TaskType>
class ASBSBatch;

template <typename TaskType>
class ASBSQueue;
}  // namespace internal

// Shared batch scheduler designed to minimize latency. The scheduler keeps
// track of a number of queues (one per model or model version) which are
// continuously enqueuing requests. The scheduler groups the requests into
// batches which it periodically sends off for processing (see
// shared_batch_scheduler.h for more details). AdaptiveSharedBatchScheduler
// (ASBS) prioritizes batches primarily by age (i.e. the batch's oldest request)
// along with a configurable preference for scheduling larger batches first.
//
//
// ASBS tries to keep the system busy by maintaining an adjustable number of
// concurrently processed batches.  If a new batch is created, and the number of
// in flight batches is below the target, the next (i.e. oldest) batch is
// immediately scheduled.  Similarly, when a batch finishes processing, the
// target is rechecked, and another batch may be scheduled.  To avoid the need
// to carefully tune the target for workload, model type, platform, etc, it is
// dynamically adjusted in order to provide the lowest average latency.
//
// Some potential use cases:
// Hardware Accelerators (GPUs & TPUs) - If some phase of batch processing
//   involves serial processing by a device, from a latency perspective it is
//   desirable to keep the device evenly loaded, avoiding the need to wait for
//   the device to process prior batches.
// CPU utilization - If the batch processing is cpu dominated, you can reap
//   latency gains when underutilized by increasing the processing rate, but
//   back the rate off when the load increases to avoid overload.

template <typename TaskType>
class AdaptiveSharedBatchScheduler
    : public std::enable_shared_from_this<
          AdaptiveSharedBatchScheduler<TaskType>> {
 public:
  ~AdaptiveSharedBatchScheduler() {
    // Finish processing batches before destroying other class members.
    batch_thread_pool_.reset();
  }

  struct Options {
    // The name to use for the pool of batch threads.
    string thread_pool_name = {"batch_threads"};
    // Number of batch processing threads - the maximum value of
    // in_flight_batches_limit_.  It is recommended that this value be set by
    // running the system under load, observing the learned value for
    // in_flight_batches_limit_, and setting this maximum to ~ 2x the value.
    // Under low load, in_flight_batches_limit_ has no substantial effect on
    // latency and therefore undergoes a random walk.  Unreasonably large values
    // for num_batch_threads allows for large in_flight_batches_limit_, which
    // will harm latency for some time once load increases again.
    int64 num_batch_threads = port::MaxParallelism();
    // You can pass a ThreadPoolInterface directly rather than the above two
    // parameters.  If given, the above two parameers are ignored.  Ownership of
    // the threadpool is not transferred.
    thread::ThreadPoolInterface* thread_pool = nullptr;
    // Lower bound for in_flight_batches_limit_. As discussed above, can be used
    // to minimize the damage caused by the random walk under low load.
    int64 min_in_flight_batches_limit = 1;
    // Although batch selection is primarily based on age, this parameter
    // specifies a preference for larger batches.  A full batch will be
    // scheduled before an older, nearly empty batch as long as the age gap is
    // less than full_batch_scheduling_boost_micros.  The optimal value for this
    // parameter should be of order the batch processing latency, but must be
    // chosen carefully, as too large a value will harm tail latency.
    int64 full_batch_scheduling_boost_micros = 0;
    // The environment to use (typically only overridden by test code).
    Env* env = Env::Default();
    // Initial limit for number of batches being concurrently processed.
    // Non-integer values correspond to probabilistic limits - i.e. a value of
    // 3.2 results in an actual cap of 3 80% of the time, and 4 20% of the time.
    double initial_in_flight_batches_limit = 3;
    // Number of batches between adjustments of in_flight_batches_limit.  Larger
    // numbers will give less noisy latency measurements, but will be less
    // responsive to changes in workload.
    int64 batches_to_average_over = 1000;
  };

  // Ownership is shared between the caller of Create() and any queues created
  // via AddQueue().
  static Status Create(
      const Options& options,
      std::shared_ptr<AdaptiveSharedBatchScheduler<TaskType>>* scheduler);

  struct QueueOptions {
    // Maximum size of each batch.
    int max_batch_size = 1000;
    // Maximum number of enqueued (i.e. non-scheduled) batches.
    int max_enqueued_batches = 10;
    // Amount of time non-full batches must wait before becoming schedulable.
    // A non-zero value can improve performance by limiting the scheduling of
    // nearly empty batches.
    int64 batch_timeout_micros = 0;
    // If non nullptr, split_input_task_func should split input_task into
    // multiple tasks, the first of which has size first_size and the remaining
    // not exceeding max_size. This function may acquire ownership of input_task
    // and should return a status indicating if the split was successful. Upon
    // success, the caller can assume that all output_tasks will be scheduled.
    // Including this option allows the scheduler to pack batches better and
    // should usually improve overall throughput.
    std::function<Status(std::unique_ptr<TaskType>* input_task, int first_size,
                         int max_size,
                         std::vector<std::unique_ptr<TaskType>>* output_tasks)>
        split_input_task_func;
  };

  using BatchProcessor = std::function<void(std::unique_ptr<Batch<TaskType>>)>;

  // Adds queue (and its callback) to be managed by this scheduler.
  Status AddQueue(const QueueOptions& options,
                  BatchProcessor process_batch_callback,
                  std::unique_ptr<BatchScheduler<TaskType>>* queue);

  double in_flight_batches_limit() {
    mutex_lock l(mu_);
    return in_flight_batches_limit_;
  }

 private:
  // access to AddBatch, MaybeScheduleClosedBatches, RemoveQueue, GetEnv.
  friend class internal::ASBSQueue<TaskType>;

  explicit AdaptiveSharedBatchScheduler(const Options& options);

  // Tracks processing latency and adjusts in_flight_batches_limit to minimize.
  void CallbackWrapper(const internal::ASBSBatch<TaskType>* batch,
                       BatchProcessor callback, bool is_express);

  // Schedules batch if in_flight_batches_limit_ is not met.
  void MaybeScheduleNextBatch() TF_EXCLUSIVE_LOCKS_REQUIRED(mu_);

  // Schedules all closed batches in batches_ for which an idle thread is
  // available in batch_thread_pool_.
  // Batches scheduled this way are called express batches.
  // Express batches are not limited by in_flight_batches_limit_, and
  // their latencies will not affect in_flight_batches_limit_.
  void MaybeScheduleClosedBatches();

  void MaybeScheduleClosedBatchesLocked() TF_EXCLUSIVE_LOCKS_REQUIRED(mu_);

  // Notifies scheduler of non-empty batch which is eligible for processing.
  void AddBatch(const internal::ASBSBatch<TaskType>* batch);

  // Removes queue from scheduler.
  void RemoveQueue(const internal::ASBSQueue<TaskType>* queue);

  Env* GetEnv() const { return options_.env; }

  const Options options_;

  // Collection of batches added by AddBatch, ordered by age. Owned by scheduler
  // until they are released for processing.
  std::vector<const internal::ASBSBatch<TaskType>*> batches_ TF_GUARDED_BY(mu_);

  // Unowned queues and callbacks added by AddQueue.
  std::unordered_map<const internal::ASBSQueue<TaskType>*, BatchProcessor>
      queues_and_callbacks_ TF_GUARDED_BY(mu_);

  mutex mu_;

  // Responsible for running the batch processing callbacks.
  std::unique_ptr<thread::ThreadPool> batch_thread_pool_;

  // Limit on number of batches which can be concurrently processed.
  // Non-integer values correspond to probabilistic limits - i.e. a value of 3.2
  // results in an actual cap of 3 80% of the time, and 4 20% of the time.
  double in_flight_batches_limit_ TF_GUARDED_BY(mu_);

  // Number of regular batches currently being processed.
  int64 in_flight_batches_ TF_GUARDED_BY(mu_) = 0;
  // Number of express batches currently being processed.
  int64 in_flight_express_batches_ TF_GUARDED_BY(mu_) = 0;

  // RNG engine and distribution.
  std::default_random_engine rand_engine_;
  std::uniform_real_distribution<double> rand_double_;

  // Fields controlling the dynamic adjustment of in_flight_batches_limit_.
  // Number of batches since the last in_flight_batches_limit_ adjustment.
  int64 batch_count_ TF_GUARDED_BY(mu_) = 0;
  // Sum of processing latency for batches counted by batch_count_.
  int64 batch_latency_sum_ TF_GUARDED_BY(mu_) = 0;
  // Average batch latency for previous value of in_flight_batches_limit_.
  double last_avg_latency_ms_ TF_GUARDED_BY(mu_) = 0;
  // Did last_avg_latency_ms_ decrease from the previous last_avg_latency_ms_?
  bool last_latency_decreased_ TF_GUARDED_BY(mu_) = false;
  // Current direction (+-) to adjust in_flight_batches_limit_
  int step_direction_ TF_GUARDED_BY(mu_) = 1;
  // Max adjustment size (as a fraction of in_flight_batches_limit_).
  constexpr static double kMaxStepSizeMultiplier = 0.125;  // 1/8;
  // Min adjustment size (as a fraction of in_flight_batches_limit_).
  constexpr static double kMinStepSizeMultiplier = 0.0078125;  // 1/128
  // Current adjustment size (as a fraction of in_flight_batches_limit_).
  double step_size_multiplier_ TF_GUARDED_BY(mu_) = kMaxStepSizeMultiplier;

  TF_DISALLOW_COPY_AND_ASSIGN(AdaptiveSharedBatchScheduler);
};

//////////////////////////////////////////////////////////
// Implementation details follow. API users need not read.

namespace internal {
// Consolidates tasks into batches, passing them off to the
// AdaptiveSharedBatchScheduler for processing.
template <typename TaskType>
class ASBSQueue : public BatchScheduler<TaskType> {
 public:
  using QueueOptions =
      typename AdaptiveSharedBatchScheduler<TaskType>::QueueOptions;

  ASBSQueue(std::shared_ptr<AdaptiveSharedBatchScheduler<TaskType>> scheduler,
            const QueueOptions& options);

  ~ASBSQueue() override;

  // Adds task to current batch. Fails if the task size is larger than the batch
  // size or if the current batch is full and this queue's number of outstanding
  // batches is at its maximum.
  Status Schedule(std::unique_ptr<TaskType>* task) override;

  // Number of tasks waiting to be scheduled.
  size_t NumEnqueuedTasks() const override;

  // Number of size 1 tasks which could currently be scheduled without failing.
  size_t SchedulingCapacity() const override;

  // Notifies queue that a batch is about to be scheduled; the queue should not
  // place any more tasks in this batch.
  void ReleaseBatch(const ASBSBatch<TaskType>* batch);

  size_t max_task_size() const override { return options_.max_batch_size; }

 private:
  // Number of size 1 tasks which could currently be scheduled without failing.
  size_t SchedulingCapacityLocked() const TF_EXCLUSIVE_LOCKS_REQUIRED(mu_);

  std::shared_ptr<AdaptiveSharedBatchScheduler<TaskType>> scheduler_;
  const QueueOptions options_;
  // Owned by scheduler_.
  ASBSBatch<TaskType>* current_batch_ TF_GUARDED_BY(mu_) = nullptr;
  int64 num_enqueued_batches_ TF_GUARDED_BY(mu_) = 0;
  int64 num_enqueued_tasks_ TF_GUARDED_BY(mu_) = 0;
  mutable mutex mu_;
  TF_DISALLOW_COPY_AND_ASSIGN(ASBSQueue);
};

// Batch which remembers when and by whom it was created.
template <typename TaskType>
class ASBSBatch : public Batch<TaskType> {
 public:
  ASBSBatch(ASBSQueue<TaskType>* queue, int64 creation_time_micros,
            int64 batch_timeout_micros)
      : queue_(queue),
        creation_time_micros_(creation_time_micros),
        schedulable_time_micros_(creation_time_micros + batch_timeout_micros) {}

  ~ASBSBatch() override {}

  ASBSQueue<TaskType>* queue() const { return queue_; }

  int64 creation_time_micros() const { return creation_time_micros_; }

  int64 schedulable_time_micros() const { return schedulable_time_micros_; }

 private:
  ASBSQueue<TaskType>* queue_;
  const int64 creation_time_micros_;
  const int64 schedulable_time_micros_;
  TF_DISALLOW_COPY_AND_ASSIGN(ASBSBatch);
};
}  // namespace internal

// ---------------- AdaptiveSharedBatchScheduler ----------------

template <typename TaskType>
constexpr double AdaptiveSharedBatchScheduler<TaskType>::kMaxStepSizeMultiplier;

template <typename TaskType>
constexpr double AdaptiveSharedBatchScheduler<TaskType>::kMinStepSizeMultiplier;

template <typename TaskType>
Status AdaptiveSharedBatchScheduler<TaskType>::Create(
    const Options& options,
    std::shared_ptr<AdaptiveSharedBatchScheduler<TaskType>>* scheduler) {
  if (options.num_batch_threads < 1) {
    return errors::InvalidArgument("num_batch_threads must be positive; was ",
                                   options.num_batch_threads);
  }
  if (options.min_in_flight_batches_limit < 1) {
    return errors::InvalidArgument(
        "min_in_flight_batches_limit must be >= 1; was ",
        options.min_in_flight_batches_limit);
  }
  if (options.min_in_flight_batches_limit > options.num_batch_threads) {
    return errors::InvalidArgument(
        "min_in_flight_batches_limit (", options.min_in_flight_batches_limit,
        ") must be <= num_batch_threads (", options.num_batch_threads, ")");
  }
  if (options.full_batch_scheduling_boost_micros < 0) {
    return errors::InvalidArgument(
        "full_batch_scheduling_boost_micros can't be negative; was ",
        options.full_batch_scheduling_boost_micros);
  }
  if (options.initial_in_flight_batches_limit > options.num_batch_threads) {
    return errors::InvalidArgument(
        "initial_in_flight_batches_limit (",
        options.initial_in_flight_batches_limit,
        ") should not be larger than num_batch_threads (",
        options.num_batch_threads, ")");
  }
  if (options.initial_in_flight_batches_limit <
      options.min_in_flight_batches_limit) {
    return errors::InvalidArgument("initial_in_flight_batches_limit (",
                                   options.initial_in_flight_batches_limit,
                                   "must be >= min_in_flight_batches_limit (",
                                   options.min_in_flight_batches_limit, ")");
  }
  if (options.batches_to_average_over < 1) {
    return errors::InvalidArgument(
        "batches_to_average_over should be "
        "greater than or equal to 1; was ",
        options.batches_to_average_over);
  }
  scheduler->reset(new AdaptiveSharedBatchScheduler<TaskType>(options));
  return Status::OK();
}

template <typename TaskType>
AdaptiveSharedBatchScheduler<TaskType>::AdaptiveSharedBatchScheduler(
    const Options& options)
    : options_(options),
      in_flight_batches_limit_(options.initial_in_flight_batches_limit),
      rand_double_(0.0, 1.0) {
  std::random_device device;
  rand_engine_.seed(device());
  if (options.thread_pool == nullptr) {
    batch_thread_pool_.reset(new thread::ThreadPool(
        GetEnv(), options.thread_pool_name, options.num_batch_threads));
  } else {
    batch_thread_pool_.reset(new thread::ThreadPool(options.thread_pool));
  }
}

template <typename TaskType>
Status AdaptiveSharedBatchScheduler<TaskType>::AddQueue(
    const QueueOptions& options, BatchProcessor process_batch_callback,
    std::unique_ptr<BatchScheduler<TaskType>>* queue) {
  if (options.max_batch_size <= 0) {
    return errors::InvalidArgument("max_batch_size must be positive; was ",
                                   options.max_batch_size);
  }
  if (options.max_enqueued_batches <= 0) {
    return errors::InvalidArgument(
        "max_enqueued_batches must be positive; was ",
        options.max_enqueued_batches);
  }
  internal::ASBSQueue<TaskType>* asbs_queue_raw;
  queue->reset(asbs_queue_raw = new internal::ASBSQueue<TaskType>(
                   this->shared_from_this(), options));
  mutex_lock l(mu_);
  queues_and_callbacks_[asbs_queue_raw] = process_batch_callback;
  return Status::OK();
}

template <typename TaskType>
void AdaptiveSharedBatchScheduler<TaskType>::AddBatch(
    const internal::ASBSBatch<TaskType>* batch) {
  mutex_lock l(mu_);
  batches_.push_back(batch);
  int64 delay_micros = batch->schedulable_time_micros() - GetEnv()->NowMicros();
  if (delay_micros <= 0) {
    MaybeScheduleNextBatch();
    return;
  }
  // Try to schedule batch once it becomes schedulable. Although scheduler waits
  // for all batches to finish processing before allowing itself to be deleted,
  // MaybeScheduleNextBatch() is called in other places, and therefore it's
  // possible the scheduler could be deleted by the time this closure runs.
  // Grab a shared_ptr reference to prevent this from happening.
  GetEnv()->SchedClosureAfter(
      delay_micros, [this, lifetime_preserver = this->shared_from_this()] {
        mutex_lock l(mu_);
        MaybeScheduleNextBatch();
      });
}

template <typename TaskType>
void AdaptiveSharedBatchScheduler<TaskType>::RemoveQueue(
    const internal::ASBSQueue<TaskType>* queue) {
  mutex_lock l(mu_);
  queues_and_callbacks_.erase(queue);
}

template <typename TaskType>
void AdaptiveSharedBatchScheduler<TaskType>::MaybeScheduleNextBatch() {
  if (batches_.empty() || in_flight_batches_ >= in_flight_batches_limit_)
    return;
  // Non-integer limit handled probabilistically.
  if (in_flight_batches_limit_ - in_flight_batches_ < 1 &&
      rand_double_(rand_engine_) >
          in_flight_batches_limit_ - in_flight_batches_) {
    return;
  }
  auto best_it = batches_.end();
  double best_score = (std::numeric_limits<double>::max)();
  int64 now_micros = GetEnv()->NowMicros();
  for (auto it = batches_.begin(); it != batches_.end(); it++) {
    if ((*it)->schedulable_time_micros() > now_micros) continue;
    const double score =
        (*it)->creation_time_micros() -
        options_.full_batch_scheduling_boost_micros * (*it)->size() /
            static_cast<double>((*it)->queue()->max_task_size());
    if (best_it == batches_.end() || score < best_score) {
      best_score = score;
      best_it = it;
    }
  }
  // No schedulable batches.
  if (best_it == batches_.end()) return;
  const internal::ASBSBatch<TaskType>* batch = *best_it;
  batches_.erase(best_it);
  // Queue may destroy itself after ReleaseBatch is called.
  batch->queue()->ReleaseBatch(batch);
  batch_thread_pool_->Schedule(
      std::bind(&AdaptiveSharedBatchScheduler<TaskType>::CallbackWrapper, this,
                batch, queues_and_callbacks_[batch->queue()], false));
  in_flight_batches_++;
}

template <typename TaskType>
void AdaptiveSharedBatchScheduler<TaskType>::MaybeScheduleClosedBatches() {
  mutex_lock l(mu_);
  MaybeScheduleClosedBatchesLocked();
}

template <typename TaskType>
void AdaptiveSharedBatchScheduler<
    TaskType>::MaybeScheduleClosedBatchesLocked() {
  // Only schedule closed batches if we have spare capacity.
  int available_threads =
      static_cast<int>(options_.num_batch_threads - in_flight_batches_ -
                       in_flight_express_batches_);
  for (auto it = batches_.begin();
       it != batches_.end() && available_threads > 0;) {
    if ((*it)->IsClosed()) {
      const internal::ASBSBatch<TaskType>* batch = *it;
      it = batches_.erase(it);
      batch->queue()->ReleaseBatch(batch);
      batch_thread_pool_->Schedule(
          std::bind(&AdaptiveSharedBatchScheduler<TaskType>::CallbackWrapper,
                    this, batch, queues_and_callbacks_[batch->queue()], true));
      in_flight_express_batches_++;
      available_threads--;
    } else {
      ++it;
    }
  }
}

template <typename TaskType>
void AdaptiveSharedBatchScheduler<TaskType>::CallbackWrapper(
    const internal::ASBSBatch<TaskType>* batch,
    AdaptiveSharedBatchScheduler<TaskType>::BatchProcessor callback,
    bool is_express) {
  int64 start_time = batch->creation_time_micros();
  callback(std::unique_ptr<Batch<TaskType>>(
      const_cast<internal::ASBSBatch<TaskType>*>(batch)));
  int64 end_time = GetEnv()->NowMicros();
  mutex_lock l(mu_);
  if (is_express) {
    in_flight_express_batches_--;
    MaybeScheduleClosedBatchesLocked();
    return;
  }
  in_flight_batches_--;
  batch_count_++;
  batch_latency_sum_ += end_time - start_time;
  // Occasionally adjust in_flight_batches_limit_ to minimize average latency.
  // Although the optimal value may depend on the workload, the latency should
  // be a simple convex function of in_flight_batches_limit_, allowing us to
  // locate the global minimum relatively quickly.
  if (batch_count_ == options_.batches_to_average_over) {
    double current_avg_latency_ms = (batch_latency_sum_ / 1000.) / batch_count_;
    bool current_latency_decreased =
        current_avg_latency_ms < last_avg_latency_ms_;
    if (current_latency_decreased) {
      // If latency improvement was because we're moving in the correct
      // direction, increase step_size so that we can get to the minimum faster.
      // If latency improvement was due to backtracking from a previous failure,
      // decrease step_size in order to refine our location.
      step_size_multiplier_ *= (last_latency_decreased_ ? 2 : 0.5);
      step_size_multiplier_ =
          std::min(step_size_multiplier_, kMaxStepSizeMultiplier);
      step_size_multiplier_ =
          std::max(step_size_multiplier_, kMinStepSizeMultiplier);
    } else {
      // Return (nearly) to previous position and confirm that latency is better
      // there before decreasing step size.
      step_direction_ = -step_direction_;
    }
    in_flight_batches_limit_ +=
        step_direction_ * in_flight_batches_limit_ * step_size_multiplier_;
    in_flight_batches_limit_ =
        std::min(in_flight_batches_limit_,
                 static_cast<double>(options_.num_batch_threads));
    in_flight_batches_limit_ =
        std::max(in_flight_batches_limit_,
                 static_cast<double>(options_.min_in_flight_batches_limit));
    last_avg_latency_ms_ = current_avg_latency_ms;
    last_latency_decreased_ = current_latency_decreased;
    batch_count_ = 0;
    batch_latency_sum_ = 0;
  }
  MaybeScheduleNextBatch();
}

// ---------------- ASBSQueue ----------------

namespace internal {
template <typename TaskType>
ASBSQueue<TaskType>::ASBSQueue(
    std::shared_ptr<AdaptiveSharedBatchScheduler<TaskType>> scheduler,
    const QueueOptions& options)
    : scheduler_(scheduler), options_(options) {}

template <typename TaskType>
ASBSQueue<TaskType>::~ASBSQueue() {
  // Wait until last batch has been scheduled.
  const int kSleepMicros = 1000;
  for (;;) {
    {
      mutex_lock l(mu_);
      if (num_enqueued_batches_ == 0) {
        break;
      }
    }
    scheduler_->GetEnv()->SleepForMicroseconds(kSleepMicros);
  }
  scheduler_->RemoveQueue(this);
}

template <typename TaskType>
Status ASBSQueue<TaskType>::Schedule(std::unique_ptr<TaskType>* task) {
  size_t size = (*task)->size();
  if (options_.split_input_task_func == nullptr &&
      size > options_.max_batch_size) {
    return errors::InvalidArgument("Task size ", size,
                                   " is larger than maximum batch size ",
                                   options_.max_batch_size);
  }
  std::vector<std::unique_ptr<TaskType>> tasks_to_schedule;
  std::vector<ASBSBatch<TaskType>*> new_batches;
  bool closed_batch = false;
  {
    mutex_lock l(mu_);
    if (size > SchedulingCapacityLocked()) {
      return errors::Unavailable("The batch scheduling queue is full");
    }
    int remaining_batch_size =
        current_batch_ == nullptr
            ? options_.max_batch_size
            : options_.max_batch_size - current_batch_->size();
    if (options_.split_input_task_func == nullptr ||
        size <= remaining_batch_size) {
      // Either we don't allow task splitting or task fits within the current
      // batch.
      tasks_to_schedule.push_back(std::move(*task));
    } else {
      // Split task in order to completely fill the current batch.
      // Beyond this point Schedule should not fail, as the caller has been
      // promised that all of the split tasks will be scheduled.
      TF_RETURN_IF_ERROR(options_.split_input_task_func(
          task, remaining_batch_size, options_.max_batch_size,
          &tasks_to_schedule));
    }
    for (auto& task : tasks_to_schedule) {
      // Can't fit within current batch, close it off and try to create another.
      if (current_batch_ &&
          current_batch_->size() + task->size() > options_.max_batch_size) {
        current_batch_->Close();
        closed_batch = true;
        current_batch_ = nullptr;
      }
      if (!current_batch_) {
        num_enqueued_batches_++;
        current_batch_ =
            new ASBSBatch<TaskType>(this, scheduler_->GetEnv()->NowMicros(),
                                    options_.batch_timeout_micros);
        new_batches.push_back(current_batch_);
      }
      current_batch_->AddTask(std::move(task));
      num_enqueued_tasks_++;
      // If current_batch_ is now full, allow it to be processed immediately.
      if (current_batch_->size() == options_.max_batch_size) {
        current_batch_->Close();
        closed_batch = true;
        current_batch_ = nullptr;
      }
    }
  }
  // Scheduler functions must be called outside of lock, since they may call
  // ReleaseBatch.
  for (auto* batch : new_batches) {
    scheduler_->AddBatch(batch);
  }
  if (closed_batch) {
    scheduler_->MaybeScheduleClosedBatches();
  }
  return Status::OK();
}

template <typename TaskType>
void ASBSQueue<TaskType>::ReleaseBatch(const ASBSBatch<TaskType>* batch) {
  mutex_lock l(mu_);
  num_enqueued_batches_--;
  num_enqueued_tasks_ -= batch->num_tasks();
  if (batch == current_batch_) {
    current_batch_->Close();
    current_batch_ = nullptr;
  }
}

template <typename TaskType>
size_t ASBSQueue<TaskType>::NumEnqueuedTasks() const {
  mutex_lock l(mu_);
  return num_enqueued_tasks_;
}

template <typename TaskType>
size_t ASBSQueue<TaskType>::SchedulingCapacity() const {
  mutex_lock l(mu_);
  return SchedulingCapacityLocked();
}

template <typename TaskType>
size_t ASBSQueue<TaskType>::SchedulingCapacityLocked() const {
  const int current_batch_capacity =
      current_batch_ ? options_.max_batch_size - current_batch_->size() : 0;
  const int spare_batches =
      options_.max_enqueued_batches - num_enqueued_batches_;
  return spare_batches * options_.max_batch_size + current_batch_capacity;
}
}  // namespace internal
}  // namespace serving
}  // namespace tensorflow

#endif  // TENSORFLOW_CORE_KERNELS_BATCHING_UTIL_ADAPTIVE_SHARED_BATCH_SCHEDULER_H_
