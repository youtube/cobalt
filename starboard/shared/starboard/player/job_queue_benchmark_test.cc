// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Micro benchmarks for JobQueue / JobThread, written as gtests so they can be
// run on devices through the existing test APKs.  Every measurement is printed
// as a single line with the format:
//
//   JQBENCH|<test>|<metric>|<value>|<unit>
//
// so that the results can be grepped out of the test output / logcat.

#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <fstream>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "starboard/common/gettid.h"
#include "starboard/common/log.h"
#include "starboard/common/time.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "starboard/shared/starboard/player/job_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

void Report(const char* test,
            const char* metric,
            double value,
            const char* unit) {
  printf("JQBENCH|%s|%s|%.3f|%s\n", test, metric, value, unit);
  fflush(stdout);
  SB_LOG(INFO) << "JQBENCH|" << test << "|" << metric << "|" << value << "|"
               << unit;
}

int64_t Percentile(std::vector<int64_t> values, double percentile) {
  if (values.empty()) {
    return 0;
  }
  std::sort(values.begin(), values.end());
  size_t index = static_cast<size_t>(percentile * (values.size() - 1) + 0.5);
  index = std::min(index, values.size() - 1);
  return values[index];
}

void ReportDistribution(const char* test,
                        const char* metric_prefix,
                        const std::vector<int64_t>& values,
                        const char* unit) {
  std::string prefix(metric_prefix);
  Report(test, (prefix + "_p50").c_str(), Percentile(values, 0.5), unit);
  Report(test, (prefix + "_p99").c_str(), Percentile(values, 0.99), unit);
  Report(test, (prefix + "_max").c_str(), Percentile(values, 1.0), unit);
}

class Waiter {
 public:
  void Wait() {
    std::unique_lock lock(mutex_);
    cv_.wait(lock, [this] { return signaled_; });
    signaled_ = false;
  }
  void Signal() {
    {
      std::lock_guard lock(mutex_);
      signaled_ = true;
    }
    cv_.notify_all();
  }

 private:
  std::mutex mutex_;
  std::condition_variable cv_;
  bool signaled_ = false;
};

struct ThreadCounters {
  int64_t voluntary_ctxt_switches = 0;
  int64_t nonvoluntary_ctxt_switches = 0;
};

ThreadCounters ReadThreadCounters(pid_t tid) {
  ThreadCounters counters;
  std::ifstream status("/proc/self/task/" + std::to_string(tid) + "/status");
  std::string line;
  while (std::getline(status, line)) {
    if (line.rfind("voluntary_ctxt_switches:", 0) == 0) {
      counters.voluntary_ctxt_switches =
          std::stoll(line.substr(sizeof("voluntary_ctxt_switches:")));
    } else if (line.rfind("nonvoluntary_ctxt_switches:", 0) == 0) {
      counters.nonvoluntary_ctxt_switches =
          std::stoll(line.substr(sizeof("nonvoluntary_ctxt_switches:")));
    }
  }
  return counters;
}

// Warm the CPU frequency governor up a little bit, the test devices usually
// have "interactive"/"schedutil" governors that start at the lowest frequency.
void WarmUp() {
  int64_t end = CurrentMonotonicTime() + 300'000;
  volatile int64_t sink = 0;
  while (CurrentMonotonicTime() < end) {
    for (int i = 0; i < 1000; ++i) {
      sink += i;
    }
  }
}

constexpr int kRepeats = 7;

// Every benchmark is run with and without JobQueue optimizations, the
// parameter is |enable_optimizations|.  The mode is part of the test name
// reported in the metrics, e.g. "SelfPostChain/Optimized".
class JobQueueBenchmark : public ::testing::TestWithParam<bool> {
 protected:
  JobQueueBenchmark() : scoped_optimizations_(GetParam()) {}

  bool enable_optimizations() const { return GetParam(); }

  std::string Name(const char* test) const {
    return std::string(test) +
           (enable_optimizations() ? "/Optimized" : "/Original");
  }

 private:
  // Sets the switch for the JobQueues and JobThreads created by the tests.
  JobQueue::ScopedOptimizationsForTesting scoped_optimizations_;
};

