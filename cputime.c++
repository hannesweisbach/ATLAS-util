#include <iostream>
#include <exception>
#include <vector>
#include <algorithm>
#include <iterator>
#include <fstream>
#include <memory>
#include <thread>
#include <iomanip>
#include <atomic>
#include <tuple>

#include <boost/program_options.hpp>

#include "atlas.h"
#include "common.h"

class background_load {
  std::vector<std::unique_ptr<std::thread>> pool;
  std::atomic_bool running;

public:
  background_load(size_t threads, int pinned) : pool(threads), running(true) {
    std::generate_n(std::begin(pool), threads, [this, pinned]() {
      return std::make_unique<std::thread>([this, pinned]() {
        if (pinned >= 0) {
          set_affinity(static_cast<unsigned>(pinned));
        }
        while (running)
          ;
      });
    });
  }
  ~background_load() {
    running = false;
    for (const auto &thread : pool)
      thread->join();
  }
  background_load(background_load &&) = delete;
  background_load &operator=(background_load &&) = delete;
};

template <class Rep, class Period, class Rep2, class Period2>
std::vector<int64_t> cpu_time(std::chrono::duration<Rep, Period> exec_time,
                              std::chrono::duration<Rep2, Period2> period,
                              size_t count, size_t background_threads,
                              int pinned_to) {
  std::vector<int64_t> exec_times;
  auto tid = gettid();
  if (pinned_to >= 0) {
    set_affinity(static_cast<unsigned>(pinned_to), tid);
  }

  exec_times.reserve(count);
  background_load threads(background_threads, pinned_to);

  for (size_t i = 0; i < count; ++i) {
    using namespace std::chrono;
    using namespace std::chrono_literals;

    auto begin = high_resolution_clock::now();
    auto deadline = begin + period;
    uint64_t id = background_threads * count * 2 + i;
    check_zero(atlas::submit(tid, id, exec_time, deadline), "Submit");

    atlas::next();
    {
      auto start = cputime_clock::now();

      wait_for_deadline();

      auto end = cputime_clock::now();

      auto ns = duration_cast<nanoseconds>(end - start);
      exec_times.push_back(ns.count());
    }
  }

  return exec_times;
}

int main(int argc, char *argv[]) {
  using namespace std::chrono;
  const auto exec_time = 33ms;
  const auto period = 2 * exec_time;
  std::vector<std::vector<int64_t>> times;

  int pinned;
  std::string fname;
  size_t samples;
  size_t max_threads;

  namespace po = boost::program_options;
  po::options_description desc(
      "Benchmark program measuring allocated CPU time.");
  desc.add_options()
    ("help", "produce help message")
    ("threads", po::value(&max_threads)->default_value(11),
      "Maximum number of background worker threads.")
    ("samples", po::value(&samples)->default_value(100),
      "Number of measurements for each number of background threads.")
    ("output", po::value(&fname)->default_value("cputime.log"),
      "Name of the file to write the results into.")
    ("pin", po::value(&pinned)->default_value(-1),
      "Whether to pin the ATLAS worker and if yes to which CPU.");

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << std::endl;
    return EXIT_FAILURE;
  }

  for (size_t workers = 0; workers < max_threads; ++workers) {
    std::cout << "Measuring " << samples << " samples with " << workers
              << " worker threads" << std::endl;
    times.push_back(cpu_time(exec_time, period, samples, workers, pinned));
  }

  size_t workers = 0;
  for (auto &&exec_times : times) {
    std::sort(std::begin(exec_times), std::end(exec_times));

    auto less_count = std::count_if(
        std::cbegin(exec_times), std::cend(exec_times),
        [requested = duration_cast<nanoseconds>(exec_time).count()](
            const auto &e_time) { return e_time < requested; });
    std::cout << less_count << " jobs got less than requested time for "
              << workers++ << " worker threads" << std::endl;
  }

  {
    std::ofstream cputime(fname);
    {
      /* header */
      cputime << "#";
      for (workers = 0; workers < times.size(); ++workers)
        cputime << std::setw(2) << workers << "_threads ";
    }
    cputime << std::endl;
    for (size_t i = 0; i < samples; ++i) {
      for (workers = 0; workers < times.size(); ++workers)
        cputime << std::setw(10) << times.at(workers).at(i) << " ";
      cputime << std::endl;
    }
  }
}
