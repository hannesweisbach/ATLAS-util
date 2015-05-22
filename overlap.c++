#include <thread>
#include <iostream>
#include <chrono>

#include <boost/program_options.hpp>

#include "atlas.h"
#include "common.h"

/* test cases for when jobs overlap */

/*
 * Scheduling two overlapping jobs, for a single task:
 *        ________
 *        |  J1  |
 *        --------_____
 *             |  J2  |
 *             --------
 * Result:
 *      _______________
 *      |  J1  |  J2  |
 *      ---------------
 */
static void overlap_single_task() {
  std::thread task([] {
    using namespace std::chrono;
    auto tid = std::this_thread::get_id();
    auto now = high_resolution_clock::now();
    atlas::np::submit(tid, 1, 1s, now + 2.5s);
    atlas::np::submit(tid, 2, 1s, now + 3s);

    auto start = cputime_clock::now();
    atlas::next();
    wait_for_deadline();
    atlas::next();
    wait_for_deadline();
    auto end = cputime_clock::now();

    auto duration = duration_cast<milliseconds>(end - start);
    std::cout << " Task got " << duration.count() << "ms of CPU time of 2000ms"
              << std::endl;
  });

  task.join();
}

static void overlap_two_tasks() {
  using namespace std::chrono;

  std::thread task1([] {
    auto start = cputime_clock::now();
    atlas::next();
    wait_for_deadline();
    auto end = cputime_clock::now();

    auto duration = duration_cast<milliseconds>(end - start);
    std::cout << " Task 1 got " << duration.count()
              << "ms of CPU time of 1000ms" << std::endl;

  });
  std::thread task2([] {
    auto start = cputime_clock::now();
    atlas::next();
    wait_for_deadline();
    auto end = cputime_clock::now();

    auto duration = duration_cast<milliseconds>(end - start);
    std::cout << " Task 2 got " << duration.count()
              << "ms of CPU time of 1000ms" << std::endl;

  });

  auto now = high_resolution_clock::now();
  atlas::np::submit(task1.get_id(), 1, 1s, now + 2.5s);
  atlas::np::submit(task2.get_id(), 2, 1s, now + 3s);

  task1.join();
  task2.join();
}

int main(int argc, char *argv[]) {
  namespace po = boost::program_options;
  po::options_description desc("Test scheduling of overlapping tasks");
  desc.add_options()
    ("help", "produce help message")
    ("one", "Overlapping jobs belong to the same task.")
    ("two", "Overlapping jobs belong to two tasks.");

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << std::endl;
    return EXIT_FAILURE;
  }

  if (vm.count("one"))
    overlap_single_task();

  if (vm.count("two"))
    overlap_two_tasks();
}