// Schedule a burst of immediate jobs on the current thread's queue and then run
// them all.  Measures the pure data structure + allocation costs of Schedule()
// and of the dispatch loop (no thread wake ups are involved).
TEST_P(JobQueueBenchmark, SameThreadScheduleThenRun) {
  constexpr int kJobs = 20000;
  WarmUp();

  std::vector<int64_t> schedule_ns, run_ns;
  for (int repeat = 0; repeat < kRepeats; ++repeat) {
    JobQueue job_queue;
    int counter = 0;
    int64_t t0 = CurrentMonotonicTime();
    for (int i = 0; i < kJobs; ++i) {
      job_queue.Schedule([&counter] { ++counter; });
    }
    int64_t t1 = CurrentMonotonicTime();
    job_queue.RunUntilIdle();
    int64_t t2 = CurrentMonotonicTime();
    ASSERT_EQ(counter, kJobs);
    schedule_ns.push_back((t1 - t0) * 1000 / kJobs);
    run_ns.push_back((t2 - t1) * 1000 / kJobs);
  }
  Report(Name("SameThreadScheduleThenRun").c_str(), "schedule_ns_per_job",
         Percentile(schedule_ns, 0.5), "ns");
  Report(Name("SameThreadScheduleThenRun").c_str(), "run_ns_per_job",
         Percentile(run_ns, 0.5), "ns");
}

// Same as above, but the callable is a std::function lvalue, which is by far
// the most common pattern in the player code (e.g. Schedule(output_cb_)).
TEST_P(JobQueueBenchmark, SameThreadStdFunctionScheduleThenRun) {
  constexpr int kJobs = 20000;
  WarmUp();

  std::vector<int64_t> schedule_ns, run_ns;
  for (int repeat = 0; repeat < kRepeats; ++repeat) {
    JobQueue job_queue;
    int counter = 0;
    std::function<void()> job = [&counter] { ++counter; };
    int64_t t0 = CurrentMonotonicTime();
    for (int i = 0; i < kJobs; ++i) {
      job_queue.Schedule(job);
    }
    int64_t t1 = CurrentMonotonicTime();
    job_queue.RunUntilIdle();
    int64_t t2 = CurrentMonotonicTime();
    ASSERT_EQ(counter, kJobs);
    schedule_ns.push_back((t1 - t0) * 1000 / kJobs);
    run_ns.push_back((t2 - t1) * 1000 / kJobs);
  }
  Report(Name("SameThreadStdFunction").c_str(), "schedule_ns_per_job",
         Percentile(schedule_ns, 0.5), "ns");
  Report(Name("SameThreadStdFunction").c_str(), "run_ns_per_job",
         Percentile(run_ns, 0.5), "ns");
}

// A job running on the JobThread re-posts itself, like ProcessAudioData() or
// the decoder output callback chains do.  Measures the full Schedule() ->
// dispatch cycle when the producer is the worker thread itself.
TEST_P(JobQueueBenchmark, SelfPostChain) {
  constexpr int kJobs = 20000;
  WarmUp();

  std::vector<int64_t> ns_per_job;
  for (int repeat = 0; repeat < kRepeats; ++repeat) {
    auto job_thread = JobThread::Create("jqbench");
    std::atomic<int> remaining{kJobs};
    Waiter done;
    std::function<void()> job = [&] {
      if (--remaining == 0) {
        done.Signal();
        return;
      }
      job_thread->Schedule(job);
    };
    int64_t t0 = CurrentMonotonicTime();
    job_thread->Schedule(job);
    done.Wait();
    int64_t t1 = CurrentMonotonicTime();
    job_thread->Stop();
    ns_per_job.push_back((t1 - t0) * 1000 / kJobs);
  }
  Report(Name("SelfPostChain").c_str(), "ns_per_job",
         Percentile(ns_per_job, 0.5), "ns");
}

// Two JobThreads posting to each other.  Measures the cross thread wake up
// latency, which is what a decoder thread -> player worker thread hand over
// costs.
TEST_P(JobQueueBenchmark, CrossThreadPingPong) {
  constexpr int kRoundTrips = 3000;
  WarmUp();

  std::vector<int64_t> us_per_round_trip;
  for (int repeat = 0; repeat < kRepeats; ++repeat) {
    auto thread_a = JobThread::Create("jqbench_a");
    auto thread_b = JobThread::Create("jqbench_b");
    std::atomic<int> remaining{kRoundTrips};
    Waiter done;
    std::function<void()> ping, pong;
    ping = [&] { thread_b->Schedule(pong); };
    pong = [&] {
      if (--remaining == 0) {
        done.Signal();
        return;
      }
      thread_a->Schedule(ping);
    };
    int64_t t0 = CurrentMonotonicTime();
    thread_a->Schedule(ping);
    done.Wait();
    int64_t t1 = CurrentMonotonicTime();
    thread_a->Stop();
    thread_b->Stop();
    us_per_round_trip.push_back((t1 - t0) / kRoundTrips);
  }
  Report(Name("CrossThreadPingPong").c_str(), "us_per_round_trip",
         Percentile(us_per_round_trip, 0.5), "us");
}

