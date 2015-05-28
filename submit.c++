#include <iostream>
#include <chrono>
#include <cerrno>
#include <thread>
#include <atomic>

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
  auto err = atlas::np::submit(std::this_thread::get_id(), 0, 1s,
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
      atlas::np::submit(t.get_id(), 0, 1s, high_resolution_clock::now() + 2s);
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
      atlas::submit(nonexistent, 0, 1s, high_resolution_clock::now() + 2s);

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
  auto err = atlas::submit(gettid(), 0, nullptr, &tv);

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
  auto err = atlas::submit(gettid(), 0, nullptr, &tv);

  if (err) {
    std::cout << "Expected error submitting job with invalid struct timeval "
                 "deadline pointer: " << std::endl;
    std::cout << "\t" << strerror(errno) << std::endl;
  } else {
    std::cout << "Submitting job with invalid times succeeded" << std::endl;
  }
  return !err;
}

int main() {
  submit_to_init();
  submit_to_self();
  submit_to_thread();
  submit_to_nonexistent();
  submit_invalid_exec();
  submit_invalid_deadline();
}
