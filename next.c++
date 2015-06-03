#include <thread>
#include <iostream>
#include <chrono>
#include <atomic>
#include <future>

#include "atlas.h"
#include "common.h"

using namespace std::chrono;

static uint64_t id{1};

/* Test the next syscall.
 * This tests correct wakeup & scheduling when sleeping/blocking in next().
 * Wakups are caused by:
 * - Explicit signals
 * - SIGKILL when the process exits (implicit)
 * - When a new job arrives
 *
 * The correct behaviour needs to be tested, regardless of the current
 * scheduler of the blocking task (there was a regression).
 *
 * 1) When blocking in ATLAS and a signal was delivered, there was an infinite
 *    scheduler loop, because ATLAS there was no job for ATLAS to schedule.
 * 2) When blocking in Recover a new job submission was added to the ATLAS
 *    RB-tree and the job was woken up. When Recover scheduled, it had no job,
 *    resulting in a BUG() or infinite scheduling loop.
 */

static void atlas_load() {}

/* workload to ensure blocking in Recover */
static void recover_load() {
  /* block to ensure Recover as scheduler */
  std::this_thread::sleep_for(0.5s);
  /* On deadline miss, transfer to Recover because of blocking time */
  wait_for_deadline();
}

static void cfs_load() {
  /* go to CFS */
  wait_for_deadline();
  busy_for(100ms);
}

static void test_submit(std::thread::id worker) {
  auto now = high_resolution_clock::now();
  atlas::np::submit(worker, id++, 1s, now + 2s);
}

static void test_signal(std::thread::id worker, int sig) {
  std::stringstream ss;
  ss << worker;

  pthread_t tid;
  ss >> tid;
  pthread_kill(tid, sig);
}

template <typename Workload, typename Test>
static bool wakeup(Workload &&w, Test &&t) {
  std::atomic_bool sleeping{false};
  std::atomic_bool done{false};
  using namespace std::chrono;

  std::thread worker([ w = std::move(w), &sleeping, &done ] {
    atlas::next();
    w();
    sleeping = true;
    atlas::next();
    done = true;
  });

  auto now = high_resolution_clock::now();
  atlas::np::submit(worker.get_id(), id++, 1s, now + 2s);

  /* Wait for the worker to block in the second next() call */
  std::this_thread::sleep_for(2s);
  while (!sleeping)
    ;
  std::this_thread::sleep_for(100ms);

  t(worker.get_id());

  std::this_thread::sleep_for(2.5s);
  if (done) {
    worker.join();
  } else {
    worker.detach();
  }

  return done;
}

/* TODO: to test process exit when a thread blocks in next, a new child process
 * needs to be spawned. The signal test below subsumes the exit test, since
 * sleeping threads are woken up by delivering SIGKILL.
 */

int main() {
  wakeup(recover_load, test_submit);
  wakeup(atlas_load, test_submit);
  wakeup(cfs_load, test_submit);

  auto test_sig = [](auto &&w) { test_signal(w, SIGCONT); };
  wakeup(recover_load, test_sig);
  wakeup(atlas_load, test_sig);
  wakeup(cfs_load, test_sig);
}
