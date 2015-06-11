#include <chrono>
#include <atomic>
#include <thread>
#include <tuple>
#include <iostream>
#include <iomanip>

#include <boost/program_options.hpp>

#include "atlas.h"
#include "common.h"

int main(int argc, char *argv[]) {
  using namespace std::chrono;
  std::thread consumer;
  auto tid = gettid();
  std::atomic_bool run{false};
  
  int pinned_to;
  size_t num;

  namespace po = boost::program_options;
  po::options_description desc("Test scheduling of overlapping tasks");
  desc.add_options()
    ("help", "produce help message")
    ("pin", po::value(&pinned_to)->default_value(-1) ,
      "Whether to pin the worker thread and if so to which CPU. "
      "(default: unpinned)")
    ("jobs", po::value(&num)->default_value(100),
      "Number of jobs to submit (default: 100)");

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << std::endl;
    return EXIT_FAILURE;
  }

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
