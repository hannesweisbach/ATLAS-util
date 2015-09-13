/* benchmark the efficacy of scheduling polcicies and their heuristics.
 * Policy: race-to-idle
 *   heuristics: worst-fit, random-fit
 * Policy: consolidate-to-idle
 *   first-fit, best-fit
 *
 * Generate random load with utilization <= m, to be scheduled upon >= m
 * processors.  Use lttng to record a trace and evaluate the used CPUs.
 * For each task maintain 5 jobs submitted to the kernel.
 */

#include <chrono>
#include <vector>
#include <random>
#include <iostream>
#include <thread>
#include <algorithm>
#include <iterator>

#include <boost/program_options.hpp>

#include "atlas.h"
#include "common.h"

using namespace std::chrono;

struct work {
  nanoseconds execution_time;
  nanoseconds deadline;
  steady_clock::time_point abs_deadline;
};

struct task {
  std::vector<work> work_items;
  steady_clock::time_point base;
  size_t submitted = 0;
  size_t done = 0;

  void submit() {
#if 0
    std::cout << "Submitting work item " << submitted << " on " << gettid()
              << std::endl;
#endif
    auto &work_item = work_items.at(submitted);
    base += work_item.deadline;
    work_item.abs_deadline = base;
    atlas::np::submit(std::this_thread::get_id(),
                      reinterpret_cast<uint64_t>(&work_item),
                      work_item.execution_time, base);
    ++submitted;
  }
};

static void worker_fun(struct task &task) {
  //ignore_deadlines();
  std::cout << "Worker " << gettid() << " started." << std::endl;

  task.base = steady_clock::now() + 20ms;

  for (; task.done < task.work_items.size(); ++task.done) {
    const auto remaining_jobs = task.work_items.size() - task.submitted;
    const auto required = 5 - (task.submitted - task.done);
    const auto to_submit = std::min(remaining_jobs, required);
    for (auto submitted = static_cast<uint64_t>(0); submitted < to_submit;
         ++submitted) {
      task.submit();
    }
    uint64_t work_id;
    check_zero(atlas::next(work_id));
    work *work = reinterpret_cast<struct work *>(work_id);

    struct work *next_work;
    busy_for((work->execution_time / 2) - 100us);
    std::this_thread::sleep_for(100us);
    busy_for(work->execution_time / 2);
  }
}

static std::vector<task> generate_work(const unsigned m, const unsigned count,
                                       double utilization = 1.0) {
  std::mt19937_64 gen;
  std::poisson_distribution<int64_t> exectimes(
      duration_cast<nanoseconds>(10ms).count());
  std::vector<task> tasks;
  std::generate_n(std::back_inserter(tasks), m, [count, utilization, &exectimes,
                                                 &gen] {
    task t;
    std::generate_n(std::back_inserter(t.work_items), count, [utilization,
                                                              &exectimes,
                                                              &gen] {
      auto exectime = exectimes(gen);
      auto deadline = exectime > duration_cast<nanoseconds>(1ms).count()
                          ? static_cast<int64_t>(exectime * 1.025)
                          : exectime + duration_cast<nanoseconds>(25us).count();
      return work{nanoseconds(static_cast<int64_t>(exectime * utilization)),
                  nanoseconds(deadline)};
    });
    return t;
  });

  return tasks;
}

int main(int argc, char *argv[]) {
  namespace po = boost::program_options;
  po::options_description desc("Benchmark load balancing");
  // clang-format off
  desc.add_options()
    ("help", "produce help message")
    ("threads", po::value<unsigned>()->default_value(1),
     "Number of threads to use. (Default: 1)")
    ("jobs", po::value<unsigned>()->default_value(10),
     "Number of jobs per thread. (Default: 10)")
    ("utilization", po::value<double>()->default_value(1.0),
     "Mean utilization of each thread. (Default: 1.0)");
  // clang-format on

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  const auto threads = vm["threads"].as<unsigned>();
  const auto jobs = vm["jobs"].as<unsigned>();
  const auto utilization = vm["utilization"].as<double>();

  auto tasks = generate_work(threads, jobs, utilization);

  for (const auto &task : tasks) {
    for (const auto &job : task.work_items) {
      std::cout << "[" << job.execution_time.count() << ", "
                << job.deadline.count() << "] ";
    }
    std::cout << std::endl;
  }

  std::vector<std::unique_ptr<std::thread>> workers(threads);
  auto task = std::begin(tasks);
  for (auto &&worker : workers) {
    worker = std::make_unique<std::thread>(worker_fun, std::ref(*task++));
  }

  for (const auto &worker : workers)
    worker->join();
}