// Several producer threads flood one JobThread, like the main thread (writing
// samples), the MediaCodec threads (output/consumed callbacks) and the audio
// sink thread do.  Measures the throughput and the latency of Schedule() as
// observed by the producers.
TEST_P(JobQueueBenchmark, MultiProducerBurst) {
  constexpr int kProducers = 3;
  constexpr int kJobsPerProducer = 10000;
  WarmUp();

  std::vector<int64_t> total_us;
  std::vector<int64_t> schedule_latency_ns;
  for (int repeat = 0; repeat < kRepeats; ++repeat) {
    auto consumer = JobThread::Create("jqbench_c");
    std::atomic<int> remaining{kProducers * kJobsPerProducer};
    Waiter done;
    std::atomic<bool> go{false};
    std::vector<std::vector<int64_t>> latencies(kProducers);
    std::vector<std::thread> producers;
    for (int p = 0; p < kProducers; ++p) {
      producers.emplace_back([&, p] {
        latencies[p].reserve(kJobsPerProducer);
        while (!go.load(std::memory_order_acquire)) {
        }
        for (int i = 0; i < kJobsPerProducer; ++i) {
          int64_t start = CurrentMonotonicTime();
          consumer->Schedule([&remaining, &done] {
            if (--remaining == 0) {
              done.Signal();
            }
          });
          latencies[p].push_back((CurrentMonotonicTime() - start) * 1000);
        }
      });
    }
    usleep(10'000);
    int64_t t0 = CurrentMonotonicTime();
    go.store(true, std::memory_order_release);
    for (auto& producer : producers) {
      producer.join();
    }
    done.Wait();
    int64_t t1 = CurrentMonotonicTime();
    consumer->Stop();
    total_us.push_back(t1 - t0);
    if (repeat == kRepeats / 2) {
      for (auto& l : latencies) {
        schedule_latency_ns.insert(schedule_latency_ns.end(), l.begin(),
                                   l.end());
      }
    }
  }
  int64_t median_total_us = Percentile(total_us, 0.5);
  Report(Name("MultiProducerBurst").c_str(), "total_us", median_total_us, "us");
  Report(Name("MultiProducerBurst").c_str(), "jobs_per_second",
         kProducers * kJobsPerProducer * 1'000'000.0 / median_total_us,
         "jobs/s");
  ReportDistribution(Name("MultiProducerBurst").c_str(), "schedule_latency",
                     schedule_latency_ns, "ns");
}

// Schedule delayed jobs one at a time on an otherwise idle JobThread and
// measure how late they run compared to their requested time.
TEST_P(JobQueueBenchmark, DelayedJobLateness) {
  WarmUp();
  for (int64_t delay_usec : {1'000, 5'000, 20'000}) {
    const int iterations = delay_usec >= 20'000 ? 50 : 100;
    auto job_thread = JobThread::Create("jqbench_d");
    std::vector<int64_t> lateness_us;
    Waiter waiter;
    for (int i = 0; i < iterations; ++i) {
      int64_t expected = CurrentMonotonicTime() + delay_usec;
      job_thread->Schedule(
          [&] {
            lateness_us.push_back(CurrentMonotonicTime() - expected);
            waiter.Signal();
          },
          delay_usec);
      waiter.Wait();
    }
    job_thread->Stop();
    std::string test =
        "DelayedJobLateness_" + std::to_string(delay_usec / 1000) + "ms";
    ReportDistribution(Name(test.c_str()).c_str(), "lateness", lateness_us,
                       "us");
  }
}

// Same as above but while a periodic job (like the 200ms media info Update())
// and a few other delayed jobs are pending, which is the steady state of a
// player JobQueue.
TEST_P(JobQueueBenchmark, DelayedJobLatenessWithPendingJobs) {
  WarmUp();
  constexpr int64_t kDelayUsec = 5'000;
  constexpr int kIterations = 100;
  auto job_thread = JobThread::Create("jqbench_d");
  // A handful of far-away delayed jobs, like what the renderers keep pending.
  for (int i = 0; i < 8; ++i) {
    job_thread->Schedule([] {}, 60'000'000 + i * 1'000'000);
  }
  std::vector<int64_t> lateness_us;
  Waiter waiter;
  for (int i = 0; i < kIterations; ++i) {
    int64_t expected = CurrentMonotonicTime() + kDelayUsec;
    job_thread->Schedule(
        [&] {
          lateness_us.push_back(CurrentMonotonicTime() - expected);
          waiter.Signal();
        },
        kDelayUsec);
    waiter.Wait();
  }
  job_thread->Stop();
  ReportDistribution(Name("DelayedJobLatenessWithPendingJobs").c_str(),
                     "lateness", lateness_us, "us");
}

// RemoveJobByToken() with many pending delayed jobs in the queue, the removed
// job being the last one.
TEST_P(JobQueueBenchmark, RemoveJobByTokenWithPendingJobs) {
  constexpr int kPendingJobs = 1000;
  constexpr int kOps = 5000;
  WarmUp();

  JobQueue job_queue;
  for (int i = 0; i < kPendingJobs; ++i) {
    job_queue.Schedule([] {}, 600'000'000);  // 10 minutes.
  }
  std::vector<int64_t> ns_per_op;
  for (int repeat = 0; repeat < kRepeats; ++repeat) {
    int64_t t0 = CurrentMonotonicTime();
    for (int i = 0; i < kOps; ++i) {
      auto token = job_queue.Schedule([] {}, 1'200'000'000);  // 20 minutes.
      job_queue.RemoveJobByToken(&token);
    }
    int64_t t1 = CurrentMonotonicTime();
    ns_per_op.push_back((t1 - t0) * 1000 / kOps);
  }
  job_queue.StopSoon();
  Report(Name("RemoveJobByTokenWithPendingJobs").c_str(),
         "ns_per_schedule_and_remove", Percentile(ns_per_op, 0.5), "ns");
}

// JobThread creation and tear down cost, which is paid by SbPlayerCreate() /
// SbPlayerDestroy().
TEST_P(JobQueueBenchmark, JobThreadCreateAndStop) {
  constexpr int kIterations = 40;
  WarmUp();

  std::vector<int64_t> create_us, stop_us;
  for (int i = 0; i < kIterations; ++i) {
    int64_t t0 = CurrentMonotonicTime();
    auto job_thread = JobThread::Create("jqbench_cs");
    int64_t t1 = CurrentMonotonicTime();
    job_thread->Stop();
    int64_t t2 = CurrentMonotonicTime();
    create_us.push_back(t1 - t0);
    stop_us.push_back(t2 - t1);
  }
  ReportDistribution(Name("JobThreadCreateAndStop").c_str(), "create",
                     create_us, "us");
  ReportDistribution(Name("JobThreadCreateAndStop").c_str(), "stop", stop_us,
                     "us");
}

// Simulates the steady state job mix of a playing SbPlayer for a few seconds:
//  - a "video decoder" thread posting 2 jobs per frame at 60fps,
//  - an "audio decoder" thread posting 2 jobs per AAC frame (~47Hz),
//  - a "main" thread posting WriteSamples() like jobs at 100Hz,
//  - the worker itself re-posting a job for every decoder output and keeping a
//    5ms delayed job (ProcessAudioData) and a 200ms periodic job (Update)
//    pending.
// Measures the CPU time consumed by the worker thread and its context
// switches, i.e. the overhead the JobQueue adds to the player worker.
TEST_P(JobQueueBenchmark, SimulatedPlaybackSteadyState) {
  constexpr int64_t kDurationUsec = 8'000'000;
  WarmUp();

  auto worker = JobThread::Create("jqbench_w");
  std::atomic<bool> stop{false};
  std::atomic<int64_t> jobs_run{0};

  pid_t worker_tid = 0;
  int64_t worker_cpu_start = 0;
  worker->ScheduleAndWait([&] {
    worker_tid = gettid();
    worker_cpu_start = CurrentMonotonicThreadTime();
  });
  ThreadCounters counters_start = ReadThreadCounters(worker_tid);

  // Jobs posted by the worker to itself.
  JobQueue::JobToken process_audio_token = JobQueue::JobToken::kUnscheduled;
  std::function<void()> process_audio_data;
  process_audio_data = [&] {
    ++jobs_run;
    process_audio_token = JobQueue::JobToken::kUnscheduled;
    if (!stop) {
      process_audio_token = worker->Schedule(process_audio_data, 5'000);
    }
  };
  std::function<void()> on_decoder_output = [&] {
    ++jobs_run;
    // Like AudioRendererPcm::OnDecoderOutput().
    worker->RemoveJobByToken(&process_audio_token);
    if (!stop) {
      process_audio_token = worker->Schedule(process_audio_data);
    }
  };
  std::function<void()> on_consumed = [&] { ++jobs_run; };
  JobQueue::JobToken update_token = JobQueue::JobToken::kUnscheduled;
  std::function<void()> update;
  update = [&] {
    ++jobs_run;
    worker->RemoveJobByToken(&update_token);
    if (!stop) {
      update_token = worker->Schedule(update, 200'000);
    }
  };
  worker->Schedule([&] {
    update_token = worker->Schedule(update, 200'000);
    process_audio_token = worker->Schedule(process_audio_data, 5'000);
  });

  auto producer = [&](int64_t period_usec, int jobs_per_period) {
    int64_t next = CurrentMonotonicTime() + period_usec;
    while (!stop) {
      int64_t now = CurrentMonotonicTime();
      if (now < next) {
        usleep(next - now);
        continue;
      }
      next += period_usec;
      for (int i = 0; i < jobs_per_period; ++i) {
        worker->Schedule(i % 2 == 0 ? on_decoder_output : on_consumed);
      }
    }
  };
  std::thread video_thread(producer, 16'667, 2);
  std::thread audio_thread(producer, 21'333, 2);
  std::thread main_thread([&] {
    int64_t next = CurrentMonotonicTime() + 10'000;
    while (!stop) {
      int64_t now = CurrentMonotonicTime();
      if (now < next) {
        usleep(next - now);
        continue;
      }
      next += 10'000;
      worker->Schedule(
          [&jobs_run, payload = std::vector<int>(4)] { ++jobs_run; });
    }
  });

  int64_t start = CurrentMonotonicTime();
  usleep(kDurationUsec);
  stop = true;
  video_thread.join();
  audio_thread.join();
  main_thread.join();
  int64_t elapsed = CurrentMonotonicTime() - start;

  int64_t worker_cpu_end = 0;
  worker->ScheduleAndWait([&] {
    worker->RemoveJobByToken(&process_audio_token);
    worker->RemoveJobByToken(&update_token);
    worker_cpu_end = CurrentMonotonicThreadTime();
  });
  ThreadCounters counters_end = ReadThreadCounters(worker_tid);
  worker->Stop();

  const int64_t jobs = jobs_run.load();
  const int64_t cpu_us = worker_cpu_end - worker_cpu_start;
  Report(Name("SimulatedPlayback").c_str(), "elapsed_s", elapsed / 1e6, "s");
  Report(Name("SimulatedPlayback").c_str(), "jobs_per_second",
         jobs * 1e6 / elapsed, "jobs/s");
  Report(Name("SimulatedPlayback").c_str(), "worker_cpu_us_per_second",
         cpu_us * 1e6 / elapsed, "us/s");
  Report(Name("SimulatedPlayback").c_str(), "worker_cpu_us_per_job",
         static_cast<double>(cpu_us) / jobs, "us");
  Report(Name("SimulatedPlayback").c_str(),
         "worker_voluntary_ctxt_switches_per_second",
         (counters_end.voluntary_ctxt_switches -
          counters_start.voluntary_ctxt_switches) *
             1e6 / elapsed,
         "1/s");
  Report(Name("SimulatedPlayback").c_str(),
         "worker_nonvoluntary_ctxt_switches_per_second",
         (counters_end.nonvoluntary_ctxt_switches -
          counters_start.nonvoluntary_ctxt_switches) *
             1e6 / elapsed,
         "1/s");
}

INSTANTIATE_TEST_SUITE_P(Optimizations,
                         JobQueueBenchmark,
                         ::testing::Bool(),
                         [](const ::testing::TestParamInfo<bool>& info) {
                           return info.param ? "Enabled" : "Disabled";
                         });

}  // namespace
}  // namespace starboard
