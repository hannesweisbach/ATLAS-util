#include <chrono>
#include <atomic>
#include <thread>
#include <tuple>
#include <iostream>
#include <iomanip>

#include <getopt.h>

#include "atlas.h"
#include "common.h"

[[noreturn]] void usage(bool error = true);
void usage(bool error) {
  std::cout << program_invocation_name
            << " [--pin=<num>] | [--thread ] | [--samples=<num>] | [--help]"
            << std::endl << std::endl;
  std::cout << "  --help               Print this help." << std::endl;
  std::cout << "  --pin=[cpu]          Pin to a CPU (default is CPU 0)"
            << std::endl;
  std::cout << "  --samples <num>      Take <num> measurements. Default: 100."
            << std::endl;
  std::cout << "  --thread             Run atlas::next in extra thread."
            << std::endl;

  if (error)
    exit(EXIT_FAILURE);
  else
    exit(EXIT_SUCCESS);
}

static std::tuple<size_t, int> options(int argc, char *argv[]) {
  int pinned = -1;
  size_t samples = 100;
  
  while (1) {
    int option_index = 0;
    static struct option long_options[] = {
        {"pin", optional_argument, nullptr, 'p'},
        {"samples", required_argument, nullptr, 's'},
        {"help", no_argument, nullptr, 'h'},
    };

    int c = getopt_long(argc, argv, "p:s:h", long_options, &option_index);
    if (c == -1)
      break;

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
    case 's':
      if (sscanf(optarg, "%zu", &samples) != 1) {
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

  return std::make_tuple(samples, pinned);
}

int main(int argc, char *argv[]) {
  using namespace std::chrono;
  std::thread consumer;
  auto tid = gettid();
  std::atomic_bool run{false};
  
  int pinned_to;
  size_t num;

  std::tie(num, pinned_to) = options(argc, argv);

  consumer = std::thread([&tid, &run, pinned_to, num]() {
    if (pinned_to >= 0)
      set_affinity(static_cast<unsigned>(pinned_to));
    tid = gettid();
    run = true;
    for (size_t i = 0; i < num; ++i) {
      check_zero(atlas::next());
    }
  });

  while (!run)
    ;

  for (size_t i = 0; i < num; ++i) {
    check_zero(atlas::submit(tid, i, 50ms, 10s + i * 1s));
  }

  std::cout << num << " jobs submitted to worker thread" << std::endl;

  consumer.join();
}
