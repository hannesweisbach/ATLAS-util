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
    atlas::next();

    auto start = cputime_clock::now();

    busy_for(0.5s);
    /* block */
    std::this_thread::sleep_for(0.3s);

    auto end = cputime_clock::now();

    auto ms = duration_cast<milliseconds>(end - start);
    std::cout << "Task " << gettid() << " executed for " << ms.count() << "ms"
              << std::endl;
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
    atlas::next();

    auto start = cputime_clock::now();

    busy_for(0.5s);
    /* block */
    std::this_thread::sleep_for(0.3s);

    auto end = cputime_clock::now();

    auto ms = duration_cast<milliseconds>(end - start);
    std::cout << "Task " << gettid() << " executed for " << ms.count() << "ms"
              << std::endl;
  });

  std::thread task2([]() {
    atlas::next();

    auto start = cputime_clock::now();
    
    busy_for(0.4s);
    
    auto end = cputime_clock::now();

    auto ms = duration_cast<milliseconds>(end - start);
    std::cout << "Task " << gettid() << " executed for " << ms.count() << "ms"
              << std::endl;
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

