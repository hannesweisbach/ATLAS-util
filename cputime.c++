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

#include <getopt.h>

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
      struct timespec start;
      struct timespec end;

      check_zero(clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start), "Start time");

      wait_for_deadline();

      check_zero(clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end), "End time");

      struct timespec diff = end - start;
      auto ns = diff.tv_nsec + diff.tv_sec * 1'000'000'000;
      exec_times.push_back(ns);
    }
  }

  return exec_times;
}

[[noreturn]] void usage(bool error = true);
void usage(bool error) {
  int indent = static_cast<int>(strlen(program_invocation_name)) + 1;
  std::cout << program_invocation_name
            << " [--pin=<num>] | [--output <filename>] | "
            << "[--threads <num>] | " << std::endl;
  std::cout << std::setw(indent) << ""
            << "[--samples=<num>] | [--help]" << std::endl;
  std::cout << "  --help               Print this help." << std::endl;
  std::cout << "  --output <filename>  Write measurements to the named file."
            << std::endl;
  std::cout << "                       Default is 'cputime.log'" << std::endl;
  std::cout << "  --pin=[cpu]          Pin to a CPU (default is CPU 0)"
            << std::endl;
  std::cout << "  --samples <num>      Take <num> measurements. Default: 100."
            << std::endl;
  std::cout
      << "  --threads <num>      Run with up to <num>-1 background threads."
      << std::endl;
  std::cout << "                       Default: 11 (10 Threads)." << std::endl;

  if (error)
    exit(EXIT_FAILURE);
  else
    exit(EXIT_SUCCESS);
}

std::tuple<int, std::string, size_t, size_t> options(int argc, char *argv[]);
std::tuple<int, std::string, size_t, size_t> options(int argc, char *argv[]) {
  int pinned = -1;
  std::string fname("cputime.log");
  size_t samples = 100;
  size_t max_threads = 11;

  while (1) {
    int option_index = 0;
    static struct option long_options[] = {
        {"output", required_argument, nullptr, 'o'},
        {"pin", optional_argument, nullptr, 'p'},
        {"samples", required_argument, nullptr, 's'},
        {"threads", required_argument, nullptr, 't'},
        {"help", no_argument, nullptr, 'h'},
    };

    int c = getopt_long(argc, argv, "p::o:s:t:h", long_options, &option_index);
    if (c == -1)
      break;

    std::cout << char(c) << std::endl;
    switch (c) {
    case 'h':
      usage(false);
    case 'p':
      pinned = 0;

      if (optarg) {
        int cpu;
        if (sscanf(optarg, "%i", &cpu) == 1) {
          pinned = cpu;
        } else {
          std::cerr << "Argument of option '" << long_options[option_index].name
                    << "' invalid: " << optarg << std::endl;
          usage();
        }
      }
      break;
    case 'o':
      fname = optarg;
      break;
    case 's':
      if (sscanf(optarg, "%zu", &samples) != 1) {
        std::cerr << "Argument of option '" << long_options[option_index].name
                  << "' invalid: " << optarg << std::endl;
        usage();
      }
      break;
    case 't':
      if (sscanf(optarg, "%zu", &max_threads) != 1) {
        std::cerr << "Argument of option '" << long_options[option_index].name
                  << "' invalid: " << optarg << std::endl;
        usage();
      }
      break;
    case '?':
    default:
      usage();
    }
  }

  return std::make_tuple(pinned, fname, samples, max_threads);
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

  std::tie(pinned, fname, samples, max_threads) = options(argc, argv);

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
