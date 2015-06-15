#include <iostream>
#include <chrono>
#include <cerrno>
#include <thread>
#include <atomic>
#include <ostream>

#include "atlas.h"
#include "type_list.h"
#include "test_cases.h"

struct tv_nullptr {
  static auto tv() { return static_cast<struct timeval *>(nullptr); }
  static void result(result &result) {
    if (result.error && result.error_code == EFAULT)
      result.accept = true;
  }
};

struct tv_1s {
  static auto tv() {
    static struct timeval tv { 1, 0 };
    return &tv;
  }
  static void result(result &result) {
    if (!result.error)
      result.accept = true;
  }
};

static uint64_t id{0};

namespace atlas {
namespace test {
template <typename Tid, typename Exectime, typename Deadline>
struct submit_test {
  static result test(std::ostringstream &os) {
    Tid tid;
    auto err = atlas::submit(tid.tid(), ++id, Exectime::tv(), Deadline::tv());
    result test_result{errno, err != 0};

    os << "TID " << tid.tid() << ", exec time  " << Exectime::tv()
       << " and deadline " << Deadline::tv();
    return test_result;
  }
};
template <typename... Us> using submit = testcase<submit_test, Us...>;
}
}

int main() {
  using Tids =
      type_list<tid_thread, tid_self, tid_negative, tid_invalid, tid_init>;
  using Deadlines = type_list<tv_nullptr, tv_1s>;
  using Exectimes = Deadlines;
  using combination = combinator<Tids, Exectimes, Deadlines>;

  using testsuite = apply<atlas::test::submit, typename combination::type>;

  testsuite::invoke();
}
