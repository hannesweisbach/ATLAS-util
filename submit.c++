#include <iostream>
#include <chrono>
#include <cerrno>
#include <thread>
#include <atomic>

#include <boost/program_options.hpp>

#include "atlas.h"
#include "common.h"

static bool submit_to_init() {
  using namespace std::chrono;
  const pid_t init{1};
  auto err = atlas::submit(init, 0, 1s, high_resolution_clock::now() + 2s);
  if (err) {
    std::cout << "Expected error when submitting job to init: " << std::endl;
    std::cout << "\t" << strerror(errno) << std::endl;
  } else {
    std::cout << "Submitting job to init succeeded errononeously" << std::endl;
  }
  return errno == EPERM;
}

static bool submit_to_self() {
  using namespace std::chrono;
  auto err = atlas::np::submit(std::this_thread::get_id(), 1, 1s,
                               high_resolution_clock::now() + 2s);
  if (err) {
    std::cout << "Error submitting job to self: " << std::endl;
    std::cout << "\t" << strerror(errno) << std::endl;
  } else {
    std::cout << "Submitting job to self succeeded" << std::endl;
  }

  return !err;
}

static bool submit_to_thread() {
  using namespace std::chrono;
  std::atomic_bool run{true};
  std::thread t([&run] {
    while (run)
      ;
  });

  auto err =
      atlas::np::submit(t.get_id(), 2, 1s, high_resolution_clock::now() + 2s);
  run = false;

  if (err) {
    std::cout << "Error submitting job to thread: " << std::endl;
    std::cout << "\t" << strerror(errno) << std::endl;
  } else {
    std::cout << "Submitting job to thread succeeded" << std::endl;
  }

  t.join();

  return !err;
}

static bool submit_to_nonexistent() {
  using namespace std::chrono;
  const pid_t nonexistent{65535};
  auto err =
      atlas::submit(nonexistent, 3, 1s, high_resolution_clock::now() + 2s);

  if (err) {
    std::cout << "Expected error when submitting job to non-existent TID: "
              << std::endl;
    std::cout << "\t" << strerror(errno) << std::endl;
  } else {
    std::cout << "Submitting job to non-existent TID succeeded" << std::endl;
  }

  return errno == ESRCH;
}

static bool submit_invalid_exec() {
  using namespace std::chrono;
  struct timeval tv;
  auto err = atlas::submit(gettid(), 4, nullptr, &tv);

  if (err) {
    std::cout << "Expected error submitting job with invalid struct timeval "
                 "execution time pointer: " << std::endl;
    std::cout << "\t" << strerror(errno) << std::endl;
  } else {
    std::cout << "Submitting job with invalid times succeeded erroneously"
              << std::endl;
  }

  return !err;
}

static bool submit_invalid_deadline() {
  using namespace std::chrono;
  struct timeval tv;
  auto err = atlas::submit(gettid(), 5, &tv, nullptr);

  if (err) {
    std::cout << "Expected error submitting job with invalid struct timeval "
                 "deadline pointer: " << std::endl;
    std::cout << "\t" << strerror(errno) << std::endl;
  } else {
    std::cout << "Submitting job with invalid times succeeded" << std::endl;
  }
  return !err;
}

int main(int argc, char *argv[]) {
  namespace po = boost::program_options;
  po::options_description desc("Interface tests for atlas::submit()");
  desc.add_options()
    ("help", "produce help message")
    ("init", "Try to submit job to init (different process)")
    ("self", "Try to submit job to self.")
    ("thread", "Try to submit job to thread of the same process.")
    ("nonexistent", "Try to submit job to non-existent PID.")
    ("invalid-exec", "Try to submit job with nullptr for exec time.")
    ("invalid-dead", "Try to submit job with nullptr for deadline.")
    ("all", "Run all test.");

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << std::endl;
    return EXIT_FAILURE;
  }

  if (vm.count("init") || vm.count("all"))
    submit_to_init();

  if (vm.count("self") || vm.count("all"))
    submit_to_self();

  if (vm.count("thread") || vm.count("all"))
    submit_to_thread();

  if (vm.count("nonexistent") || vm.count("all"))
    submit_to_nonexistent();

  if (vm.count("invalid-exec") || vm.count("all"))
    submit_invalid_exec();

  if (vm.count("invalid-dead") || vm.count("all"))
    submit_invalid_deadline();

}
