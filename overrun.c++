#include <thread>
#include <chrono>
#include <atomic>

#include <boost/program_options.hpp>

#include "atlas.h"
#include "common.h"

static std::atomic<size_t> id{0};

static void overload() {
  using namespace std::chrono;
  auto now = std::chrono::high_resolution_clock::now();
  check_zero(atlas::np::submit(std::this_thread::get_id(), id++, 1s, now + 1.5s));
  check_zero(atlas::np::submit(std::this_thread::get_id(), id++, 1s, now + 1.5s));

  check_zero(atlas::next());

  std::this_thread::sleep_for(0.5s);
  wait_for_deadline();

  check_zero(atlas::next());
}

/* A task with a missed-deadline job, get's ATLAS for future jobs, if the
 * overrun reaches into the next jobs exeuction time.
 */
static void overrun_cfs() {
  using namespace std::chrono;
  auto now = std::chrono::high_resolution_clock::now();
  /* two jobs, 1s inbetween */
  check_zero(atlas::np::submit(std::this_thread::get_id(), id++, 1s, now + 1.5s));
  check_zero(atlas::np::submit(std::this_thread::get_id(), id++, 1s, now + 3.5s));

  /* Scheduler must be ATLAS */
  check_zero(atlas::next());
  std::cout << "Scheduler: " << sched_getscheduler(0) << std::endl;

  /* miss first deadline, scheduler should be CFS */
  wait_for_deadline();
  std::cout << "Scheduler: " << sched_getscheduler(0) << std::endl;

  /* The task runs into its reservation for the second job - the task should be
   * scheduled by ATLAS again.
   */
  busy_for(1.5s);
  std::cout << "Scheduler: " << sched_getscheduler(0) << std::endl;
}

static void overrun_recover() {
  using namespace std::chrono;
  auto now = std::chrono::high_resolution_clock::now();
  /* two jobs, 1s inbetween */
  check_zero(atlas::np::submit(std::this_thread::get_id(), id++, 2s, now + 2.5s));
  check_zero(atlas::np::submit(std::this_thread::get_id(), id++, 2s, now + 5.5s));

  /* Scheduler must be ATLAS */
  check_zero(atlas::next());
  std::cout << "Scheduler: " << sched_getscheduler(0) << std::endl;

  /* accumulate blocking time to get in Recover */
  std::this_thread::sleep_for(1.5s);

  /* miss first deadline, scheduler should be Recover */
  wait_for_deadline();
  std::cout << "Scheduler: " << sched_getscheduler(0) << std::endl;

  /* The task runs into its reservation for the second job - the task should be
   * scheduled by ATLAS again.
   */
  busy_for(1.5s);
  std::cout << "Scheduler: " << sched_getscheduler(0) << std::endl;
}

int main(int argc, char *argv[]) {
  namespace po = boost::program_options;
  po::options_description desc("Overrun/overload tests");
  desc.add_options()
    ("help", "produce help message")
    ("all", "run all tests")
    ("overload", "run overload test")
    ("cfs", "run overrun-into-CFS test")
    ("recover", "run overrun-into-Recover test")
    ("combined", "run overrun-into-CFS/Recover combined test");

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << std::endl;
    return EXIT_FAILURE;
  }


  if(vm.count("overload") || vm.count("all")) {
    std::thread worker{overload};
    worker.join();
  }

  if (vm.count("cfs") || vm.count("all")) {
    std::thread worker{overrun_cfs};
    worker.join();
  }

  if (vm.count("recover") || vm.count("all")) {
    std::thread worker{overrun_recover};
    worker.join();
  }

  if (vm.count("combined")) {
    std::thread worker([] {
      overrun_cfs();
      overrun_recover();
    });
    worker.join();
  }
}
