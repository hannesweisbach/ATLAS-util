/* Test behaviour if an ATLAS task blocks.
 *
 * Planned schedule:
 * ____________________________
 * |Job 1/task 1||Job 2/task 2|
 * ----------------------------
 *
 *  Real schedule:
 *  _______ _______
 *  |Job 1||Job 2  …
 *  ---------------
 *        ^
 *        Job 1 blocks
 */

#include <thread>
#include <chrono>
#include <iostream>

#include <boost/program_options.hpp>

#include "atlas.h"
#include "common.h"

/* An ATLAS task blocks and no other ATLAS tasks are in the system */
static void block_single_task_atlas() {
  using namespace std::chrono;

  std::thread task1([]() {
    const auto sleep_time = 300ms;
    const auto exec_time = 500ms;
    atlas::next();

    auto start = cputime_clock::now();

    busy_for(exec_time);
    /* block */
    std::this_thread::sleep_for(sleep_time);

    auto end = cputime_clock::now();

    auto ms = duration_cast<milliseconds>(end - start);
    std::cout << "Task " << gettid() << " executed for " << ms.count()
              << "ms of " << exec_time.count() << "ms" << std::endl;
  });

  auto now = high_resolution_clock::now();
  check_zero(atlas::np::submit(task1.get_id(), 1, 1s, now + 1200ms));
  task1.join();
}

/*
 * An ATLAS task blocks and the computation time should be handed to the next
 * ready ATLAS task.
 */
static void block_two_tasks_atlas() {
  using namespace std::chrono;

  std::thread task1([]() {
    const auto exec_time = 300ms;
    const auto sleep_time = 300ms;
    atlas::next();

    auto start = cputime_clock::now();

    busy_for(exec_time);
    /* block */
    std::this_thread::sleep_for(sleep_time);

    auto end = cputime_clock::now();

    auto ms = duration_cast<milliseconds>(end - start);
    std::cout << "Task " << gettid() << " executed for " << ms.count()
              << "ms of " << exec_time.count() << "ms" << std::endl;
  });

  std::thread task2([]() {
    const auto exec_time = 400ms;
    atlas::next();

    auto start = cputime_clock::now();
    auto wall_start = high_resolution_clock::now();
    
    busy_for<cputime_clock>(exec_time);
    
    auto end = cputime_clock::now();
    auto wall_end = high_resolution_clock::now();

    auto ms = duration_cast<milliseconds>(end - start);
    auto wall_ms = duration_cast<milliseconds>(wall_end - wall_start);

    std::cout << "Task " << gettid() << " executed for " << ms.count()
              << "ms of CPU time and " << wall_ms.count()
              << "ms of wall clock time for " << exec_time.count()
              << "ms of CPU-time" << std::endl;
  });

  auto now = high_resolution_clock::now();
  check_zero(atlas::np::submit(task1.get_id(), 1, 1s, now + 1200ms));
  check_zero(atlas::np::submit(task2.get_id(), 2, 1s, now + 2300ms));

  task1.join();
  task2.join();
}

int main(int argc, char *argv[]) {
  namespace po = boost::program_options;
  po::options_description desc("Test scheduling of blocking tasks");
  desc.add_options()
    ("help", "produce help message")
    ("one", "A single ATLAS task has a blocking job.")
    ("two", "From two ATLAS tasks one has a blocking job.")
    ("all", "Run all tests.");

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << std::endl;
    return EXIT_FAILURE;
  }

  if (vm.count("one") || vm.count("all")) {
    block_single_task_atlas();
  }

  if (vm.count("two") || vm.count("all")) {
    block_two_tasks_atlas();
  }
}

